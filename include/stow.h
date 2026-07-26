#ifndef STOW_ENGINE_H
#define STOW_ENGINE_H

#include "checker.h"

void unfold_directory_symlinks(const char *target_dir, const char *dotfiles_dir);
void prepare_target_conflicts(const char *target_dir, const char *dotfiles_dir, const char *pkg_name);

bool is_package_stowed(const char *target_dir, const char *dotfiles_dir, const char *pkg_name);
void handle_mutual_exclusions(const char *target_dir, const char *dotfiles_dir, const char *pkg_name);

int stow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install);
int unstow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name);
int restow_package(const char *dotfiles_dir, const char *target_dir, const char *pkg_name, bool auto_install);

void stow_all_packages(const char *dotfiles_dir, const char *target_dir, bool auto_install);
void list_packages_status(const char *dotfiles_dir, const char *target_dir);

#endif /* STOW_ENGINE_H */
