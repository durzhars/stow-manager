/*
 * Dotfiles Stow Manager (stow-manager)
 * Copyright (C) 2026 durzhars
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "scanner.h"
#include "registry.h"

#include "logger.h"
#include "manifest.h"
#include "utils/defs.h"
#include "utils/fs.h"
#include "utils/path.h"
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void parse_shebang_interpreter(const char *first_line, StringArray *shebangs)
{
    if (strncmp(first_line, "#!", 2) != 0) {
        return;
    }

    char *copy = strdup(first_line + 2);
    if (!copy) {
        return;
    }

    char *saveptr = NULL;
    char *token = strtok_r(copy, " \t\r\n", &saveptr);

    while (token) {
        char *trimmed = trim_whitespace(token);
        if (trimmed[0] != '\0' && trimmed[0] != '-') {
            char *base = strrchr(trimmed, '/');
            if (base) {
                trimmed = base + 1;
            }

            if (strcmp(trimmed, "env") != 0 && strcmp(trimmed, "exec") != 0) {
                if (strlen(trimmed) > 0 && !str_array_contains(shebangs, trimmed)) {
                    str_array_append(shebangs, trimmed);
                }
                break;
            }
        }
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    free(copy);
}

static bool contains_word_token(const char *line, const char *word)
{
    if (!line || !word || *word == '\0' || *line == '\0') {
        return false;
    }
    size_t wlen = strlen(word);

    const char *pos = line;
    while ((pos = strstr(pos, word)) != NULL) {
        const char *after = pos + wlen;
        int left_ok = (pos == line) || (!isalnum((unsigned char)*(pos - 1)) && *(pos - 1) != '_' &&
                                        *(pos - 1) != '-');
        int right_ok =
            (*after == '\0') || (!isalnum((unsigned char)*after) && *after != '_' && *after != '-');

        if (left_ok && right_ok) {
            return true;
        }
        pos += wlen;
    }
    return false;
}

static void process_single_file(const char *filepath,
                                const StringArray *candidate_tools,
                                const char *pkg_name,
                                StringArray *shebangs,
                                StringArray *invocations)
{
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        return;
    }

    char *linebuf = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    bool is_first_line = true;

    while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
        (void)linelen;
        if (is_first_line) {
            is_first_line = false;
            parse_shebang_interpreter(linebuf, shebangs);
        }

        if (candidate_tools) {
            for (size_t t = 0; t < candidate_tools->count; t++) {
                const char *tool = candidate_tools->items[t];
                if (strcmp(tool, pkg_name) == 0) {
                    continue;
                }
                if (!str_array_contains(invocations, tool)) {
                    if (contains_word_token(linebuf, tool)) {
                        str_array_append(invocations, tool);
                    }
                }
            }
        }
    }

    free(linebuf);
    fclose(fp);
}

typedef struct {
    const StringArray *candidate_tools;
    const char *pkg_name;
    StringArray *shebangs;
    StringArray *invocations;
} ScanFileState;

static void scan_file_cb(const char *file_path, const char *rel_path, void *user_data)
{
    (void)rel_path;
    if (file_exists(file_path) && !is_symlink(file_path)) {
        ScanFileState *st = (ScanFileState *)user_data;
        process_single_file(
            file_path, st->candidate_tools, st->pkg_name, st->shebangs, st->invocations);
    }
}

void scan_package(const char *dotfiles_dir, const char *pkg_name)
{
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!file_exists(pkg_dir)) {
        log_error("Package directory '%s' does not exist!", pkg_name);
        return;
    }

    log_info("Recursively scanning package content in '%s' for dependencies...", pkg_name);

    StringArray candidate_tools;
    str_array_init(&candidate_tools);

    // Collect candidate tools from dotfiles packages and registry
    get_all_packages(dotfiles_dir, &candidate_tools);
    registry_get_all_tools(dotfiles_dir, &candidate_tools);

    StringArray shebangs;
    StringArray invocations;
    str_array_init(&shebangs);
    str_array_init(&invocations);

    ScanFileState state = {&candidate_tools, pkg_name, &shebangs, &invocations};
    walk_dir_files(pkg_dir, "", scan_file_cb, &state);

    // If package name itself is a system tool (e.g. hyprland, zsh, tmux), ensure it is in required
    // shebangs
    if (is_executable_in_path(pkg_name) && !str_array_contains(&shebangs, pkg_name)) {
        str_array_append(&shebangs, pkg_name);
    }

    printf("  %sScan Results for package '%s':%s\n", COLOR_BOLD, pkg_name, COLOR_RESET);
    printf("    %sDetected Shebangs (Required):%s ", COLOR_BOLD, COLOR_RESET);
    if (shebangs.count > 0) {
        for (size_t i = 0; i < shebangs.count; i++) {
            printf("%s ", shebangs.items[i]);
        }
    } else {
        printf("none");
    }
    printf("\n");

    printf("    %sDetected Invocations (Optional):%s ", COLOR_BOLD, COLOR_RESET);
    if (invocations.count > 0) {
        for (size_t i = 0; i < invocations.count; i++) {
            printf("%s ", invocations.items[i]);
        }
    } else {
        printf("none");
    }
    printf("\n\n");

    char manifest_path[PATH_MAX * 4];
    join_path(manifest_path, sizeof(manifest_path), pkg_dir, ".stowdeps");
    if (!file_exists(manifest_path)) {
        log_info("Auto-generating '.stowdeps' manifest for '%s'...", pkg_name);
        PackageManifest manifest;
        manifest_init(&manifest, pkg_name);
        for (size_t i = 0; i < shebangs.count; i++) {
            str_array_append(&manifest.required, shebangs.items[i]);
        }
        for (size_t i = 0; i < invocations.count; i++) {
            str_array_append(&manifest.optional, invocations.items[i]);
        }
        manifest_save(&manifest, dotfiles_dir);
        log_success("Generated '%s'", manifest_path);
        manifest_free(&manifest);
    }

    str_array_free(&shebangs);
    str_array_free(&invocations);
    str_array_free(&candidate_tools);
}
