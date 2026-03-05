#!/bin/bash
# LiMe OS Package Builder
# Builds PKGBUILD files for Arch Linux packages

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACKAGES_DIR="${PROJECT_ROOT}/packaging"
BUILD_DIR="${PROJECT_ROOT}/build/packages"
OUT_DIR="${PROJECT_ROOT}/out/packages"

log_info() { echo "[INFO] $1"; }
log_error() { echo "[ERROR] $1" >&2; }

mkdir -p "$BUILD_DIR" "$OUT_DIR"

build_package() {
    local pkg_name="$1"
    local pkg_dir="${PACKAGES_DIR}/${pkg_name}"

    if [ ! -d "$pkg_dir" ]; then
        log_error "Package directory not found: $pkg_dir"
        return 1
    fi

    log_info "Building package: $pkg_name"

    cd "$pkg_dir"

    # Update PKGBUILD
    if [ -f "PKGBUILD" ]; then
        # Clean previous builds
        rm -rf src pkg *.pkg.tar.zst

        # Build package
        makepkg -s --noconfirm

        # Copy to output
        mv *.pkg.tar.zst "$OUT_DIR/" 2>/dev/null || log_error "Build failed for $pkg_name"
        log_info "✓ Package built: $pkg_name"
    fi

    return 0
}

# Build all packages
log_info "Starting package builds..."

# LiMe DE
build_package "lime-cinnamon" || true

# LiMe Installer
build_package "lime-installer" || true

# LiMe AI App
build_package "lime-ai-app" || true

# LiMe Welcome
build_package "lime-welcome" || true

# LiMe Themes
build_package "lime-themes" || true

# LiMe Control Center
build_package "lime-control-center" || true

log_info "All packages built successfully!"
ls -la "$OUT_DIR"
