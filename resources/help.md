# Dotfiles Stow Manager (`stow-manager`)

**Usage**: `stow-manager [options] <command> [arguments]`

High-performance ISO C17 dotfiles framework & Stow package manager. Auto-resolves package dependencies, mutual exclusion conflicts, directory symlink folding collisions, and multi-repository setups.

## Global Options

- **`-d, --dotfiles-dir`** `<path>` : Set dotfiles repository directory for current command (e.g. `-d ~/dotfiles`)
- **`-t, --target-dir`** `<path>`   : Set target home directory for current command (e.g. `-t ~/`)
- **`-y, --install`**             : Auto-confirm installation of missing required dependencies & optional plugins
- **`-n, --dry-run`**             : Dry-run mode (preview disk changes, symlink creations & backups without modifying disk)
- **`-h, --help`**                : Display this comprehensive help manual

## Configuration Commands (`config:*`)

- **`config show`**                        : Display active configuration, dotfiles repositories & target directory
- **`config set [dotfiles|target]`** `<path>` : Set primary dotfiles repository or target home directory
- **`config add`** `<path>`                  : Add an additional dotfiles repository directory (multi-repo mode)
- **`config remove`** `<path>`               : Remove a dotfiles repository directory from config

## Package Management Commands (`pkg:*`)

- **`pkg:create`** `<name>`             : Scaffold a new Stow package directory & initialize `.stowdeps` manifest (alias: `make:package`)
- **`pkg:remove`** `<name ...>`         : Safely unstow and remove one or multiple Stow package directories (alias: `remove:package`)
- **`pkg:list`**                       : List all packages with status: `[STOWED]`, `[PARTIAL]`, or `[UNSTOWED]` (alias: `list`)

## Dependency & Manifest Commands (`deps:*`)

- **`deps:add`** `<pkg> <dep> [--type]`  : Add dependency/conflict to `.stowdeps` (`--required`, `--optional`, `--conflict`)
- **`deps:edit`** `<pkg> <dep> <type>`   : Edit existing dependency classification (`--required`, `--optional`, `--conflict`)
- **`deps:remove`** `<pkg> <dep>`        : Remove a dependency or conflict entry from package `.stowdeps`
- **`deps:show`** `<pkg>`               : Display raw `.stowdeps` manifest contents for a package
- **`scan`** `[pkg ...]`                 : Recursively scan package scripts/configs to auto-detect required tools & plugins

## Stow & Deployment Commands

- **`stow`** `<pkg ...>`                 : Stow one or multiple packages with automatic conflict & dependency resolution
- **`unstow`** `<pkg ...>`               : Unstow one or multiple packages from target directory
- **`restow`** `<pkg ...>`               : Restow (unstow & stow) one or multiple packages
- **`all`**                            : Stow all packages present in dotfiles repository
- **`diff`** `[pkg ...]`                 : Preview symlink creations, conflict backups, and missing dependencies (dry-run)
- **`fix-conflicts`**                  : Unfold directory symlinks in target into real directories to resolve folding collisions
- **`check`** `[pkg ...]`                : Verify required/optional tools, plugins, and symlink integrity for packages
- **`check-symlinks`**                 : Scan repository & target home for broken symlinks and unmanaged orphan symlinks
- **`help`**                           : Display this help manual

## Workflow Examples

- **Scaffold & Configure Package**:
  `stow-manager pkg:create hyprland`
  `stow-manager deps:add hyprland waybar --required`
  `stow-manager deps:add hyprland rofi --optional`

- **Stow Package with Auto-Install**:
  `stow-manager -y stow hyprland`

- **Preview Stow Changes (Dry-Run)**:
  `stow-manager -n stow terminal`

- **Unstow & Delete Package**:
  `stow-manager pkg:remove hyprland`
