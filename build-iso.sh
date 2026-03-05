#!/bin/bash
# LiMe OS ISO Builder
# Main script to build the LiMe OS ISO image

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="${PROJECT_ROOT}/build"
OUT_DIR="${PROJECT_ROOT}/out"
ARCHISO_CONFIG="${PROJECT_ROOT}/archiso"
GENERATED_ARCHISO="${BUILD_DIR}/generated-archiso"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_prerequisites() {
    log_info "Checking prerequisites..."

    local missing_tools=()
    local required=(mkarchiso)
    local optional=(pacman grub mksquashfs)

    for tool in "${required[@]}"; do
        if ! command -v "$tool" &>/dev/null; then
            missing_tools+=("$tool")
        fi
    done

    for tool in "${optional[@]}"; do
        if ! command -v "$tool" &>/dev/null; then
            log_warn "Optional tool missing: $tool"
        fi
    done

    if [ ${#missing_tools[@]} -gt 0 ]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        exit 1
    fi

    log_info "✓ Prerequisite check complete"
}

ensure_archiso_profile() {
    local target_profile="$GENERATED_ARCHISO"

    if [ -d "$ARCHISO_CONFIG" ]; then
        target_profile="$ARCHISO_CONFIG"
    else
        log_warn "archiso profile not found; creating a minimal profile at $target_profile"
        mkdir -p "$target_profile/airootfs" "$target_profile/syslinux"
    fi

    if [ -d "$ARCHISO_CONFIG" ]; then
        return
    fi

    cat > "$target_profile/packages.x86_64" <<'PKGS'
base
linux
linux-firmware
PKGS

    cat > "$target_profile/profiledef.sh" <<'PROFILE'
#!/usr/bin/env bash
iso_name="lime-os"
iso_label="LIME_$(date +%Y%m)"
iso_publisher="LiMe <https://example.invalid>"
iso_application="LiMe Live/Rescue CD"
install_dir="arch"
buildmodes=('iso')
bootmodes=('bios.syslinux.x86_64' 'uefi-x64.systemd-boot')
arch="x86_64"
pacman_conf="pacman.conf"
PROFILE

    cat > "$target_profile/pacman.conf" <<'PACMAN'
[options]
HoldPkg = pacman glibc
PACMAN

    cat > "$target_profile/syslinux/syslinux.cfg" <<'SYSLINUX'
DEFAULT arch
LABEL arch
    LINUX /%INSTALL_DIR%/boot/x86_64/vmlinuz-linux
SYSLINUX
}

prepare_build() {
    log_info "Preparing build environment..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR" "$OUT_DIR"

    ensure_archiso_profile

    local profile_source="$ARCHISO_CONFIG"
    if [ ! -d "$profile_source" ]; then
        profile_source="$GENERATED_ARCHISO"
    fi

    cp -r "$profile_source" "$BUILD_DIR/archiso"

    log_info "✓ Build environment prepared"
}

get_version() {
    local version_file="${PROJECT_ROOT}/VERSION"
    if [ -f "$version_file" ]; then
        cat "$version_file"
    else
        echo "0.1.1-prealpha"
    fi
}

build_iso() {
    log_info "Building LiMe OS ISO..."
    local version
    version=$(get_version)
    local iso_name="lime-os-${version}-x86_64.iso"

    mkarchiso -v -o "$OUT_DIR" "$BUILD_DIR/archiso"

    local generated_iso
    generated_iso=$(find "$OUT_DIR" -maxdepth 1 -type f -name "*.iso" | head -n 1 || true)

    if [ -n "$generated_iso" ]; then
        mv "$generated_iso" "$OUT_DIR/$iso_name"
        log_info "✓ ISO created: $iso_name"
    else
        log_error "ISO generation failed: no ISO artifact found"
        exit 1
    fi
}

main() {
    log_info "Starting LiMe OS build process..."
    check_prerequisites
    prepare_build
    build_iso
    log_info "Done. Output directory: $OUT_DIR"
}

main "$@"
