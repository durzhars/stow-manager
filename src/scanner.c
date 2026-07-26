#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "scanner.h"
#include "registry.h"

static void parse_shebang_interpreter(const char *first_line, StringArray *shebangs) {
    if (strncmp(first_line, "#!", 2) != 0) return;

    char *copy = strdup(first_line + 2);
    if (!copy) return;

    char *saveptr = NULL;
    char *token = strtok_r(copy, " \t\r\n", &saveptr);

    while (token) {
        char *trimmed = trim_whitespace(token);
        if (trimmed[0] != '\0') {
            if (trimmed[0] != '-') {
                char *base = strrchr(trimmed, '/');
                if (base) trimmed = base + 1;

                if (strcmp(trimmed, "env") != 0 && strcmp(trimmed, "exec") != 0) {
                    if (strlen(trimmed) > 0 && !str_array_contains(shebangs, trimmed)) {
                        str_array_append(shebangs, trimmed);
                    }
                    break;
                }
            }
        }
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    free(copy);
}

static void process_single_file(const char *filepath, const StringArray *candidate_tools, StringArray *shebangs, StringArray *invocations) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return;

    char *linebuf = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    bool is_first_line = true;

    typedef struct {
        const char *tool;
        char p1[256], p2[256], p3[256], p4[256];
        bool found;
    } ToolPattern;

    ToolPattern *patterns = NULL;
    if (candidate_tools && candidate_tools->count > 0) {
        patterns = calloc(candidate_tools->count, sizeof(ToolPattern));
        if (patterns) {
            for (size_t t = 0; t < candidate_tools->count; t++) {
                patterns[t].tool = candidate_tools->items[t];
                patterns[t].found = str_array_contains(invocations, candidate_tools->items[t]);
                snprintf(patterns[t].p1, sizeof(patterns[t].p1), "command -v %s", patterns[t].tool);
                snprintf(patterns[t].p2, sizeof(patterns[t].p2), "exec %s", patterns[t].tool);
                snprintf(patterns[t].p3, sizeof(patterns[t].p3), "%s init", patterns[t].tool);
                snprintf(patterns[t].p4, sizeof(patterns[t].p4), "%s -c", patterns[t].tool);
            }
        }
    }

    while ((linelen = getline(&linebuf, &linecap, fp)) != -1) {
        if (is_first_line) {
            is_first_line = false;
            parse_shebang_interpreter(linebuf, shebangs);
        }

        if (patterns) {
            for (size_t t = 0; t < candidate_tools->count; t++) {
                if (!patterns[t].found) {
                    if (strstr(linebuf, patterns[t].p1) || strstr(linebuf, patterns[t].p2) ||
                        strstr(linebuf, patterns[t].p3) || strstr(linebuf, patterns[t].p4)) {
                        patterns[t].found = true;
                        if (!str_array_contains(invocations, patterns[t].tool)) {
                            str_array_append(invocations, patterns[t].tool);
                        }
                    }
                }
            }
        }
    }

    free(linebuf);
    if (patterns) free(patterns);
    fclose(fp);
}

static void scan_dir_recursive(const char *dotfiles_dir, const char *dir_path, const StringArray *candidate_tools, StringArray *shebangs, StringArray *invocations) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    char path[PATH_MAX * 2];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        join_path(path, sizeof(path), dir_path, entry->d_name);

        if (is_dir(path)) {
            scan_dir_recursive(dotfiles_dir, path, candidate_tools, shebangs, invocations);
        } else if (file_exists(path) && !is_symlink(path)) {
            process_single_file(path, candidate_tools, shebangs, invocations);
        }
    }

    closedir(dir);
}

void scan_package(const char *dotfiles_dir, const char *pkg_name) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!file_exists(pkg_dir)) {
        log_error("Package directory '%s' does not exist!", pkg_name);
        return;
    }

    log_info("Recursively scanning package content in '%s' for dependencies...", pkg_name);

    StringArray candidate_tools;
    str_array_init(&candidate_tools);
    registry_get_all_tools(dotfiles_dir, &candidate_tools);

    StringArray shebangs, invocations;
    str_array_init(&shebangs);
    str_array_init(&invocations);

    scan_dir_recursive(dotfiles_dir, pkg_dir, &candidate_tools, &shebangs, &invocations);

    printf("  %sScan Results for package '%s':%s\n", COLOR_BOLD, pkg_name, COLOR_RESET);
    printf("    %sDetected Shebangs (Required):%s ", COLOR_BOLD, COLOR_RESET);
    if (shebangs.count > 0) {
        for (size_t i = 0; i < shebangs.count; i++) printf("%s ", shebangs.items[i]);
    } else {
        printf("none");
    }
    printf("\n");

    printf("    %sDetected Invocations (Optional):%s ", COLOR_BOLD, COLOR_RESET);
    if (invocations.count > 0) {
        for (size_t i = 0; i < invocations.count; i++) printf("%s ", invocations.items[i]);
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
