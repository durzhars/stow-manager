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
#include "manifest.h"
#include "stow.h"

static void parse_space_delimited(const char *str, StringArray *arr) {
    if (!str) return;
    char *copy = strdup(str);
    if (!copy) return;

    char *saveptr = NULL;
    char *token = strtok_r(copy, " \t\r\n", &saveptr);
    while (token) {
        str_array_append(arr, token);
        token = strtok_r(NULL, " \t\r\n", &saveptr);
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
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, manifest->package_name);

    char path[PATH_MAX * 4];
    join_path(path, sizeof(path), pkg_dir, ".stowdeps");

    FILE *fp = fopen(path, "r");
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

            if (strcmp(key, "REQUIRED") == 0) {
                parse_space_delimited(val, &manifest->required);
            } else if (strcmp(key, "OPTIONAL") == 0) {
                parse_space_delimited(val, &manifest->optional);
            } else if (strcmp(key, "CONFLICTS") == 0) {
                parse_space_delimited(val, &manifest->conflicts);
            }
        }
    }

    free(linebuf);
    fclose(fp);
    return true;
}

bool manifest_save(const PackageManifest *manifest, const char *dotfiles_dir) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, manifest->package_name);
    mkdir_p(pkg_dir, 0755);

    char path[PATH_MAX * 4];
    join_path(path, sizeof(path), pkg_dir, ".stowdeps");

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

    if (type && (strcmp(type, "--required") == 0 || strcmp(type, "-r") == 0 || strcmp(type, "required") == 0)) {
        if (!str_array_contains(&manifest.required, dep)) {
            str_array_append(&manifest.required, dep);
            log_success("Added '%s' as REQUIRED dependency for package '%s'.", dep, pkg_name);
        }
    } else if (type && (strcmp(type, "--conflict") == 0 || strcmp(type, "-c") == 0 || strcmp(type, "conflict") == 0)) {
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

void manifest_edit_dep(const char *dotfiles_dir, const char *pkg_name, const char *dep, const char *new_type) {
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

    if (new_type && (strcmp(new_type, "--required") == 0 || strcmp(new_type, "-r") == 0 || strcmp(new_type, "required") == 0)) {
        str_array_append(&manifest.required, dep);
        log_success("Updated '%s' to REQUIRED for package '%s'.", dep, pkg_name);
    } else if (new_type && (strcmp(new_type, "--conflict") == 0 || strcmp(new_type, "-c") == 0 || strcmp(new_type, "conflict") == 0)) {
        str_array_append(&manifest.conflicts, dep);
        log_success("Updated '%s' to CONFLICT for package '%s'.", dep, pkg_name);
    } else {
        str_array_append(&manifest.optional, dep);
        log_success("Updated '%s' to OPTIONAL for package '%s'.", dep, pkg_name);
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
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);
    char path[PATH_MAX * 4];
    join_path(path, sizeof(path), pkg_dir, ".stowdeps");

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

void package_remove(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool dry_run) {
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) {
        log_error("Package directory '%s' does not exist.", pkg_dir);
        return;
    }

    StowStatus status = get_package_stow_status(target_dir, dotfiles_dir, pkg_name);
    if (status != STOW_STATUS_UNSTOWED) {
        if (dry_run) {
            log_warn("[DRY-RUN] Package '%s' is currently stowed. Would unstow before removing.", pkg_name);
        } else {
            log_warn("Package '%s' is stowed. Unstowing package prior to removal...", pkg_name);
            unstow_package(dotfiles_dir, target_dir, pkg_name, dry_run);
        }
    }

    if (dry_run) {
        log_info("[DRY-RUN] Would remove package directory: %s", pkg_dir);
        log_success("[DRY-RUN] Dry run complete for package removal '%s'.", pkg_name);
        return;
    }

    log_warn("Removing package directory '%s'...", pkg_dir);
    char rm_cmd[PATH_MAX * 2 + 32];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", pkg_dir);
    if (run_system_cmd(rm_cmd) == 0) {
        log_success("Successfully removed package '%s'.", pkg_name);
    } else {
        log_error("Failed to remove package directory '%s'.", pkg_dir);
    }
}
