# LiMe OS Build Documentation

## Quick Start

1. Clone this repository
2. Install dependencies
3. Run the build script

```bash
sudo ./build-tools/scripts/build-iso.sh
```

The ISO will be created in the `out/` directory.

## System Requirements

- Arch Linux or compatible distribution
- Root/sudo access
- 10GB+ free disk space
- 4GB+ RAM recommended

### Dependencies

Install the required packages:

```bash
sudo pacman -S archiso grub efibootmgr base-devel meson ninja
pip install PyQt5 transformers torch
```

## Project Structure

### archiso/
Configuration for the ISO build using archiso. Contains:
- `profiles.conf` - Package lists and profiles
- `airootfs/` - Files to include in the ISO
- `configs/` - Build configuration

### lime-de/
Cinnamon fork implementation (LiMe Desktop Environment):
- `src/` - C source code
  - `main.c` - Main entry point
  - `lime-theme-manager.c` - Theme customization
  - `lime-ai-integration.c` - AI integration
- `build/` - Build artifacts
- `meson.build` - Build configuration

### lime-installer/
Graphical installation wizard:
- `src/installer.py` - Main Qt5 application
- `ui/` - UI components
- `scripts/` - Installation helpers

### lime-ai/
AI assistant application:
- `src/lime_ai_app.py` - Main application
- `models/` - Model management
- `config/` - Configuration files

### packaging/
PKGBUILD files for Arch packages:
- `lime-cinnamon/` - DE package
- `lime-ai-app/` - AI app package
- `lime-installer/` - Installer package
- `lime-welcome/` - Welcome/first-run app
- `lime-themes/` - Theme package

## Building Components

### Build the ISO

```bash
sudo ./build-tools/scripts/build-iso.sh
```

This will:
1. Check prerequisites
2. Build the LiMe DE from source
3. Compile the AI module
4. Package the installer
5. Generate the final ISO

### Build Individual Packages

```bash
./build-tools/scripts/build-packages.sh
```

Or build specific packages:

```bash
cd packaging/lime-cinnamon
makepkg -s  # Build with dependencies
```

### Build Just the DE

```bash
cd lime-de/src
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
```

### Build the Installer

```bash
cd lime-installer/src
python -m build
pip install dist/*.whl
```

### Build the AI App

```bash
cd lime-ai/src
python -m build
pip install dist/*.whl
```

## Installation

### From ISO

1. Write ISO to USB:
```bash
sudo dd if=out/lime-os-*.iso of=/dev/sdX bs=4M status=progress
```

2. Boot from USB
3. Follow the graphical installer
4. Select installation options (packages, AI models, etc.)
5. Reboot

### Manual Installation

The build scripts can also perform direct installation on a running Arch system:

```bash
sudo ./build-tools/scripts/post-install.sh /
```

## Customization

### Theming

Edit `lime-de/src/lime-theme-manager.c` to add custom themes.

Theme files should be placed in `/usr/share/lime/themes/`

### AI Models

Edit `lime-ai/src/lime_ai_app.py` to add or remove available models.

Available by default:
- Qwen 2B, 7B, 14B (from Alibaba)
- Nix-AI small, medium
- Mistral 7B
- Neural Chat 7B

### Packages

Edit `archiso/profiles.conf` to customize the default package list.

### Post-Installation

Customize `build-tools/scripts/post-install.sh` to run custom scripts after installation.

## Development Workflow

### Working on the DE

1. Edit files in `lime-de/src/`
2. Rebuild: `meson compile -C build`
3. Test: `./build/cinnamon` or install and test with DE session

### Working on the Installer

1. Edit `lime-installer/src/installer.py`
2. Test locally: `python3 lime-installer/src/installer.py`
3. Rebuild package when ready

### Working on the AI App

1. Edit `lime-ai/src/lime_ai_app.py`
2. Test: `python3 lime-ai/src/lime_ai_app.py`
3. Install models for testing

## Testing

### ISO Testing

1. Create a virtual machine (VirtualBox/QEMU)
2. Boot from the ISO
3. Test the installer
4. Test the DE and AI app

### Package Testing

Test packages individually:

```bash
pacman -U out/packages/lime-cinnamon-*.pkg.tar.zst
```

## Troubleshooting

### Build Fails with "Missing dependencies"

Install the missing packages:
```bash
sudo pacman -S [package-name]
```

### ISO Generation Fails

Check that archiso is properly installed:
```bash
pacman -S archiso
mkarchiso --version
```

### Installer Won't Start

Ensure PyQt5 is installed:
```bash
pip install PyQt5
```

### AI Module Errors

Check that PyTorch is available and models are downloaded:
```bash
python3 -c "import torch; print(torch.__version__)"
```

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

LiMe OS combines components under GPL-2.0:
- Cinnamon (GPL)
- Arch Linux (GPL)
- Custom components (GPL)

See LICENSE file for details.

## Resources

- [Archiso Documentation](https://wiki.archlinux.org/title/Archiso)
- [Cinnamon Project](https://github.com/linuxmint/cinnamon)
- [Arch Linux Wiki](https://wiki.archlinux.org)
- [PKGBUILD Documentation](https://wiki.archlinux.org/title/Creating_packages)
- [PyQt5 Documentation](https://www.riverbankcomputing.com/static/Docs/PyQt5/)

## Contact

For issues and questions, open an issue on the GitHub repository.
