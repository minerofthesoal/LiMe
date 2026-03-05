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
