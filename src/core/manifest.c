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

#include "core/manifest.h"
#include "core/stow.h"

#include "utils/defs.h"
#include "utils/fs.h"
#include "utils/logger.h"
#include "utils/mem.h"
#include "utils/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void parse_space_delimited(const char *str, StringArray *arr)
{
    if (!str) {
        return;
    }
    char *copy = strdup(str);
    if (!copy) {
        return;
    }

    char *saveptr = NULL;
    char *token = strtok_r(copy, " \t\r\n", &saveptr);
    while (token) {
        str_array_append(arr, token);
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    free(copy);
}

void manifest_init(PackageManifest *manifest, const char *pkg_name)
{
    manifest->package_name = strdup(pkg_name);
    manifest->target_path = NULL;
    str_array_init(&manifest->required);
    str_array_init(&manifest->optional);
    str_array_init(&manifest->conflicts);
}

bool manifest_load(PackageManifest *manifest, const char *dotfiles_dir)
{
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, manifest->package_name);

    char path[PATH_MAX * 4];
    join_path(path, sizeof(path), pkg_dir, ".stowdeps");

    FILE *fp = fopen(path, "r");
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

            size_t vlen = strlen(val);
            if (vlen >= 2 && ((val[0] == '"' && val[vlen - 1] == '"') ||
                              (val[0] == '\'' && val[vlen - 1] == '\''))) {
                val[vlen - 1] = '\0';
                val++;
            }

            if (strcmp(key, "TARGET") == 0 || strcmp(key, "TARGET_DIR") == 0) {
                if (manifest->target_path) {
                    free(manifest->target_path);
                }
                manifest->target_path = safe_strdup(val);
            } else if (strcmp(key, "REQUIRED") == 0) {
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

bool manifest_save(const PackageManifest *manifest, const char *dotfiles_dir)
{
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, manifest->package_name);
    mkdir_p(pkg_dir, 0755);

    char path[PATH_MAX * 4];
    join_path(path, sizeof(path), pkg_dir, ".stowdeps");

    FILE *fp = fopen(path, "w");
    if (!fp) {
        return false;
    }

    fprintf(fp, "# Package Dependency Manifest for '%s'\n", manifest->package_name);

    if (manifest->target_path && strlen(manifest->target_path) > 0) {
        fprintf(fp, "TARGET=\"%s\"\n", manifest->target_path);
    }

    fprintf(fp, "REQUIRED=\"");
    for (size_t i = 0; i < manifest->required.count; i++) {
        fprintf(
            fp, "%s%s", manifest->required.items[i], (i + 1 < manifest->required.count) ? " " : "");
    }
    fprintf(fp, "\"\n");

    fprintf(fp, "OPTIONAL=\"");
    for (size_t i = 0; i < manifest->optional.count; i++) {
        fprintf(
            fp, "%s%s", manifest->optional.items[i], (i + 1 < manifest->optional.count) ? " " : "");
    }
    fprintf(fp, "\"\n");

    fprintf(fp, "CONFLICTS=\"");
    for (size_t i = 0; i < manifest->conflicts.count; i++) {
        fprintf(fp,
                "%s%s",
                manifest->conflicts.items[i],
                (i + 1 < manifest->conflicts.count) ? " " : "");
    }
    fprintf(fp, "\"\n");

    fclose(fp);
    return true;
}

void manifest_free(PackageManifest *manifest)
{
    if (!manifest) {
        return;
    }
    if (manifest->package_name) {
        free(manifest->package_name);
        manifest->package_name = NULL;
    }
    if (manifest->target_path) {
        free(manifest->target_path);
        manifest->target_path = NULL;
    }
    str_array_free(&manifest->required);
    str_array_free(&manifest->optional);
    str_array_free(&manifest->conflicts);
}

void manifest_set_target(const char *dotfiles_dir, const char *pkg_name, const char *target_path)
{
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    if (manifest.target_path) {
        free(manifest.target_path);
    }
    manifest.target_path = safe_strdup(target_path);

    if (manifest_save(&manifest, dotfiles_dir)) {
        log_success("Set target path for package '%s': %s", pkg_name, target_path);
    } else {
        log_error("Failed to save target path for package '%s'", pkg_name);
    }
    manifest_free(&manifest);
}

typedef enum { DEP_TYPE_REQUIRED, DEP_TYPE_CONFLICT, DEP_TYPE_OPTIONAL } DepType;

static DepType parse_dep_type(const char *type)
{
    if (type) {
        if (strcmp(type, "--required") == 0 || strcmp(type, "-r") == 0 ||
            strcmp(type, "required") == 0) {
            return DEP_TYPE_REQUIRED;
        }
        if (strcmp(type, "--conflict") == 0 || strcmp(type, "-c") == 0 ||
            strcmp(type, "conflict") == 0) {
            return DEP_TYPE_CONFLICT;
        }
    }
    return DEP_TYPE_OPTIONAL;
}

void manifest_add_dep(const char *dotfiles_dir,
                      const char *pkg_name,
                      const char *dep,
                      const char *type)
{
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    DepType dt = parse_dep_type(type);
    if (dt == DEP_TYPE_REQUIRED) {
        if (!str_array_contains(&manifest.required, dep)) {
            str_array_append(&manifest.required, dep);
            log_success("Added '%s' as REQUIRED dependency for package '%s'.", dep, pkg_name);
        }
    } else if (dt == DEP_TYPE_CONFLICT) {
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

static void str_array_filter_out(StringArray *arr, const char *target)
{
    size_t w = 0;
    for (size_t r = 0; r < arr->count; r++) {
        if (strcmp(arr->items[r], target) == 0) {
            free(arr->items[r]);
        } else {
            arr->items[w++] = arr->items[r];
        }
    }
    arr->count = w;
}

static void manifest_remove_dep_from_all(PackageManifest *manifest, const char *dep)
{
    str_array_filter_out(&manifest->required, dep);
    str_array_filter_out(&manifest->optional, dep);
    str_array_filter_out(&manifest->conflicts, dep);
}

void manifest_edit_dep(const char *dotfiles_dir,
                       const char *pkg_name,
                       const char *dep,
                       const char *new_type)
{
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    manifest_remove_dep_from_all(&manifest, dep);

    DepType dt = parse_dep_type(new_type);
    if (dt == DEP_TYPE_REQUIRED) {
        str_array_append(&manifest.required, dep);
        log_success("Updated '%s' to REQUIRED for package '%s'.", dep, pkg_name);
    } else if (dt == DEP_TYPE_CONFLICT) {
        str_array_append(&manifest.conflicts, dep);
        log_success("Updated '%s' to CONFLICT for package '%s'.", dep, pkg_name);
    } else {
        str_array_append(&manifest.optional, dep);
        log_success("Updated '%s' to OPTIONAL for package '%s'.", dep, pkg_name);
    }

    manifest_save(&manifest, dotfiles_dir);
    manifest_free(&manifest);
}

void manifest_remove_dep(const char *dotfiles_dir, const char *pkg_name, const char *dep)
{
    PackageManifest manifest;
    manifest_init(&manifest, pkg_name);
    manifest_load(&manifest, dotfiles_dir);

    manifest_remove_dep_from_all(&manifest, dep);

    manifest_save(&manifest, dotfiles_dir);
    log_success("Removed '%s' from package '%s'.", dep, pkg_name);
    manifest_free(&manifest);
}

void manifest_show(const char *dotfiles_dir, const char *pkg_name)
{
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);
    char path[PATH_MAX * 4];
    join_path(path, sizeof(path), pkg_dir, ".stowdeps");

    if (!file_exists(path)) {
        log_warn("Package '%s' does not have a '.stowdeps' manifest file.", pkg_name);
        return;
    }

    printf("\n%s%s=== Manifest [.stowdeps] for '%s' ===%s\n\n",
           COLOR_CYAN,
           COLOR_BOLD,
           pkg_name,
           COLOR_RESET);
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

void package_remove(const char *dotfiles_dir,
                    const char *target_dir,
                    const char *pkg_name,
                    bool dry_run)
{
    char pkg_dir[PATH_MAX * 2];
    join_path(pkg_dir, sizeof(pkg_dir), dotfiles_dir, pkg_name);

    if (!is_dir(pkg_dir)) {
        log_error("Package directory '%s' does not exist.", pkg_dir);
        return;
    }

    StowStatus status = get_package_stow_status(target_dir, dotfiles_dir, pkg_name);
    if (status != STOW_STATUS_UNSTOWED) {
        if (dry_run) {
            log_warn("[DRY-RUN] Package '%s' is currently stowed. Would unstow before removing.",
                     pkg_name);
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
    char escaped_pkg_dir[PATH_MAX * 3];
    escape_shell_arg(pkg_dir, escaped_pkg_dir, sizeof(escaped_pkg_dir));
    char rm_cmd[PATH_MAX * 3 + 32];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", escaped_pkg_dir);
    if (run_system_cmd(rm_cmd) == 0) {
        log_success("Successfully removed package '%s'.", pkg_name);
    } else {
        log_error("Failed to remove package directory '%s'.", pkg_dir);
    }
}
