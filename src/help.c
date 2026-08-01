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

#include "help.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __has_include("help_text_plain.h")
#include "help_text_plain.h"
#else
static const char *EMBEDDED_HELP_TXT = "Dotfiles Stow Manager (stow-manager)\n\n"
                                       "Usage: stow-manager [options] <command> [arguments]\n\n"
                                       "Run make to compile full help menu or pass -h / --help.\n";
#endif

static void render_plain_line(const char *line, bool use_color)
{
    size_t len = strlen(line);

    /* Strip trailing newline for consistent output */
    char buf[1024];
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    memcpy(buf, line, len);
    buf[len] = '\0';
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    if (!use_color) {
        printf("%s\n", buf);
        return;
    }

    /* Title line (first non-empty line, no leading whitespace, contains program name) */
    if (strstr(buf, "stow-manager") && buf[0] != ' ') {
        if (strncmp(buf, "Usage:", 6) == 0) {
            /* "Usage: ..." line */
            printf("%s%sUsage:%s %s\n", COLOR_BOLD, COLOR_WHITE, COLOR_RESET, buf + 6);
            return;
        }
        if (strncmp(buf, "Dotfiles", 8) == 0) {
            printf("\n%s%s%s%s\n", COLOR_BOLD, COLOR_CYAN, buf, COLOR_RESET);
            return;
        }
    }

    /* Section headers: e.g. "USAGE:", "CORE COMMANDS:", "PACKAGE MANAGEMENT (pkg):" */
    if (buf[0] != ' ' && buf[0] != '\0' && len > 1) {
        size_t blen = strlen(buf);
        if (blen > 0 && buf[blen - 1] == ':') {
            printf("\n%s%s%s%s\n", COLOR_BOLD, COLOR_CYAN, buf, COLOR_RESET);
            return;
        }
    }

    /* Indented command/option lines: "  command  Description" */
    if (buf[0] == ' ' && buf[1] == ' ' && buf[2] != ' ' && buf[2] != '\0') {
        printf("%s\n", buf);
        return;
    }

    /* Sub-section label: "  Label:" (e.g. "  Scaffold & Configure Package:") */
    if (buf[0] == ' ' && buf[1] == ' ' && buf[2] != ' ') {
        printf("  %s•%s %s\n", COLOR_CYAN, COLOR_RESET, buf + 2);
        return;
    }

    /* Code example lines: "    command" (4-space indent) */
    if (strncmp(buf, "    ", 4) == 0 && buf[4] != '\0' && buf[4] != ' ') {
        printf("    %s$%s %s%s%s\n", COLOR_YELLOW, COLOR_RESET, COLOR_GREEN, buf + 4, COLOR_RESET);
        return;
    }

    /* Everything else: plain text */
    printf("%s\n", buf);
}

void show_help(void)
{
    bool use_color = isatty(STDOUT_FILENO) != 0 && getenv("NO_COLOR") == NULL;

    /* Always use help.txt — spacing is 1:1 with terminal output */
    const char *help_filename = "help.txt";

    StringArray search_paths;
    str_array_init(&search_paths);

    char data_home[PATH_MAX];
    get_xdg_data_home(data_home, sizeof(data_home));
    char p1[PATH_MAX * 2];
    snprintf(p1, sizeof(p1), "%s/stow-manager/%s", data_home, help_filename);
    str_array_append(&search_paths, p1);

    char config_home[PATH_MAX];
    get_xdg_config_home(config_home, sizeof(config_home));
    char p2[PATH_MAX * 2];
    snprintf(p2, sizeof(p2), "%s/stow-manager/%s", config_home, help_filename);
    str_array_append(&search_paths, p2);

    StringArray data_dirs;
    str_array_init(&data_dirs);
    get_xdg_data_dirs(&data_dirs);
    for (size_t i = 0; i < data_dirs.count; i++) {
        char path[PATH_MAX * 2];
        snprintf(path, sizeof(path), "%s/stow-manager/%s", data_dirs.items[i], help_filename);
        str_array_append(&search_paths, path);
    }
    str_array_free(&data_dirs);

#ifdef DATADIR
    char p3[PATH_MAX * 2];
    snprintf(p3, sizeof(p3), "%s/stow-manager/%s", DATADIR, help_filename);
    str_array_append(&search_paths, p3);
#endif

    char p_res[PATH_MAX];
    snprintf(p_res, sizeof(p_res), "resources/%s", help_filename);
    str_array_append(&search_paths, p_res);

    FILE *fp = NULL;
    for (size_t i = 0; i < search_paths.count; i++) {
        if (file_exists(search_paths.items[i])) {
            fp = fopen(search_paths.items[i], "r");
            if (fp) {
                break;
            }
        }
    }

    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            render_plain_line(line, use_color);
        }
        fclose(fp);
    } else {
        /* Embedded fallback (always plain text) */
        char *copy = strdup(EMBEDDED_HELP_TXT);
        if (copy) {
            char *saveptr = NULL;
            char *token = strtok_r(copy, "\n", &saveptr);
            while (token) {
                render_plain_line(token, use_color);
                token = strtok_r(NULL, "\n", &saveptr);
            }
            free(copy);
        }
    }

    str_array_free(&search_paths);
}
