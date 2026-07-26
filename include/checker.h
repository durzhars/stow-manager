#ifndef CHECKER_H
#define CHECKER_H

#include "manifest.h"

void check_package_dependencies(const char *dotfiles_dir, const char *target_pkg, bool auto_install);
void check_symlink_health(const char *dotfiles_dir, const char *target_dir);

#endif /* CHECKER_H */
