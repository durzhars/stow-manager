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
#ifndef STR
#define XSTR(s) #s
#define STR(s) XSTR(s)
#endif

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

static void render_plain_line(const char *line, int use_color)
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
    int use_color = isatty(STDOUT_FILENO) != 0 && getenv("NO_COLOR") == NULL;

    FILE *fp = open_resource_file("help.txt");
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
}
