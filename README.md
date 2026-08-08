# Dotfiles Stow Manager (`stow-manager`)

[![ISO C17](https://img.shields.io/badge/C-ISO%20C17-blue.svg)](https://en.wikipedia.org/wiki/C17_(C_standard_revision))
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

A high-performance, zero-dependency, ISO C17 framework manager and dependency resolver for GNU Stow dotfiles packages.

`stow-manager` automates dotfiles package deployment, cross-distro package manager dependency installation (`pacman`, `apt`, `dnf`, `apk`, `brew`), mutual exclusion conflicts (e.g. `terminal` vs `headless`), GNU Stow directory symlink folding collisions, multi-repository management, and broken/orphan symlink integrity checking.

---

## Table of Contents

- [Features](#features)
- [Architecture & Resolution Order](#architecture--resolution-order)
  - [Target Directory Precedence](#target-directory-precedence)
  - [Dotfiles Repository Precedence](#dotfiles-repository-precedence)
- [Build & Installation](#build--installation)
  - [Standard Build Commands](#standard-build-commands)
  - [Advanced Clang Optimization & Diagnostic Profiles](#advanced-clang-optimization--diagnostic-profiles)
- [Command Line Reference](#command-line-reference)
  - [Global Options](#global-options)
  - [Stow & Deployment Commands](#stow--deployment-commands)
  - [Package Management (`pkg`)](#package-management-pkg)
  - [Dependency Management (`deps`)](#dependency-management-deps)
  - [File Filtering (`ignore`)](#file-filtering-ignore)
  - [Diagnostics & Repair (`check`)](#diagnostics--repair-check)
  - [Configuration (`config`)](#configuration-config)
- [Configuration & Manifest File Formats](#configuration--manifest-file-formats)
  - [Package Manifest (`.stowdeps`)](#package-manifest-stowdeps)
  - [Ignore Rules (`.stowignore`)](#ignore-rules-stowignore)
  - [Tool Registry (`stow.registry`)](#tool-registry-stowregistry)
- [Environment Variables](#environment-variables)
- [License](#license)

---

## Features

- **Zero-Dependency ISO C17**: Lightweight, high-performance native binary compiled with `-std=c17` and zero third-party library dependencies.
- **Symlink Unfolding & Collision Prevention (`fix-conflicts`)**: Automatically detects and unfolds directory symlinks in target directories to prevent GNU Stow directory folding collisions.
- **Dependency & Plugin Resolution (`deps`, `scan`)**: Auto-detects missing tools across Linux and macOS package managers (`pacman`, `apt`, `dnf`, `apk`, `brew`) and shell plugins (`stow.registry`).
- **Automated Dependency Scanner (`scan`)**: Recursively scans package scripts and configs for shebang interpreters and command invocations to auto-generate `.stowdeps` manifests.
- **Conflict & Mutual Exclusion Management**: Auto-unstows conflicting dotfile packages (defined in `.stowdeps` `CONFLICTS` or detected dynamically when target paths collide) prior to stowing.
- **Standardized Command Namespaces**: Intuitive CRUD command namespaces for package management (`pkg`), dependencies (`deps`), file filtering (`ignore`), and system settings (`config`).
- **Multi-Repository & Per-Package Target Configuration**: Manage multiple dotfiles repositories simultaneously and assign custom target directories per package (e.g., `/etc` or custom system paths).
- **Global & Package File Filtering (`ignore`)**: Manage `.stowignore` glob patterns at repository root or package level with inheritance and redundant pattern detection.
- **Symlink Health & Integrity Audit (`check-symlinks`)**: Scans repository for broken symlinks and target home for unmanaged orphan symlinks.
- **Safety & Signal Cleanups**: Built-in signal handlers (`SIGINT`/`Ctrl+C`) perform atomic temp directory cleanups on unexpected exits.
- **Performance Profiling (`-p`, `--profile`)**: High-precision nanosecond execution profiling for benchmarking deployment steps.

---

## Architecture & Resolution Order

### Target Directory Precedence

When resolving the target destination for symlink deployment, `stow-manager` checks sources in the following strict order of precedence:

1. **CLI Flag**: `-t, --target-dir <path>`
2. **Package Manifest**: `TARGET="/path"` entry in `.stowdeps` (when evaluating a specific package)
3. **Environment Variable**: `STOW_TARGET_DIR` or `TARGET_DIR`
4. **Configuration File**: `TARGET_DIR` set via `stow-manager config set target <path>`
5. **Fallback Environment**: `$HOME`

### Dotfiles Repository Precedence

When locating the active dotfiles repository directory:

1. **CLI Flag**: `-d, --dotfiles-dir <path>`
2. **Environment Variable**: `STOW_DOTFILES_DIR` or `DOTFILES_DIR`
3. **Working Directory Marker**: Current working directory if `stow.registry` or `.stowregistry` is present
4. **Configuration File**: Primary entry in `DOTFILES_DIRS` set via `stow-manager config set dotfiles <path>`
5. **Fallback Environment**: Current working directory (`getcwd`)

---

## Build & Installation

### Standard Build Commands

```bash
# Build release binary (bin/stow-manager)
make

# Run unit test suite (bin/test_runner)
make test

# Run end-to-end integration feature tests
make test-feature

# Build static binary for standalone distribution
make static

# Clean build artifacts
make clean

# Install binary and system resource files (defaults to /usr/local)
sudo make install

# Custom prefix installation (e.g. ~/.local)
make install PREFIX=$HOME/.local

# Uninstall binary and installed resource files
sudo make uninstall
```

### Advanced Clang Optimization & Diagnostic Profiles

The build system includes pre-configured Clang targets for performance tuning, sanitizer instrumentation, and static analysis:

```bash
# Build with Clang ThinLTO, -O3, and optimization remarks
make build-clang-opt

# 2-Stage Profile-Guided Optimization (PGO) build using feature test workload
make build-pgo

# Build with AddressSanitizer (ASan) & UndefinedBehaviorSanitizer (UBSan)
make build-sanitize

# Build binary optimized for size (-Oz, ThinLTO)
make build-size

# Run clang-tidy static analysis across all source and test files
make tidy

# Format source files with clang-format
make format

# Verify formatting compliance without modifying files
make format-check
```

---

## Command Line Reference

### Global Options

| Flag | Description |
| :--- | :--- |
| `-d, --dotfiles-dir <path>` | Set dotfiles repository directory for current command (e.g., `-d ~/dotfiles`). |
| `-t, --target-dir <path>` | Set target home directory for current command (e.g., `-t ~/`). |
| `-y, --install` | Auto-confirm installation of missing required dependencies & optional plugins without prompting. |
| `-n, --dry-run` | Preview disk changes, symlink creations, backups, and actions without modifying disk. |
| `-s, --save` | Save command-line directory overrides (`-d`/`-t`) directly to user configuration file. |
| `-p, --profile` | Enable nanosecond execution performance profiler logging (also enabled via `PROFILE=1`). |
| `-h, --help` | Display comprehensive help manual. |

---

### Stow & Deployment Commands

```bash
# Stow one or multiple packages (with automatic dependency & conflict handling)
stow-manager stow <pkg...>

# Short invocation (omitting 'stow' keyword defaults to stowing valid packages)
stow-manager <pkg...>

# Unstow one or multiple packages
stow-manager unstow <pkg...>

# Restow (unstow then stow) one or multiple packages
stow-manager restow <pkg...>

# Stow all packages present in dotfiles repository
stow-manager all

# Preview pending symlink creations, backups, and missing dependencies (dry-run)
stow-manager diff [pkg...]
```

---

### Package Management (`pkg`)

Namespace: `pkg` (aliases: `package`). Supports both space-separated (`pkg create`) and colon-separated (`pkg:create`) syntaxes.

```bash
# Scaffold a new package directory & initialize a default .stowdeps manifest
stow-manager pkg create <name>
# Aliases: pkg:create, package:create, make:pkg

# Safely unstow and remove package directory from disk
stow-manager pkg remove <name...>
# Aliases: pkg:remove, package:remove, pkg:rm

# List all packages with active stowed status ([STOWED], [PARTIAL], [UNSTOWED])
stow-manager pkg list
# Aliases: pkg:list, package:list, pkg:show, list
```

---

### Dependency Management (`deps`)

Namespace: `deps`. Supports both space-separated (`deps add`) and colon-separated (`deps:add`) syntaxes.

```bash
# Add a dependency or conflict entry to package .stowdeps manifest
stow-manager deps add <pkg> <dep> [--required | --optional | --conflict]
# Aliases: deps:add (default classification: --optional)

# Edit existing dependency classification
stow-manager deps edit <pkg> <dep> <type>
# Aliases: deps:edit, deps:set (type: --required, --optional, or --conflict)

# Remove a dependency or conflict entry from package manifest
stow-manager deps remove <pkg> <dep>
# Aliases: deps:remove, deps:rm

# Display raw .stowdeps manifest contents for a package
stow-manager deps show <pkg>
# Aliases: deps:show, deps:list

# Set per-package target directory override in package manifest
stow-manager deps target <pkg> <path>
# Aliases: deps:target

# Recursively scan package scripts/configs to auto-detect missing tools & plugins
stow-manager scan [pkg...]
```

---

### File Filtering (`ignore`)

Namespace: `ignore`. Manages `.stowignore` files at repository root (global) or inside individual packages.

```bash
# Scaffold global or package-level .stowignore template
stow-manager ignore init [pkg...]
# Aliases: ignore:init, ignore:create

# Append glob pattern(s) to package or global (-g) .stowignore
stow-manager ignore add [pkg] <pattern...>
stow-manager ignore add -g <pattern...>
# Aliases: ignore:add

# Remove glob pattern(s) from package or global (-g) .stowignore
stow-manager ignore remove [pkg] <pattern...>
stow-manager ignore remove -g <pattern...>
# Aliases: ignore:remove, ignore:rm, ignore:delete

# Purge .stowignore file(s) for package(s) or repository root
stow-manager ignore clear [pkg...]
# Aliases: ignore:clear, ignore:purge

# Display active .stowignore rules (indicates redundant package rules covered globally)
stow-manager ignore show [pkg...]
# Aliases: ignore:show, ignore:list
```

---

### Diagnostics & Repair (`check`)

```bash
# Verify required/optional tools, plugins, and symlink integrity for packages
stow-manager check [pkg...]

# Scan repository & target home for broken symlinks and unmanaged orphan symlinks
stow-manager check-symlinks
# Alias: stow-manager check symlinks

# Unfold directory symlinks in target into real directories to resolve Stow folding collisions
stow-manager fix-conflicts
# Alias: stow-manager fix
```

---

### Configuration (`config`)

Namespace: `config`. Manages global settings in `~/.config/stow-manager/config`.

```bash
# Display active configuration, dotfiles repositories, and target directory
stow-manager config show
# Aliases: config:show, config:list, config:get

# Set primary dotfiles repository or default target directory
stow-manager config set target <path>
stow-manager config set dotfiles <path>
# Aliases: config:set, config:target

# Add an additional dotfiles repository directory (multi-repository setup)
stow-manager config add <path>
# Aliases: config:add

# Remove a dotfiles repository directory from configuration
stow-manager config remove <path>
# Aliases: config:remove, config:rm
```

---

## Configuration & Manifest File Formats

### Package Manifest (`.stowdeps`)

Located inside individual package directories (e.g. `~/dotfiles/hyprland/.stowdeps`).

```ini
# Package Dependency Manifest for 'hyprland'
TARGET="/home/user"
REQUIRED="hyprland waybar"
OPTIONAL="rofi dunst"
CONFLICTS="sway"
```

- **`TARGET`**: Custom destination target path for this package.
- **`REQUIRED`**: Space-separated list of required CLI executables/tools.
- **`OPTIONAL`**: Space-separated list of optional plugins or secondary utilities.
- **`CONFLICTS`**: Space-separated list of packages that must be unstowed before stowing this package.

---

### Ignore Rules (`.stowignore`)

Follows standard glob pattern rules. Global `.stowignore` resides at repository root; package `.stowignore` resides inside package directories.

```gitignore
# Global or package-level ignore patterns
*.zwc
*.pyc
*.stow_backup_*
.DS_Store
Thumbs.db
.idea/
.vscode/
```

Default ignored patterns (mirrors GNU Stow default ignore list): `.stowdeps`, `.stowignore`, `.git`, `.gitignore`, `.gitattributes`, `.gitmodules`, `.DS_Store`, `CVS`, `.svn`, `.hg`, `README*`, `LICENSE*`, `COPYING*`, `*~`, `#*#`, `.#*`.

---

### Tool Registry (`stow.registry`)

Optionally placed in dotfiles repository root (`stow.registry` or `.stowregistry`) to map tool names to distro package manager names and shell plugin locations.

```ini
# Distro package name overrides (tool@distro=package_name)
neovim@arch=neovim
neovim@ubuntu=neovim
fd@ubuntu=fd-find
ripgrep@debian=ripgrep

# Tool alias & shell plugin path mapping (tool=alias1|alias2|plugin:~/.zsh/plugins/tool)
zsh-autosuggestions=plugin:~/.zsh/plugins/zsh-autosuggestions
bat=bat|batcat
```

---

## Environment Variables

| Variable | Description |
| :--- | :--- |
| `STOW_DOTFILES_DIR` / `DOTFILES_DIR` | Override active dotfiles repository directory path. |
| `STOW_TARGET_DIR` / `TARGET_DIR` | Override target destination home directory path. |
| `PROFILE` / `STOW_PROFILE` | Set to non-empty string to enable execution profiling. |
| `HOME` | Default target home directory when no override is configured. |
| `XDG_CONFIG_HOME` | Primary directory for `stow-manager/config` (`~/.config`). |
| `XDG_CONFIG_DIRS` | System-wide search directories for `stow-manager/config`. |
| `NO_COLOR` | Disable ANSI color codes in terminal help output. |

---

## License

Licensed under the [GNU General Public License v3.0](LICENSE).
