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
#include "config.h"

void config_init(Config *cfg)
{
    memset(cfg->config_file_path, 0, sizeof(cfg->config_file_path));
    str_array_init(&cfg->dotfiles_dirs);
    memset(cfg->target_dir, 0, sizeof(cfg->target_dir));
    get_config_file_path(cfg->config_file_path, sizeof(cfg->config_file_path));
}

void config_free(Config *cfg)
{
    str_array_free(&cfg->dotfiles_dirs);
}

void get_config_file_path(char *buf, size_t buf_size)
{
    char xdg_config[PATH_MAX];
    get_xdg_config_home(xdg_config, sizeof(xdg_config));

    char primary[PATH_MAX * 2];
    snprintf(primary, sizeof(primary), "%s/stow-manager/config", xdg_config);

    if (file_exists(primary)) {
        snprintf(buf, buf_size, "%s", primary);
        return;
    }

    StringArray config_dirs;
    str_array_init(&config_dirs);
    get_xdg_config_dirs(&config_dirs);

    for (size_t i = 0; i < config_dirs.count; i++) {
        char sys_path[PATH_MAX * 2];
        snprintf(sys_path, sizeof(sys_path), "%s/stow-manager/config", config_dirs.items[i]);
        if (file_exists(sys_path)) {
            snprintf(buf, buf_size, "%s", sys_path);
            str_array_free(&config_dirs);
            return;
        }
    }
    str_array_free(&config_dirs);

    snprintf(buf, buf_size, "%s", primary);
}

bool config_load(Config *cfg)
{
    if (cfg->config_file_path[0] == '\0') {
        get_config_file_path(cfg->config_file_path, sizeof(cfg->config_file_path));
    }
    str_array_init(&cfg->dotfiles_dirs);
    memset(cfg->target_dir, 0, sizeof(cfg->target_dir));

    FILE *fp = fopen(cfg->config_file_path, "r");
    if (!fp) {
        return false;
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
            char *val = trim_whitespace(eq + 1);

            if (strcmp(key, "DOTFILES_DIR") == 0 || strcmp(key, "DOTFILES_DIRS") == 0) {
                char *saveptr = NULL;
                char *token = strtok_r(val, ":", &saveptr);
                while (token) {
                    char *p = trim_whitespace(token);
                    if (strlen(p) > 0 && is_dir(p) && !str_array_contains(&cfg->dotfiles_dirs, p)) {
                        str_array_append(&cfg->dotfiles_dirs, p);
                    }
                    token = strtok_r(NULL, ":", &saveptr);
                }
            } else if (strcmp(key, "TARGET_DIR") == 0) {
                snprintf(cfg->target_dir, sizeof(cfg->target_dir), "%s", val);
            }
        }
    }

    free(linebuf);
    fclose(fp);
    return true;
}

bool config_save(const Config *cfg)
{
    char dir_path[PATH_MAX * 2];
    snprintf(dir_path, sizeof(dir_path), "%s", cfg->config_file_path);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_p(dir_path, 0755);
    }

    FILE *fp = fopen(cfg->config_file_path, "w");
    if (!fp) {
        log_error("Failed to open config file for writing: %s", cfg->config_file_path);
        return false;
    }

    fprintf(fp, "# Stow Manager Configuration\n");
    fprintf(fp, "DOTFILES_DIRS=");
    for (size_t i = 0; i < cfg->dotfiles_dirs.count; i++) {
        fprintf(
            fp, "%s%s", cfg->dotfiles_dirs.items[i], (i + 1 < cfg->dotfiles_dirs.count) ? ":" : "");
    }
    fprintf(fp, "\n");

    if (cfg->target_dir[0] != '\0') {
        fprintf(fp, "TARGET_DIR=%s\n", cfg->target_dir);
    }

    fclose(fp);
    return true;
}

void config_set_dotfiles_dir(const char *path)
{
    char abs_path[PATH_MAX * 2];
    expand_tilde_path(path, abs_path, sizeof(abs_path));
    normalize_path(abs_path);

    if (!is_dir(abs_path)) {
        log_error("Directory does not exist: %s", abs_path);
        return;
    }

    Config cfg;
    config_init(&cfg);
    (void)config_load(&cfg);

    StringArray new_dirs;
    str_array_init(&new_dirs);
    str_array_append(&new_dirs, abs_path);

    for (size_t i = 0; i < cfg.dotfiles_dirs.count; i++) {
        if (strcmp(cfg.dotfiles_dirs.items[i], abs_path) != 0) {
            str_array_append(&new_dirs, cfg.dotfiles_dirs.items[i]);
        }
    }

    str_array_free(&cfg.dotfiles_dirs);
    cfg.dotfiles_dirs = new_dirs;

    if (config_save(&cfg)) {
        log_success("Set primary dotfiles directory to: %s", abs_path);
    }
    config_free(&cfg);
}

void config_add_dotfiles_dir(const char *path)
{
    char abs_path[PATH_MAX * 2];
    expand_tilde_path(path, abs_path, sizeof(abs_path));
    normalize_path(abs_path);

    if (!is_dir(abs_path)) {
        log_error("Directory does not exist: %s", abs_path);
        return;
    }

    Config cfg;
    config_init(&cfg);
    (void)config_load(&cfg);

    if (!str_array_contains(&cfg.dotfiles_dirs, abs_path)) {
        str_array_append(&cfg.dotfiles_dirs, abs_path);
        if (config_save(&cfg)) {
            log_success("Added dotfiles directory: %s", abs_path);
        }
    } else {
        log_info("Dotfiles directory already registered: %s", abs_path);
    }

    config_free(&cfg);
}

void config_remove_dotfiles_dir(const char *path)
{
    char abs_path[PATH_MAX * 2];
    expand_tilde_path(path, abs_path, sizeof(abs_path));
    normalize_path(abs_path);

    Config cfg;
    config_init(&cfg);
    (void)config_load(&cfg);

    StringArray new_dirs;
    str_array_init(&new_dirs);
    bool found = false;

    for (size_t i = 0; i < cfg.dotfiles_dirs.count; i++) {
        if (strcmp(cfg.dotfiles_dirs.items[i], abs_path) == 0) {
            found = true;
        } else {
            str_array_append(&new_dirs, cfg.dotfiles_dirs.items[i]);
        }
    }

    str_array_free(&cfg.dotfiles_dirs);
    cfg.dotfiles_dirs = new_dirs;

    if (found) {
        if (config_save(&cfg)) {
            log_success("Removed dotfiles directory: %s", abs_path);
        }
    } else {
        log_warn("Directory is not in dotfiles configuration: %s", abs_path);
    }

    config_free(&cfg);
}

void config_set_target_dir(const char *path)
{
    char abs_path[PATH_MAX];
    expand_tilde_path(path, abs_path, sizeof(abs_path));
    normalize_path(abs_path);

    Config cfg;
    config_init(&cfg);
    (void)config_load(&cfg);

    snprintf(cfg.target_dir, sizeof(cfg.target_dir), "%s", abs_path);

    if (config_save(&cfg)) {
        log_success("Set target directory to: %s", abs_path);
    }
    config_free(&cfg);
}

void config_show(void)
{
    Config cfg;
    config_init(&cfg);
    (void)config_load(&cfg);

    printf("\n=== Stow Manager Configuration ===\n\n");
    printf("  Config File Path: %s\n", cfg.config_file_path);

    printf("  Dotfiles Repositories:\n");
    if (cfg.dotfiles_dirs.count == 0) {
        printf("    (none configured - using current working directory fallback)\n");
    } else {
        for (size_t i = 0; i < cfg.dotfiles_dirs.count; i++) {
            printf(
                "    %zu. %s%s\n", i + 1, cfg.dotfiles_dirs.items[i], (i == 0) ? " (primary)" : "");
        }
    }

    if (cfg.target_dir[0] != '\0') {
        printf("  Target Directory: %s (configured)\n", cfg.target_dir);
    } else {
        char default_target[PATH_MAX];
        get_target_dir(default_target, sizeof(default_target));
        if (default_target[0] != '\0') {
            printf("  Target Directory: %s (fallback environment $HOME)\n", default_target);
        } else {
            printf("  Target Directory: (none - $HOME environment variable is not set)\n");
        }
    }
    printf("\n");

    config_free(&cfg);
}

void get_active_dotfiles_dir(const char *cli_override, char *buf, size_t buf_size)
{
    if (cli_override && strlen(cli_override) > 0) {
        expand_tilde_path(cli_override, buf, buf_size);
        normalize_path(buf);
        return;
    }

    const char *env_dir = getenv("STOW_DOTFILES_DIR");
    if (!env_dir) {
        env_dir = getenv("DOTFILES_DIR");
    }
    if (env_dir && strlen(env_dir) > 0) {
        expand_tilde_path(env_dir, buf, buf_size);
        normalize_path(buf);
        return;
    }

    char cwd[PATH_MAX * 2];
    if (getcwd(cwd, sizeof(cwd))) {
        char test_reg1[PATH_MAX * 2];
        char test_reg2[PATH_MAX * 2];
        join_path(test_reg1, sizeof(test_reg1), cwd, "stow.registry");
        join_path(test_reg2, sizeof(test_reg2), cwd, ".stowregistry");
        if (file_exists(test_reg1) || file_exists(test_reg2)) {
            snprintf(buf, buf_size, "%s", cwd);
            return;
        }
    }

    Config cfg;
    config_init(&cfg);
    if (config_load(&cfg) && cfg.dotfiles_dirs.count > 0) {
        snprintf(buf, buf_size, "%s", cfg.dotfiles_dirs.items[0]);
        config_free(&cfg);
        return;
    }
    config_free(&cfg);

    get_dotfiles_dir(buf, buf_size);
}

void get_active_target_dir(const char *cli_override, char *buf, size_t buf_size)
{
    if (cli_override && strlen(cli_override) > 0) {
        expand_tilde_path(cli_override, buf, buf_size);
        normalize_path(buf);
        return;
    }

    const char *env_target = getenv("STOW_TARGET_DIR");
    if (!env_target) {
        env_target = getenv("TARGET_DIR");
    }
    if (env_target && strlen(env_target) > 0) {
        expand_tilde_path(env_target, buf, buf_size);
        normalize_path(buf);
        return;
    }

    Config cfg;
    config_init(&cfg);
    if (config_load(&cfg) && cfg.target_dir[0] != '\0') {
        snprintf(buf, buf_size, "%s", cfg.target_dir);
        config_free(&cfg);
        return;
    }
    config_free(&cfg);

    get_target_dir(buf, buf_size);
    if (buf[0] == '\0') {
        log_error(
            "Fatal: $HOME environment variable is not set and no target directory was configured "
            "(-t / --target-dir). Exiting.");
        exit(EXIT_FAILURE);
    }
}
