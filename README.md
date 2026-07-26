# Dotfiles Stow Manager (`stow-manager`)

A high-performance, zero-dependency, ISO C17 framework manager and dependency resolver for GNU Stow dotfiles packages.

## Features

- **Zero-Dependency ISO C17**: Lightweight, high-performance C binary compiled with `-std=c17`.
- **Symlink Unfolding & Collision Prevention**: Automatically unfolds directory symlinks to prevent Stow folding collisions.
- **Dependency & Plugin Resolution**: Auto-detects missing tools across distro package managers (`pacman`, `apt`, `dnf`, `apk`, `brew`) and Zsh plugins (`stow.registry`).
- **Conflict & Mutual Exclusion Management**: Auto-unstows conflicting dotfile profiles (e.g., `terminal` vs `headless`).
- **Configuration System**: Multi-repository management (`config set`, `config add`, `config remove`, `config show`).
- **Safety & Cleanup**: Interrupt handlers (`SIGINT`/`Ctrl+C`) clean up temporary unfolding files on exit.

## Build & Installation

```bash
# Build binary
make

# Run C unit test suite
make test

# Install to ~/.local/bin
make install PREFIX=$HOME/.local

# Build static binary for release
make static
```

## Quick Start

```bash
# Stow a package
stow-manager stow terminal

# List package status
stow-manager list

# Check dependencies & broken symlinks
stow-manager check

# Preview changes without modifying disk (dry-run)
stow-manager diff terminal

# Manage dotfiles repository paths
stow-manager config add ~/dotfiles
stow-manager config show

# Manage package dependencies (.stowdeps)
stow-manager deps:add terminal tmux --optional
stow-manager deps:show terminal
```

## License

Licensed under the [GNU General Public License v3.0](LICENSE).
