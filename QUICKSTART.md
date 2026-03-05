# LiMe OS Quick Start Guide

## What is LiMe OS?

LiMe OS is a custom Linux distribution based on Arch Linux with:
- **LiMe Desktop Environment** - A modern Cinnamon fork
- **Integrated AI Assistant** - Built-in AI with LLM support
- **Graphical Installer** - User-friendly setup wizard
- **Custom Branding** - Unique visual identity

## Quick Start for Developers

### 1. Set Up Development Environment

```bash
chmod +x scripts/setup-dev.sh
./scripts/setup-dev.sh
```

### 2. Build the ISO

```bash
sudo ./build-tools/scripts/build-iso.sh
```

The bootable ISO will be created in `out/lime-os-*.iso`

### 3. Test in Virtual Machine

```bash
# Create VM in VirtualBox/QEMU
# Boot from ISO
# Follow the graphical installer
```

## Project Structure Overview

```
lime-os/
├── README.md                 # Project overview
├── LICENSE                   # GPL-2.0 license
├── VERSION                   # Version number
├── .gitignore               # Git ignore rules
│
├── archiso/                 # ISO build configuration
│   ├── profiles.conf        # Package lists
│   ├── airootfs/            # Files included in ISO
│   └── configs/             # Boot configuration
│
├── lime-de/                 # Desktop Environment (Cinnamon fork)
│   ├── src/                 # C source code
│   │   ├── main.c           # Entry point
│   │   ├── lime-theme-manager.c
│   │   ├── lime-ai-integration.c
│   │   └── shell-util.c
│   └── meson.build          # Build system
│
├── lime-installer/          # Graphical installer
│   ├── src/
│   │   ├── installer.py     # Qt5 application
│   │   └── setup.py         # Python packaging
│   └── ui/                  # UI components
│
├── lime-ai/                 # AI assistant module
│   ├── src/
│   │   ├── lime_ai_app.py   # Main Qt5 app
│   │   ├── model_manager_cli.py
│   │   └── setup.py
│   └── models/              # Model management
│
├── packaging/               # Arch PKGBUILD files
│   ├── lime-cinnamon/
│   ├── lime-ai-app/
│   ├── lime-installer/
│   └── ...
│
├── build-tools/             # Build scripts
│   ├── scripts/
│   │   ├── build-iso.sh
│   │   ├── build-packages.sh
│   │   ├── post-install.sh
│   │   └── ...
│   └── templates/           # Configuration templates
│
├── scripts/                 # Utility scripts
│   └── setup-dev.sh
│
├── config/                  # System configuration
│   └── skel/               # User skeleton files
│
└── docs/                   # Documentation
    ├── BUILD.md           # Build instructions
    └── DEVELOPMENT.md     # Development guide
```

## Key Files to Edit

### To customize packages:
`archiso/profiles.conf`

### To modify the DE:
`lime-de/src/`

### To change installer flow:
`lime-installer/src/installer.py`

### To add AI models:
`lime-ai/src/lime_ai_app.py` (line ~60)

### To change post-install:
`build-tools/scripts/post-install.sh`

## Building Components

### Just the DE
```bash
cd lime-de/src
meson setup build
meson compile -C build
```

### Just the Installer
```bash
cd lime-installer/src
python -m build
```

### Just the AI App
```bash
cd lime-ai/src
python -m build
```

### Full ISO
```bash
sudo ./build-tools/scripts/build-iso.sh
```

## Testing

### Test installer locally
```bash
python3 lime-installer/src/installer.py
```

### Test AI app locally
```bash
python3 lime-ai/src/lime_ai_app.py
```

### Test ISO in VM
1. Write to USB: `sudo dd if=out/lime-os-*.iso of=/dev/sdX bs=4M`
2. Boot VM from USB
3. Run through installer
4. Test DE and AI app

## Documentation

- **BUILD.md** - Detailed build instructions
- **DEVELOPMENT.md** - Architecture and development guide
- **README.md** - Project overview

## Supported Models

### Qwen (Alibaba)
- Qwen 2B (2GB)
- Qwen 7B (7GB)
- Qwen 14B (14GB)

### Nix-AI
- Nix Small (3GB)
- Nix Medium (7GB)

### Others
- Mistral 7B (14GB)
- Neural Chat 7B

Models download from HuggingFace automatically.

## Troubleshooting

### Build fails with missing packages
```bash
sudo pacman -S [package-name]
```

### PyQt5 import errors
```bash
pip install PyQt5
```

### ISO generation fails
```bash
pacman -S archiso
```

### Need to rebuild from scratch
```bash
rm -rf build out
sudo ./build-tools/scripts/build-iso.sh
```

## Next Steps

1. Review the BUILD.md for detailed instructions
2. Set up the development environment
3. Build the ISO
4. Test in a virtual machine
5. Customize packages/themes as needed

## Contributing

Contributions welcome! Areas needing work:

- [ ] Polish DE UI
- [ ] Complete installer logic
- [ ] Add more AI models
- [ ] Performance optimizations
- [ ] Documentation improvements
- [ ] Theme designs
- [ ] Security hardening

## License

LiMe OS is licensed under GNU GPL v2, combining:
- Arch Linux (GPL)
- Cinnamon (GPL)
- Custom components (GPL)

See LICENSE file for details.

## Resources

- [Archiso Wiki](https://wiki.archlinux.org/title/Archiso)
- [Cinnamon GitHub](https://github.com/linuxmint/cinnamon)
- [Arch Linux Wiki](https://wiki.archlinux.org)
- [PyQt5 Docs](https://riverbankcomputing.com/software/pyqt/)
- [HuggingFace Models](https://huggingface.co/models)

---

For questions or issues, refer to the documentation or open an issue on GitHub.
