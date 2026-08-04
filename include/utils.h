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
#ifndef UTILS_H
#define UTILS_H

/*
 * Umbrella header — includes every utils sub-header for backward
 * compatibility.  New code should include individual sub-headers
 * from utils/ for finer-grained dependency control.
 */

#include "utils/defs.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/mem.h"
#include "utils/path.h"
#include "utils/signal.h"
#include "utils/stowignore.h"

/* Transitive system headers kept for backward compatibility */
#include "logger.h"
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif /* UTILS_H */
