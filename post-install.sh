#!/bin/bash
# Post-installation script for LiMe OS
# Runs after the base system is installed to configure LiMe-specific settings

set -e

CHROOT_PATH="${1:-.}"
LIME_USER="${2:-limeuser}"

log_info() { echo "[*] $1"; }
log_error() { echo "[ERROR] $1" >&2; }

# Enter chroot environment
if [ "$CHROOT_PATH" != "." ]; then
    log_info "Entering chroot environment: $CHROOT_PATH"
fi

# Configure GRUB
log_info "Configuring GRUB bootloader..."
chroot "$CHROOT_PATH" /bin/bash -c "
    grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=LiMe
    grub-mkconfig -o /boot/grub/grub.cfg
"

# Enable LightDM display manager
log_info "Enabling display manager..."
chroot "$CHROOT_PATH" /bin/bash -c "
    systemctl enable lightdm
"

# Create default user if provided
if [ ! -z "$LIME_USER" ]; then
    log_info "Creating user: $LIME_USER"
    chroot "$CHROOT_PATH" /bin/bash -c "
        if ! id '$LIME_USER' &>/dev/null; then
            useradd -m -G wheel,audio,video,storage -s /bin/bash '$LIME_USER'
            echo 'User $LIME_USER created'
        fi
    "
fi

# Install LiMe-specific packages
log_info "Installing LiMe packages..."
chroot "$CHROOT_PATH" /bin/bash -c "
    pacman -Sy --noconfirm \
        lime-cinnamon \
        lime-ai-app \
        lime-installer \
        lime-welcome \
        lime-themes

    # Optional: Install development tools
    # pacman -Sy --noconfirm base-devel gcc make cmake
"

# Create LiMe skeleton directories
log_info "Setting up user skeleton..."
chroot "$CHROOT_PATH" /bin/bash -c "
    mkdir -p /etc/skel/.config/lime
    mkdir -p /etc/skel/.local/share/lime

    # Copy default configs
    cp -r /usr/share/lime/default-config/* /etc/skel/.config/lime/ || true
"

# Configure LiMe themes
log_info "Setting up themes..."
chroot "$CHROOT_PATH" /bin/bash -c "
    mkdir -p /usr/share/lime/themes
    mkdir -p /usr/share/lime/icons
    mkdir -p /usr/share/lime/wallpapers
"

# Enable services
log_info "Enabling system services..."
chroot "$CHROOT_PATH" /bin/bash -c "
    systemctl enable NetworkManager
    systemctl enable sshd
"

# Configure locale and timezone
log_info "Final configuration..."
chroot "$CHROOT_PATH" /bin/bash -c "
    locale-gen
    hwclock --systohc
"

log_info "Post-installation complete!"
