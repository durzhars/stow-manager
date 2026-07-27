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
#include "registry.h"

static FILE *open_registry_file(const char *dotfiles_dir)
{
    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), "%s/stow.registry", dotfiles_dir);
    FILE *fp = fopen(path, "r");
    if (fp) {
        return fp;
    }

    snprintf(path, sizeof(path), "%s/.stowregistry", dotfiles_dir);
    fp = fopen(path, "r");
    if (fp) {
        return fp;
    }

    char data_home[PATH_MAX];
    get_xdg_data_home(data_home, sizeof(data_home));
    snprintf(path, sizeof(path), "%s/stow-manager/stow.registry", data_home);
    fp = fopen(path, "r");
    if (fp) {
        return fp;
    }

    char config_home[PATH_MAX];
    get_xdg_config_home(config_home, sizeof(config_home));
    snprintf(path, sizeof(path), "%s/stow-manager/stow.registry", config_home);
    fp = fopen(path, "r");
    if (fp) {
        return fp;
    }

    StringArray data_dirs;
    str_array_init(&data_dirs);
    get_xdg_data_dirs(&data_dirs);
    for (size_t i = 0; i < data_dirs.count; i++) {
        snprintf(path, sizeof(path), "%s/stow-manager/stow.registry", data_dirs.items[i]);
        fp = fopen(path, "r");
        if (fp) {
            str_array_free(&data_dirs);
            return fp;
        }
    }
    str_array_free(&data_dirs);

    return NULL;
}

void registry_get_aliases(const char *dotfiles_dir, const char *tool, StringArray *aliases)
{
    FILE *fp = open_registry_file(dotfiles_dir);
    if (!fp) {
        str_array_append(aliases, tool);
        return;
    }

    char *linebuf = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    bool found = false;

    while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
        (void)linelen;
        char *trimmed = trim_whitespace(linebuf);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim_whitespace(trimmed);
            char *val = trim_whitespace(eq + 1);

            if (strcmp(key, tool) == 0) {
                found = true;
                char *saveptr = NULL;
                char *token = strtok_r(val, "|", &saveptr);
                while (token) {
                    char *p = trim_whitespace(token);
                    if (strlen(p) > 0 && !str_array_contains(aliases, p)) {
                        str_array_append(aliases, p);
                    }
                    token = strtok_r(NULL, "|", &saveptr);
                }
                break;
            }
        }
    }

    if (!found) {
        str_array_append(aliases, tool);
    }

    free(linebuf);
    fclose(fp);
}

void registry_get_distro_pkg(const char *dotfiles_dir,
                             const char *tool,
                             const char *distro_id,
                             char *pkg_out,
                             size_t pkg_out_size)
{
    snprintf(pkg_out, pkg_out_size, "%s", tool);

    FILE *fp = open_registry_file(dotfiles_dir);
    if (!fp) {
        return;
    }

    char key_distro[256];
    snprintf(key_distro, sizeof(key_distro), "%s@%s", tool, distro_id);

    char *linebuf = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
        (void)linelen;
        char *trimmed = trim_whitespace(linebuf);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim_whitespace(trimmed);
            char *val = trim_whitespace(eq + 1);

            if (strcmp(key, key_distro) == 0) {
                snprintf(pkg_out, pkg_out_size, "%s", val);
                break;
            }
        }
    }

    free(linebuf);
    fclose(fp);
}

void registry_get_all_tools(const char *dotfiles_dir, StringArray *tools)
{
    FILE *fp = open_registry_file(dotfiles_dir);
    if (!fp) {
        return;
    }

    char *linebuf = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
        (void)linelen;
        char *trimmed = trim_whitespace(linebuf);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim_whitespace(trimmed);
            char *at = strchr(key, '@');
            if (at) {
                *at = '\0';
            }
            char *tool = trim_whitespace(key);
            if (strlen(tool) > 0 && !str_array_contains(tools, tool)) {
                str_array_append(tools, tool);
            }
        }
    }

    free(linebuf);
    fclose(fp);
}

bool is_tool_installed_dynamic(const char *dotfiles_dir, const char *tool)
{
    StringArray aliases;
    str_array_init(&aliases);
    registry_get_aliases(dotfiles_dir, tool, &aliases);

    bool installed = false;
    for (size_t i = 0; i < aliases.count; i++) {
        const char *entry = aliases.items[i];
        if (strncmp(entry, "plugin:", 7) == 0) {
            const char *plugin_path = entry + 7;
            char expanded[PATH_MAX * 2];
            expand_tilde_path(plugin_path, expanded, sizeof(expanded));
            if (access(expanded, R_OK) == 0) {
                installed = true;
                break;
            }
        } else {
            if (is_executable_in_path(entry)) {
                installed = true;
                break;
            }
        }
    }

    str_array_free(&aliases);
    return installed;
}
