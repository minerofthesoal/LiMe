# LiMe Installer UI Assets

This folder stores Qt UI assets for the installer demo build:

- `theme.qss`: styling for installer pages
- icons/background images for slideshow and branding
- translations (`*.qm`) for installer localization

The current repository uses a pure-Python UI, but this folder is now created so
ISO build scripts can mount/copy UI assets without failing on missing paths.
