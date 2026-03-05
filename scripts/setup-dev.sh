#!/bin/bash
# LiMe development environment setup for Arch-based systems
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

log() { echo "[lime-dev] $*"; }
warn() { echo "[lime-dev][warn] $*" >&2; }

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<USAGE
Usage: $0 [--no-python-deps]
USAGE
  exit 0
fi

NO_PY_DEPS="false"
if [[ "${1:-}" == "--no-python-deps" ]]; then
  NO_PY_DEPS="true"
fi

if ! command -v pacman >/dev/null 2>&1; then
  warn "pacman not found. This script is for Arch-based systems."
  exit 1
fi

log "Installing system dependencies"
sudo pacman -S --needed --noconfirm \
  base-devel git meson ninja pkgconf archiso grub efibootmgr \
  gtk3 glib2 clutter

if [[ "$NO_PY_DEPS" != "true" ]]; then
  log "Installing Python dependencies"
  python3 -m pip install --user --upgrade pip wheel setuptools
  python3 -m pip install --user PyQt5 requests psutil pyyaml
fi

log "Ensuring script executability"
chmod +x "$PROJECT_ROOT"/*.sh "$PROJECT_ROOT"/scripts/*.sh || true

log "Creating standard build directories"
mkdir -p "$PROJECT_ROOT"/out "$PROJECT_ROOT"/build "$PROJECT_ROOT"/packaging

log "Development environment setup complete"
