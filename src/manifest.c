#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "manifest.h"

static void parse_space_delimited(const char *str, StringArray *arr) {
    if (!str) return;
    char *copy = strdup(str);
    if (!copy) return;

    char *token = strtok(copy, " \t\r\n");
    while (token) {
        str_array_append(arr, token);
        token = strtok(NULL, " \t\r\n");
    }
    free(copy);
}

void manifest_init(PackageManifest *manifest, const char *pkg_name) {
    manifest->package_name = strdup(pkg_name);
    str_array_init(&manifest->required);
    str_array_init(&manifest->optional);
    str_array_init(&manifest->conflicts);
}

bool manifest_load(PackageManifest *manifest, const char *dotfiles_dir) {
    char path[PATH_MAX * 4];
    snprintf(path, sizeof(path), "%s/%s/.stowdeps", dotfiles_dir, manifest->package_name);

    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim_whitespace(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

        char *eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim_whitespace(trimmed);
            char *val = trim_whitespace(eq + 1);

            if (strcmp(key, "REQUIRED") == 0) {
                parse_space_delimited(val, &manifest->required);
            } else if (strcmp(key, "OPTIONAL") == 0) {
                parse_space_delimited(val, &manifest->optional);
            } else if (strcmp(key, "CONFLICTS") == 0) {
                parse_space_delimited(val, &manifest->conflicts);
            }
        }
    }

    fclose(fp);
    return true;
}

bool manifest_save(const PackageManifest *manifest, const char *dotfiles_dir) {
    char pkg_dir[PATH_MAX * 2];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", dotfiles_dir, manifest->package_name);
    mkdir(pkg_dir, 0755);

    char path[PATH_MAX * 4];
    snprintf(path, sizeof(path), "%s/.stowdeps", pkg_dir);

    FILE *fp = fopen(path, "w");
    if (!fp) return false;

    fprintf(fp, "# Package Dependency Manifest for '%s'\n", manifest->package_name);

    fprintf(fp, "REQUIRED=\"");
    for (size_t i = 0; i < manifest->required.count; i++) {
        fprintf(fp, "%s%s", manifest->required.items[i], (i + 1 < manifest->required.count) ? " " : "");
    }
    fprintf(fp, "\"\n");

    fprintf(fp, "OPTIONAL=\"");
    for (size_t i = 0; i < manifest->optional.count; i++) {
        fprintf(fp, "%s%s", manifest->optional.items[i], (i + 1 < manifest->optional.count) ? " " : "");
    }
    fprintf(fp, "\"\n");

    fprintf(fp, "CONFLICTS=\"");
    for (size_t i = 0; i < manifest->conflicts.count; i++) {
        fprintf(fp, "%s%s", manifest->conflicts.items[i], (i + 1 < manifest->conflicts.count) ? " " : "");
    }
    fprintf(fp, "\"\n");

    fclose(fp);
    return true;
}

void manifest_free(PackageManifest *manifest) {
    if (!manifest) return;
    free(manifest->package_name);
    str_array_free(&manifest->required);
    str_array_free(&manifest->optional);
    str_array_free(&manifest->conflicts);
}

void manifest_add_dep(const char *dotfiles_dir, const char *pkg_name, const char *dep, const char *type) {
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    if (type && (strcmp(type, "--required") == 0 || strcmp(type, "-r") == 0)) {
        if (!str_array_contains(&manifest.required, dep)) {
            str_array_append(&manifest.required, dep);
            log_success("Added '%s' as REQUIRED dependency for package '%s'.", dep, pkg_name);
        }
    } else if (type && (strcmp(type, "--conflict") == 0 || strcmp(type, "-c") == 0)) {
        if (!str_array_contains(&manifest.conflicts, dep)) {
            str_array_append(&manifest.conflicts, dep);
            log_success("Added '%s' as CONFLICT entry for package '%s'.", dep, pkg_name);
        }
    } else {
        if (!str_array_contains(&manifest.optional, dep)) {
            str_array_append(&manifest.optional, dep);
            log_success("Added '%s' as OPTIONAL dependency for package '%s'.", dep, pkg_name);
        }
    }

    manifest_save(&manifest, dotfiles_dir);
    manifest_free(&manifest);
}

void manifest_remove_dep(const char *dotfiles_dir, const char *pkg_name, const char *dep) {
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    StringArray new_req, new_opt, new_cnf;
    str_array_init(&new_req);
    str_array_init(&new_opt);
    str_array_init(&new_cnf);

    for (size_t i = 0; i < manifest.required.count; i++) {
        if (strcmp(manifest.required.items[i], dep) != 0) str_array_append(&new_req, manifest.required.items[i]);
    }
    for (size_t i = 0; i < manifest.optional.count; i++) {
        if (strcmp(manifest.optional.items[i], dep) != 0) str_array_append(&new_opt, manifest.optional.items[i]);
    }
    for (size_t i = 0; i < manifest.conflicts.count; i++) {
        if (strcmp(manifest.conflicts.items[i], dep) != 0) str_array_append(&new_cnf, manifest.conflicts.items[i]);
    }

    str_array_free(&manifest.required);
    str_array_free(&manifest.optional);
    str_array_free(&manifest.conflicts);

    manifest.required = new_req;
    manifest.optional = new_opt;
    manifest.conflicts = new_cnf;

    manifest_save(&manifest, dotfiles_dir);
    log_success("Removed '%s' from package '%s'.", dep, pkg_name);
    manifest_free(&manifest);
}

void manifest_show(const char *dotfiles_dir, const char *pkg_name) {
    char path[PATH_MAX * 4];
    snprintf(path, sizeof(path), "%s/%s/.stowdeps", dotfiles_dir, pkg_name);

    if (!file_exists(path)) {
        log_warn("Package '%s' does not have a '.stowdeps' manifest file.", pkg_name);
        return;
    }

    printf("\n%s%s=== Manifest [.stowdeps] for '%s' ===%s\n\n", COLOR_CYAN, COLOR_BOLD, pkg_name, COLOR_RESET);
    FILE *fp = fopen(path, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            fputs(line, stdout);
        }
        fclose(fp);
    }
    printf("\n");
}
