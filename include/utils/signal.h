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
#ifndef UTILS_SIGNAL_H
#define UTILS_SIGNAL_H

#include <signal.h>

extern volatile sig_atomic_t g_interrupted;

void setup_signal_handlers(void);
void register_temp_path(const char *path);
void unregister_temp_path(const char *path);
void cleanup_temp_paths(void);
void cleanup_temp_paths_signal_safe(void);

#endif /* UTILS_SIGNAL_H */
