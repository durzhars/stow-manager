# Dotfiles Stow Manager (stow-manager)

High-performance, zero-dependency, ISO C17 framework manager and dependency resolver for GNU Stow dotfiles packages.

## Features

- **ISO C17 Compliance**: Built with standard C libraries (`-std=c17`).
- **Zero Runtime Overhead**: Instantaneous execution (< 1 ms).
- **Directory Symlink Unfolding**: Automatically unfolds folder symlinks to prevent Stow folding collisions.
- **Dependency & Plugin Checker**: Auto-detects missing tools (`pacman`, `apt`, `dnf`, `brew`) and Zsh plugins.
- **Mutual Exclusions**: Resolves conflicting packages (e.g. `terminal` vs `headless`).
- **Artisan-Style CLI**: Manage `.stowdeps` package manifests directly from the terminal.

## Build & Install

```bash
# Build
make

# Run Unit Tests
make test

# Install to ~/.local/bin
make install PREFIX=$HOME/.local

# Static Build for Standalone Release
make static
```

## Usage

```bash
# Stow a package
stow-manager terminal

# Check package dependencies
stow-manager check

# List package status
stow-manager list

# Add package dependency
stow-manager deps:add terminal tmux --optional
```
