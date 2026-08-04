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
#define _POSIX_C_SOURCE 200809L

#include "ignore.h"

#include "logger.h"
#include "utils/defs.h"
#include "utils/fs.h"
#include "utils/mem.h"
#include "utils/path.h"
#include "utils/stowignore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
get_stowignore_path(const char *dotfiles_dir, const char *pkg_name, char *out_path, size_t out_size)
{
    if (pkg_name && *pkg_name != '\0') {
        char pkg_dir[PATH_MAX * 2];
        join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);
        mkdir_p(pkg_dir, 0755);
        join_path(out_path, out_size, pkg_dir, ".stowignore");
    } else {
        join_path(out_path, out_size, dotfiles_dir, ".stowignore");
    }
}

static void print_ignore_file_lines(const char *path, const StringArray *global_patterns)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        char *trimmed = trim_whitespace(line);

        /* Strip inline or full-line comments */
        char *hash = strchr(trimmed, '#');
        if (hash) {
            *hash = '\0';
            trimmed = trim_whitespace(trimmed);
        }

        /* Skip blank lines / comment-only lines */
        if (*trimmed == '\0') {
            continue;
        }

        bool is_redundant = (bool)(global_patterns && str_array_contains(global_patterns, trimmed));

        if (is_redundant) {
            printf("  %s (redundant: covered by global)%s\n", trimmed, COLOR_RESET);
        } else {
            printf("  %s\n", trimmed);
        }
    }
    fclose(fp);
}

/* =========================================================================
 * Show Subsystem Operations
 * =========================================================================
 */

static void ignore_show_global(const char *dotfiles_dir)
{
    char global_path[PATH_MAX];
    get_stowignore_path(dotfiles_dir, NULL, global_path, sizeof(global_path));

    if (!file_exists(global_path)) {
        log_warn("No global '.stowignore' file found at repository root.");
        return;
    }

    printf("\n%s%s=== Global Ignore Rules [.stowignore] ===%s\n\n",
           COLOR_CYAN,
           COLOR_BOLD,
           COLOR_RESET);
    print_ignore_file_lines(global_path, NULL);
    printf("\n");
}

static void ignore_show_package(const char *dotfiles_dir, const char *pkg_name)
{
    char pkg_path[PATH_MAX];
    get_stowignore_path(dotfiles_dir, pkg_name, pkg_path, sizeof(pkg_path));

    if (!file_exists(pkg_path)) {
        log_warn("No '.stowignore' file found for package '%s'.", pkg_name);
        return;
    }

    printf("\n%s%s=== Ignore Rules [.stowignore] for Package '%s' ===%s\n\n",
           COLOR_CYAN,
           COLOR_BOLD,
           pkg_name,
           COLOR_RESET);

    /* Load raw global ignore patterns to mark redundant declarations */
    StringArray global_patterns;
    str_array_init(&global_patterns);
    parse_stowignore_raw(dotfiles_dir, &global_patterns);

    print_ignore_file_lines(pkg_path, &global_patterns);
    printf("\n");

    str_array_free(&global_patterns);
}

static void ignore_show_all(const char *dotfiles_dir)
{
    ignore_show_global(dotfiles_dir);

    StringArray packages;
    str_array_init(&packages);
    get_all_packages(dotfiles_dir, &packages);

    for (size_t i = 0; i < packages.count; i++) {
        char pkg_path[PATH_MAX];
        get_stowignore_path(dotfiles_dir, packages.items[i], pkg_path, sizeof(pkg_path));
        if (file_exists(pkg_path)) {
            ignore_show_package(dotfiles_dir, packages.items[i]);
        }
    }

    str_array_free(&packages);
}

/* =========================================================================
 * Single Target Init / Clear
 * ========================================================================= */

static void ignore_init_single(const char *dotfiles_dir, const char *pkg_name)
{
    char path[PATH_MAX * 4];
    get_stowignore_path(dotfiles_dir, pkg_name, path, sizeof(path));

    if (file_exists(path)) {
        if (pkg_name && *pkg_name != '\0') {
            log_warn("'.stowignore' already exists for package '%s'.", pkg_name);
        } else {
            log_warn("'.stowignore' already exists at repository root.");
        }
        return;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        log_error("Failed to create '.stowignore' at: %s", path);
        return;
    }

    if (pkg_name && *pkg_name != '\0') {
        fprintf(fp, "# .stowignore for package '%s'\n", pkg_name);
    } else {
        fprintf(fp, "# Global .stowignore for dotfiles repository\n");
    }

    FILE *tmpl = open_resource_file("stowignore.template");
    if (tmpl) {
        char line[512];
        while (fgets(line, sizeof(line), tmpl)) {
            fputs(line, fp);
        }
        fclose(tmpl);
    } else {
        fprintf(fp,
                "# Syntax matches standard glob/gitignore pattern rules\n\n"
                "# Build & Runtime Cache Artifacts\n"
                "*.zwc\n"
                "*.pyc\n"
                "*.stow_backup_*\n\n"
                "# OS & Editor Metadata\n"
                ".DS_Store\n"
                "Thumbs.db\n"
                ".idea/\n"
                ".vscode/\n");
    }

    fclose(fp);

    if (pkg_name && *pkg_name != '\0') {
        log_success("Initialized '.stowignore' for package '%s'.", pkg_name);
    } else {
        log_success("Initialized global '.stowignore' at repository root.");
    }
}

static void ignore_clear_single(const char *dotfiles_dir, const char *pkg_name)
{
    char path[PATH_MAX * 4];
    get_stowignore_path(dotfiles_dir, pkg_name, path, sizeof(path));

    if (!file_exists(path)) {
        if (pkg_name && *pkg_name != '\0') {
            log_warn("No '.stowignore' file found for package '%s'.", pkg_name);
        } else {
            log_warn("No global '.stowignore' file found at repository root.");
        }
        return;
    }

    if (remove(path) == 0) {
        if (pkg_name && *pkg_name != '\0') {
            log_success("Cleared '.stowignore' file for package '%s'.", pkg_name);
        } else {
            log_success("Cleared global '.stowignore' file at repository root.");
        }
    } else {
        log_error("Failed to delete '.stowignore' file at: %s", path);
    }
}

/* =========================================================================
 * Batch Entrypoints (Zero Allocation Slices)
 * ========================================================================= */

void ignore_init(const char *dotfiles_dir, const char *const *pkgs, size_t count)
{
    if (!pkgs || count == 0) {
        ignore_init_single(dotfiles_dir, NULL);
        return;
    }
    for (size_t i = 0; i < count; i++) {
        ignore_init_single(dotfiles_dir, pkgs[i]);
    }
}

void ignore_clear(const char *dotfiles_dir, const char *const *pkgs, size_t count)
{
    if (!pkgs || count == 0) {
        ignore_clear_single(dotfiles_dir, NULL);
        return;
    }
    for (size_t i = 0; i < count; i++) {
        ignore_clear_single(dotfiles_dir, pkgs[i]);
    }
}

void ignore_show(const char *dotfiles_dir, const char *const *pkgs, size_t count)
{
    if (!pkgs || count == 0) {
        ignore_show_all(dotfiles_dir);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcmp(pkgs[i], "all") == 0 || strcmp(pkgs[i], "--all") == 0) {
            ignore_show_all(dotfiles_dir);
        } else {
            ignore_show_package(dotfiles_dir, pkgs[i]);
        }
    }
}

void ignore_add_patterns(const char *dotfiles_dir,
                         const char *pkg_name,
                         const char *const *patterns,
                         size_t count)
{
    if (!patterns || count == 0) {
        log_error("No patterns specified to add!");
        return;
    }

    int is_package = (pkg_name && *pkg_name != '\0');

    StringArray global_patterns;
    str_array_init(&global_patterns);
    if (is_package) {
        parse_stowignore_raw(dotfiles_dir, &global_patterns);
    }

    char path[PATH_MAX];
    get_stowignore_path(dotfiles_dir, pkg_name, path, sizeof(path));

    if (!file_exists(path)) {
        ignore_init_single(dotfiles_dir, pkg_name);
    }

    StringArray existing;
    str_array_init(&existing);

    FILE *rfp = fopen(path, "r");
    if (rfp) {
        char linebuf[512];
        while (fgets(linebuf, sizeof(linebuf), rfp)) {
            char *trimmed = trim_whitespace(linebuf);
            if (*trimmed != '\0') {
                str_array_append(&existing, trimmed);
            }
        }
        fclose(rfp);
    }

    FILE *afp = fopen(path, "a");
    if (!afp) {
        log_error("Failed to open '.stowignore' for writing: %s", path);
        str_array_free(&existing);
        str_array_free(&global_patterns);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        const char *pat = patterns[i];
        if (!pat || *pat == '\0') {
            continue;
        }

        if (is_package && str_array_contains(&global_patterns, pat)) {
            log_warn("Pattern '%s' is already in global '.stowignore' (skipping "
                     "duplicate).",
                     pat);
            continue;
        }

        if (str_array_contains(&existing, pat)) {
            log_warn("Pattern '%s' already exists in '.stowignore' (skipping "
                     "duplicate).",
                     pat);
            continue;
        }

        fprintf(afp, "%s\n", pat);
        str_array_append(&existing, pat);

        if (is_package) {
            log_success("Added pattern '%s' to '.stowignore' for package '%s'.", pat, pkg_name);
        } else {
            log_success("Added pattern '%s' to global '.stowignore'.", pat);
        }
    }

    fclose(afp);
    str_array_free(&existing);
    str_array_free(&global_patterns);
}

void ignore_remove_patterns(const char *dotfiles_dir,
                            const char *pkg_name,
                            const char *const *patterns,
                            size_t count)
{
    if (!patterns || count == 0) {
        log_error("No patterns specified to remove!");
        return;
    }

    char path[PATH_MAX * 4];
    get_stowignore_path(dotfiles_dir, pkg_name, path, sizeof(path));

    if (!file_exists(path)) {
        if (pkg_name && *pkg_name != '\0') {
            log_warn("No '.stowignore' file found for package '%s'.", pkg_name);
        } else {
            log_warn("No global '.stowignore' file found at repository root.");
        }
        return;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        log_error("Failed to open '.stowignore' for reading: %s", path);
        return;
    }

    StringArray lines;
    str_array_init(&lines);

    char linebuf[512];
    while (fgets(linebuf, sizeof(linebuf), fp)) {
        size_t len = strlen(linebuf);
        if (len > 0 && (linebuf[len - 1] == '\n' || linebuf[len - 1] == '\r')) {
            linebuf[len - 1] = '\0';
        }
        str_array_append(&lines, linebuf);
    }
    fclose(fp);

    bool file_modified = false;

    for (size_t p = 0; p < count; p++) {
        const char *pat = patterns[p];
        bool found = false;

        StringArray filtered;
        str_array_init(&filtered);

        for (size_t l = 0; l < lines.count; l++) {
            char *trimmed = trim_whitespace(lines.items[l]);
            if (strcmp(trimmed, pat) == 0) {
                found = true;
                file_modified = true;
            } else {
                str_array_append(&filtered, lines.items[l]);
            }
        }

        str_array_free(&lines);
        lines = filtered;

        if (found) {
            if (pkg_name && *pkg_name != '\0') {
                log_success(
                    "Removed pattern '%s' from '.stowignore' for package '%s'.", pat, pkg_name);
            } else {
                log_success("Removed pattern '%s' from global '.stowignore'.", pat);
            }
        } else {
            log_warn("Pattern '%s' was not found in '.stowignore'.", pat);
        }
    }

    if (file_modified) {
        FILE *wfp = fopen(path, "w");
        if (wfp) {
            for (size_t i = 0; i < lines.count; i++) {
                fprintf(wfp, "%s\n", lines.items[i]);
            }
            fclose(wfp);
        } else {
            log_error("Failed to write updated '.stowignore': %s", path);
        }
    }

    str_array_free(&lines);
}
