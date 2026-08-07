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

#ifndef REGISTRY_H
#define REGISTRY_H

#include "utils/str.h"

void registry_get_aliases(const char *dotfiles_dir, const char *tool, StringArray *aliases);
void registry_get_distro_pkg(const char *dotfiles_dir,
                             const char *tool,
                             const char *distro,
                             char *out,
                             size_t out_size);
void registry_get_all_tools(const char *dotfiles_dir, StringArray *tools);
bool is_tool_installed_dynamic(const char *dotfiles_dir, const char *tool);

#endif /* REGISTRY_H */
