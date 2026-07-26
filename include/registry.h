#ifndef REGISTRY_H
#define REGISTRY_H

#include "utils.h"

void registry_get_aliases(const char *dotfiles_dir, const char *tool, StringArray *aliases);
void registry_get_distro_pkg(const char *dotfiles_dir, const char *tool, const char *distro, char *out, size_t out_size);
void registry_get_all_tools(const char *dotfiles_dir, StringArray *tools);
bool is_tool_installed_dynamic(const char *dotfiles_dir, const char *tool);

#endif /* REGISTRY_H */
