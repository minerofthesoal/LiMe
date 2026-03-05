#!/bin/bash
# LiMe package build helper
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PACKAGES_DIR="$PROJECT_ROOT/packaging"
OUT_DIR="$PROJECT_ROOT/out/packages"

log() { echo "[lime-packages] $*"; }
warn() { echo "[lime-packages][warn] $*" >&2; }

usage() {
  cat <<USAGE
Usage: $0 [package1 package2 ...]
If no package names are provided, a default LiMe package set is attempted.
USAGE
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

mkdir -p "$OUT_DIR"

DEFAULT_PACKAGES=(
  lime-cinnamon
  lime-ai-app
  lime-installer
  lime-welcome
  lime-themes
  lime-control-center
)

if [[ $# -gt 0 ]]; then
  PACKAGES=("$@")
else
  PACKAGES=("${DEFAULT_PACKAGES[@]}")
fi

if ! command -v makepkg >/dev/null 2>&1; then
  warn "makepkg not found; install base-devel on Arch"
  exit 1
fi

for pkg in "${PACKAGES[@]}"; do
  PKG_DIR="$PACKAGES_DIR/$pkg"
  if [[ ! -d "$PKG_DIR" ]]; then
    warn "Package directory missing: $PKG_DIR"
    continue
  fi

  log "Building $pkg"
  pushd "$PKG_DIR" >/dev/null
  rm -rf src pkg *.pkg.tar.zst *.pkg.tar.xz || true

  if ! makepkg -s --noconfirm; then
    warn "Build failed for $pkg"
    popd >/dev/null
    continue
  fi

  shopt -s nullglob
  artifacts=(*.pkg.tar.zst *.pkg.tar.xz)
  shopt -u nullglob
  if [[ ${#artifacts[@]} -eq 0 ]]; then
    warn "No package artifact found for $pkg"
  else
    cp -v "${artifacts[@]}" "$OUT_DIR/"
  fi
  popd >/dev/null

done

log "Done. Package outputs (if any) are in $OUT_DIR"
ls -lh "$OUT_DIR" 2>/dev/null || true
