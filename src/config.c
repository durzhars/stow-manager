#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "config.h"

void config_init(Config *cfg) {
    memset(cfg->config_file_path, 0, sizeof(cfg->config_file_path));
    str_array_init(&cfg->dotfiles_dirs);
    memset(cfg->target_dir, 0, sizeof(cfg->target_dir));
    get_config_file_path(cfg->config_file_path, sizeof(cfg->config_file_path));
}

void config_free(Config *cfg) {
    str_array_free(&cfg->dotfiles_dirs);
}

void get_config_file_path(char *buf, size_t buf_size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && strlen(xdg) > 0 && xdg[0] == '/') {
        snprintf(buf, buf_size, "%s/stow-manager/config", xdg);
    } else {
        const char *home = getenv("HOME");
        if (home && strlen(home) > 0) {
            snprintf(buf, buf_size, "%s/.config/stow-manager/config", home);
        } else {
            snprintf(buf, buf_size, "/tmp/.stow-manager.config");
        }
    }
}

bool config_load(Config *cfg) {
    if (cfg->config_file_path[0] == '\0') {
        get_config_file_path(cfg->config_file_path, sizeof(cfg->config_file_path));
    }
    str_array_init(&cfg->dotfiles_dirs);
    memset(cfg->target_dir, 0, sizeof(cfg->target_dir));

    FILE *fp = fopen(cfg->config_file_path, "r");
    if (!fp) return false;

    char *linebuf = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
        (void)linelen;
        char *trimmed = trim_whitespace(linebuf);
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

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

bool config_save(const Config *cfg) {
    char cfg_path[PATH_MAX * 2];
    snprintf(cfg_path, sizeof(cfg_path), "%s", cfg->config_file_path[0] != '\0' ? cfg->config_file_path : "/tmp/.stow-manager.config");

    char dir_path[PATH_MAX * 2];
    snprintf(dir_path, sizeof(dir_path), "%s", cfg_path);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_p(dir_path, 0755);
    }

    FILE *fp = fopen(cfg_path, "w");
    if (!fp) return false;

    fprintf(fp, "# Stow Manager Configuration\n");
    fprintf(fp, "DOTFILES_DIRS=\"");
    for (size_t i = 0; i < cfg->dotfiles_dirs.count; i++) {
        fprintf(fp, "%s%s", cfg->dotfiles_dirs.items[i], (i + 1 < cfg->dotfiles_dirs.count) ? ":" : "");
    }
    fprintf(fp, "\"\n");

    if (strlen(cfg->target_dir) > 0) {
        fprintf(fp, "TARGET_DIR=\"%s\"\n", cfg->target_dir);
    }

    fclose(fp);
    return true;
}

void config_set_dotfiles_dir(const char *path) {
    if (!path || strlen(path) == 0) {
        log_error("Usage: config set dotfiles <path>");
        return;
    }

    char abs_path[PATH_MAX * 2];
    expand_tilde_path(path, abs_path, sizeof(abs_path));
    normalize_path(abs_path);

    if (!is_dir(abs_path)) {
        log_error("Directory '%s' does not exist or is not a directory!", abs_path);
        return;
    }

    Config cfg;
    config_init(&cfg);
    config_load(&cfg);

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

    config_save(&cfg);
    log_success("Set primary dotfiles directory to: %s", abs_path);
    config_free(&cfg);
}

void config_add_dotfiles_dir(const char *path) {
    if (!path || strlen(path) == 0) {
        log_error("Usage: config add <path>");
        return;
    }

    char abs_path[PATH_MAX * 2];
    expand_tilde_path(path, abs_path, sizeof(abs_path));
    normalize_path(abs_path);

    if (!is_dir(abs_path)) {
        log_error("Directory '%s' does not exist or is not a directory!", abs_path);
        return;
    }

    Config cfg;
    config_init(&cfg);
    config_load(&cfg);

    if (!str_array_contains(&cfg.dotfiles_dirs, abs_path)) {
        str_array_append(&cfg.dotfiles_dirs, abs_path);
        config_save(&cfg);
        log_success("Added dotfiles directory: %s", abs_path);
    } else {
        log_info("Dotfiles directory already configured: %s", abs_path);
    }

    config_free(&cfg);
}

void config_remove_dotfiles_dir(const char *path) {
    if (!path || strlen(path) == 0) {
        log_error("Usage: config remove <path>");
        return;
    }

    char abs_path[PATH_MAX * 2];
    expand_tilde_path(path, abs_path, sizeof(abs_path));
    normalize_path(abs_path);

    Config cfg;
    config_init(&cfg);
    config_load(&cfg);

    StringArray new_dirs;
    str_array_init(&new_dirs);
    bool removed = false;

    for (size_t i = 0; i < cfg.dotfiles_dirs.count; i++) {
        if (strcmp(cfg.dotfiles_dirs.items[i], abs_path) == 0 || strcmp(cfg.dotfiles_dirs.items[i], path) == 0) {
            removed = true;
        } else {
            str_array_append(&new_dirs, cfg.dotfiles_dirs.items[i]);
        }
    }

    str_array_free(&cfg.dotfiles_dirs);
    cfg.dotfiles_dirs = new_dirs;

    if (removed) {
        config_save(&cfg);
        log_success("Removed dotfiles directory: %s", abs_path);
    } else {
        log_warn("Directory '%s' was not found in configuration.", path);
    }

    config_free(&cfg);
}

void config_set_target_dir(const char *path) {
    if (!path || strlen(path) == 0) {
        log_error("Usage: config set target <path>");
        return;
    }

    char abs_path[PATH_MAX * 2];
    expand_tilde_path(path, abs_path, sizeof(abs_path));
    normalize_path(abs_path);

    if (!is_dir(abs_path)) {
        log_error("Target directory '%s' does not exist or is not a directory!", abs_path);
        return;
    }

    Config cfg;
    config_init(&cfg);
    config_load(&cfg);

    snprintf(cfg.target_dir, sizeof(cfg.target_dir), "%s", abs_path);
    config_save(&cfg);
    log_success("Set target directory to: %s", abs_path);
    config_free(&cfg);
}

void config_show(void) {
    Config cfg;
    config_init(&cfg);
    config_load(&cfg);

    printf("\n%s%s=== Stow Manager Configuration ===%s\n\n", COLOR_CYAN, COLOR_BOLD, COLOR_RESET);
    printf("  %sConfig File Path:%s %s\n", COLOR_BOLD, COLOR_RESET, cfg.config_file_path);
    printf("  %sDotfiles Repositories:%s\n", COLOR_BOLD, COLOR_RESET);
    if (cfg.dotfiles_dirs.count > 0) {
        for (size_t i = 0; i < cfg.dotfiles_dirs.count; i++) {
            printf("    %s%zu.%s %s%s\n", COLOR_CYAN, i + 1, COLOR_RESET, cfg.dotfiles_dirs.items[i], (i == 0) ? " (primary)" : "");
        }
    } else {
        char cwd[PATH_MAX];
        get_dotfiles_dir(cwd, sizeof(cwd));
        printf("    %s (default current directory)\n", cwd);
    }

    printf("  %sTarget Directory:%s ", COLOR_BOLD, COLOR_RESET);
    if (strlen(cfg.target_dir) > 0) {
        printf("%s%s%s\n", COLOR_GREEN, cfg.target_dir, COLOR_RESET);
    } else {
        char tgt[PATH_MAX];
        get_target_dir(tgt, sizeof(tgt));
        printf("%s (fallback environment $HOME)\n", tgt);
    }
    printf("\n");

    config_free(&cfg);
}

void get_active_dotfiles_dir(const char *cli_override, char *buf, size_t buf_size) {
    if (cli_override && strlen(cli_override) > 0) {
        snprintf(buf, buf_size, "%s", cli_override);
        normalize_path(buf);
        return;
    }

    const char *env_dot = getenv("STOW_DOTFILES_DIR");
    if (!env_dot) env_dot = getenv("DOTFILES_DIR");
    if (env_dot && strlen(env_dot) > 0) {
        snprintf(buf, buf_size, "%s", env_dot);
        normalize_path(buf);
        return;
    }

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        char reg[PATH_MAX * 2];
        join_path(reg, sizeof(reg), cwd, "stow.registry");
        if (file_exists(reg)) {
            snprintf(buf, buf_size, "%s", cwd);
            normalize_path(buf);
            return;
        }
    }

    Config cfg;
    config_init(&cfg);
    if (config_load(&cfg) && cfg.dotfiles_dirs.count > 0) {
        snprintf(buf, buf_size, "%s", cfg.dotfiles_dirs.items[0]);
        config_free(&cfg);
        normalize_path(buf);
        return;
    }
    config_free(&cfg);

    get_dotfiles_dir(buf, buf_size);
    normalize_path(buf);
}

void get_active_target_dir(const char *cli_override, char *buf, size_t buf_size) {
    if (cli_override && strlen(cli_override) > 0) {
        snprintf(buf, buf_size, "%s", cli_override);
        normalize_path(buf);
        return;
    }

    const char *env_tgt = getenv("STOW_TARGET_DIR");
    if (!env_tgt) env_tgt = getenv("TARGET_DIR");
    if (env_tgt && strlen(env_tgt) > 0) {
        snprintf(buf, buf_size, "%s", env_tgt);
        normalize_path(buf);
        return;
    }

    Config cfg;
    config_init(&cfg);
    if (config_load(&cfg) && strlen(cfg.target_dir) > 0) {
        snprintf(buf, buf_size, "%s", cfg.target_dir);
        config_free(&cfg);
        normalize_path(buf);
        return;
    }
    config_free(&cfg);

    get_target_dir(buf, buf_size);
    normalize_path(buf);
}
