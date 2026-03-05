#!/bin/bash
# LiMe OS ISO Builder
# Main script to build the LiMe OS ISO image

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="${PROJECT_ROOT}/build"
OUT_DIR="${PROJECT_ROOT}/out"
ARCHISO_CONFIG="${PROJECT_ROOT}/archiso"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."

    local missing_tools=()

    for tool in pacman archiso mkarchiso mksquashfs mksquashfs grub; do
        if ! command -v "$tool" &> /dev/null; then
            missing_tools+=("$tool")
        fi
    done

    if [ ${#missing_tools[@]} -gt 0 ]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        log_info "Install with: sudo pacman -S archiso grub"
        exit 1
    fi

    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi

    log_info "✓ All prerequisites satisfied"
}

# Prepare build environment
prepare_build() {
    log_info "Preparing build environment..."

    # Clean previous builds
    [ -d "$BUILD_DIR" ] && rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR" "$OUT_DIR"

    # Copy archiso config
    cp -r "$ARCHISO_CONFIG" "$BUILD_DIR/archiso"

    log_info "✓ Build environment prepared"
}

# Generate version string
get_version() {
    local version_file="${PROJECT_ROOT}/VERSION"
    if [ -f "$version_file" ]; then
        cat "$version_file"
    else
        echo "0.1.0"
    fi
}

# Build the ISO
build_iso() {
    log_info "Building LiMe OS ISO..."

    local version=$(get_version)
    local iso_name="lime-os-${version}-x86_64.iso"

    cd "$BUILD_DIR/archiso"

    # Run mkarchiso
    mkarchiso -v -o "$OUT_DIR" .

    # Rename the output
    if [ -f "$OUT_DIR/archlinux-"*.iso ]; then
        mv "$OUT_DIR/archlinux-"*.iso "$OUT_DIR/$iso_name"
        log_info "✓ ISO created: $iso_name"
    else
        log_error "ISO generation failed"
        exit 1
    fi
}

# Build installer application
build_installer() {
    log_info "Building LiMe Installer..."

    local installer_src="${PROJECT_ROOT}/lime-installer/src"
    local installer_build="${BUILD_DIR}/installer-build"

    mkdir -p "$installer_build"

    # Build Python-based installer
    cd "$installer_src"

    if [ -f "setup.py" ]; then
        python3 setup.py build --build-base "$installer_build"
        log_info "✓ Installer built"
    else
        log_warn "Installer setup.py not found, skipping..."
    fi
}

# Build DE (Cinnamon fork)
build_de() {
    log_info "Building LiMe Desktop Environment..."

    local de_src="${PROJECT_ROOT}/lime-de/src"
    local de_build="${BUILD_DIR}/de-build"

    if [ ! -d "$de_src" ]; then
        log_warn "LiMe DE source not found, skipping..."
        return
    fi

    mkdir -p "$de_build"

    cd "$de_src"

    if [ -f "meson.build" ]; then
        meson "$de_build"
        ninja -C "$de_build"
        log_info "✓ LiMe DE built"
    elif [ -f "configure" ]; then
        ./configure --prefix=/usr --build-dir="$de_build"
        make -C "$de_build"
        log_info "✓ LiMe DE built"
    else
        log_warn "LiMe DE build system not recognized"
    fi
}

# Build AI module
build_ai() {
    log_info "Building LiMe AI Module..."

    local ai_src="${PROJECT_ROOT}/lime-ai/src"

    if [ ! -d "$ai_src" ]; then
        log_warn "LiMe AI source not found, skipping..."
        return
    fi

    cd "$ai_src"

    if [ -f "setup.py" ]; then
        python3 setup.py build --build-base "${BUILD_DIR}/ai-build"
        log_info "✓ AI module built"
    elif [ -f "requirements.txt" ]; then
        log_info "AI module is Python package, will be installed at runtime"
    fi
}

# Summary
print_summary() {
    log_info "Build completed!"
    echo ""
    echo "========================================="
    echo "LiMe OS Build Summary"
    echo "========================================="
    echo "Output directory: $OUT_DIR"
    echo "Version: $(get_version)"
    echo ""
    echo "ISO Image:"
    ls -lh "$OUT_DIR"/*.iso 2>/dev/null || echo "  No ISO found"
    echo ""
    echo "Next steps:"
    echo "  1. Write ISO to USB: sudo dd if=$OUT_DIR/lime-os-*.iso of=/dev/sdX bs=4M status=progress"
    echo "  2. Boot from USB and run the installer"
    echo "========================================="
}

# Main execution
main() {
    log_info "Starting LiMe OS build process..."
    echo ""

    check_prerequisites
    prepare_build

    # Build components in order
    build_de
    build_ai
    build_installer
    build_iso

    print_summary
}

# Run main function
main "$@"
