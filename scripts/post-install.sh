#!/bin/bash
# LiMe post-install configurator for an installed target root
set -euo pipefail

TARGET_ROOT="${1:-/mnt/lime-os}"
LIME_USER="${2:-limeuser}"

log() { echo "[lime-post] $*"; }
warn() { echo "[lime-post][warn] $*" >&2; }

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<USAGE
Usage: $0 [target_root] [username]
Example: $0 /mnt/lime-os ceo
USAGE
  exit 0
fi

if [[ ! -d "$TARGET_ROOT" ]]; then
  warn "Target root not found: $TARGET_ROOT"
  exit 1
fi

run_in_target() {
  arch-chroot "$TARGET_ROOT" /bin/bash -lc "$1"
}

if ! command -v arch-chroot >/dev/null 2>&1; then
  warn "arch-chroot missing (install arch-install-scripts)"
  exit 1
fi

log "Enabling core services"
run_in_target "systemctl enable NetworkManager || true"
run_in_target "systemctl enable lightdm || true"

log "Creating user $LIME_USER if absent"
run_in_target "id '$LIME_USER' >/dev/null 2>&1 || useradd -m -G wheel,audio,video,storage -s /bin/bash '$LIME_USER'"
run_in_target "sed -i 's/^# %wheel ALL=(ALL:ALL) NOPASSWD: ALL/%wheel ALL=(ALL:ALL) NOPASSWD: ALL/' /etc/sudoers"

log "Ensuring LiMe directories"
run_in_target "mkdir -p /usr/share/lime/{themes,icons,wallpapers}"
run_in_target "mkdir -p /etc/skel/.config/lime /etc/skel/.local/share/lime"

log "Regenerating locale and clocks"
run_in_target "locale-gen || true"
run_in_target "hwclock --systohc || true"

log "Post-install steps completed"
