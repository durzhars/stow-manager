# Dotfiles Stow Manager (`stow-manager`)

**Usage**: `stow-manager [options] <command> [arguments]`

## Options

- **`-d, --dotfiles-dir`** `<path>` : Set dotfiles repository directory for current command
- **`-t, --target-dir`** `<path>`   : Set target home directory for current command
- **`-y, --install`**             : Auto-confirm installation of missing dependencies/plugins
- **`-n, --dry-run`**             : Dry-run mode (preview changes without modifying disk)
- **`-h, --help`**                : Show this help menu

## Configuration Commands (`config:*`)

- **`config show`**                        : Display current configuration settings & paths
- **`config set [dotfiles|target]`** `<path>` : Set primary dotfiles/target directory in config file
- **`config add`** `<path>`                  : Add an additional dotfiles repository directory
- **`config remove`** `<path>`               : Remove a dotfiles repository directory from config

## Package Management Commands (`pkg:*`)

- **`pkg:create`** `<name>`             : Scaffold a new Stow package directory & manifest (alias: `make:package`)
- **`pkg:remove`** `<name>`             : Unstow and remove a Stow package directory (alias: `remove:package`)
- **`pkg:list`**                       : List all packages and stowed status (alias: `list`)

## Dependency Management Commands (`deps:*`)

- **`deps:add`** `<pkg> <dep> [--opt]`   : Add a dependency/conflict to package manifest
- **`deps:edit`** `<pkg> <dep> <type>`   : Edit dependency type (`--required`, `--optional`, `--conflict`)
- **`deps:remove`** `<pkg> <dep>`        : Remove a dependency from package manifest
- **`deps:show`** `<pkg>`               : Display package manifest contents

## Stow & Deployment Commands

- **`stow`** `<pkg ...>`                 : Stow one or multiple packages with auto conflict resolution
- **`unstow`** `<pkg ...>`               : Unstow one or multiple packages
- **`restow`** `<pkg ...>`               : Restow one or multiple packages
- **`diff`** `[pkg ...]`                 : Preview symlink creations, conflict backups, and missing deps
- **`check`** `[pkg ...]`                : Detect missing dependencies, plugins & broken symlinks
- **`check-symlinks`**                 : Scan for broken repo symlinks & unmanaged target symlinks
- **`scan`** `[pkg ...]`                 : Recursively scan package files to auto-detect dependencies
- **`fix-conflicts`**                  : Unfold directory symlinks & resolve conflicts
- **`all`**                            : Stow all packages
- **`help`**                           : Show this help menu
