#ifndef CHECKER_H
#define CHECKER_H

#include "manifest.h"

void check_package_dependencies(const char *dotfiles_dir, const char *target_pkg, bool auto_install);

#endif /* CHECKER_H */
