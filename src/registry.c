#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "registry.h"

void registry_get_aliases(const char *dotfiles_dir, const char *tool, StringArray *aliases) {
    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), "%s/stow.registry", dotfiles_dir);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        str_array_append(aliases, tool);
        return;
    }

    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim_whitespace(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

        char *eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim_whitespace(trimmed);
            char *val = trim_whitespace(eq + 1);

            if (strcmp(key, tool) == 0) {
                found = true;
                char *token = strtok(val, "|");
                while (token) {
                    char *alias = trim_whitespace(token);
                    if (strlen(alias) > 0) {
                        str_array_append(aliases, alias);
                    }
                    token = strtok(NULL, "|");
                }
                break;
            }
        }
    }
    fclose(fp);

    if (!found || aliases->count == 0) {
        str_array_append(aliases, tool);
    }
}

void registry_get_distro_pkg(const char *dotfiles_dir, const char *tool, const char *distro, char *out, size_t out_size) {
    snprintf(out, out_size, "%s", tool);

    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), "%s/stow.registry", dotfiles_dir);

    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char target_key[256];
    snprintf(target_key, sizeof(target_key), "%s@%s", tool, distro);

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim_whitespace(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

        char *eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim_whitespace(trimmed);
            char *val = trim_whitespace(eq + 1);

            if (strcmp(key, target_key) == 0) {
                snprintf(out, out_size, "%s", val);
                break;
            }
        }
    }
    fclose(fp);
}

void registry_get_all_tools(const char *dotfiles_dir, StringArray *tools) {
    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), "%s/stow.registry", dotfiles_dir);

    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim_whitespace(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

        char *eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim_whitespace(trimmed);
            if (!strchr(key, '@') && !str_array_contains(tools, key)) {
                str_array_append(tools, key);
            }
        }
    }
    fclose(fp);
}

bool is_tool_installed_dynamic(const char *dotfiles_dir, const char *tool) {
    StringArray aliases;
    str_array_init(&aliases);
    registry_get_aliases(dotfiles_dir, tool, &aliases);

    bool installed = false;
    for (size_t i = 0; i < aliases.count; i++) {
        const char *entry = aliases.items[i];
        if (strncmp(entry, "plugin:", 7) == 0) {
            const char *plugin_path = entry + 7;
            if (access(plugin_path, R_OK) == 0) {
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
