# Dotfiles Stow Manager (`stow-manager`)

A high-performance, zero-dependency, ISO C17 framework manager and dependency resolver for GNU Stow dotfiles packages.

## Features

- **Zero-Dependency ISO C17**: Lightweight, high-performance C binary compiled with `-std=c17`.
- **Symlink Unfolding & Collision Prevention**: Automatically unfolds directory symlinks to prevent Stow folding collisions.
- **Dependency & Plugin Resolution**: Auto-detects missing tools across distro package managers (`pacman`, `apt`, `dnf`, `apk`, `brew`) and Zsh plugins (`stow.registry`).
- **Conflict & Mutual Exclusion Management**: Auto-unstows conflicting dotfile profiles (e.g., `terminal` vs `headless`).
- **Standardized Command Namespaces**: Intuitive CRUD namespaces for package management (`pkg:*`), dependencies (`deps:*`), and configuration (`config:*`).
- **Configuration System**: Multi-repository management (`config set`, `config add`, `config remove`, `config show`).
- **Safety & Cleanup**: Signal handlers (`SIGINT`/`Ctrl+C`) perform atomic in-place cleanup on exit.

## Build & Installation

```bash
# Build binary
make

# Run unit and feature test suites
make test
make test-feature

# Install to ~/.local/bin
make install PREFIX=$HOME/.local

# Build static binary for release
make static
```

## Quick Start

```bash
# Scaffold a package & manage manifest dependencies
stow-manager pkg:create hyprland
stow-manager deps:add hyprland waybar --required
stow-manager deps:edit hyprland waybar --optional
stow-manager deps:show hyprland

# Stow one or multiple packages
stow-manager stow hyprland terminal

# List package stowed status ([STOWED], [PARTIAL], [UNSTOWED])
stow-manager pkg:list

# Check dependencies & broken symlinks
stow-manager check

# Preview changes without modifying disk (dry-run)
stow-manager diff terminal

# Manage dotfiles repository paths
stow-manager config add ~/dotfiles
stow-manager config show

# Remove package (unstows automatically before deletion)
stow-manager pkg:remove hyprland
```

## License

Licensed under the [GNU General Public License v3.0](LICENSE).
