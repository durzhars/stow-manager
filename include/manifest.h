#ifndef MANIFEST_H
#define MANIFEST_H

#include "utils.h"

typedef struct {
    char *package_name;
    StringArray required;
    StringArray optional;
    StringArray conflicts;
} PackageManifest;

void manifest_init(PackageManifest *manifest, const char *pkg_name);
bool manifest_load(PackageManifest *manifest, const char *dotfiles_dir);
bool manifest_save(const PackageManifest *manifest, const char *dotfiles_dir);
void manifest_free(PackageManifest *manifest);

void manifest_add_dep(const char *dotfiles_dir, const char *pkg_name, const char *dep, const char *type);
void manifest_remove_dep(const char *dotfiles_dir, const char *pkg_name, const char *dep);
void manifest_show(const char *dotfiles_dir, const char *pkg_name);

#endif /* MANIFEST_H */
