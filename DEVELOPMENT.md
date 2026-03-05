# LiMe OS Development Guide

## Architecture Overview

LiMe OS is built on three main components:

1. **Arch Linux Base** - Minimal, fast, rolling release
2. **LiMe Desktop Environment** - Fork of Cinnamon with enhancements
3. **LiMe AI Assistant** - Integrated LLM application

```
┌─────────────────────────────────────┐
│     LiMe OS ISO/Distribution        │
├─────────────────────────────────────┤
│  LiMe AI App | LiMe Installer       │
│  LiMe Themes | LiMe Welcome          │
├─────────────────────────────────────┤
│  LiMe Desktop Environment (DE)       │
│  - Cinnamon fork                    │
│  - Custom theming                   │
│  - AI integration                   │
├─────────────────────────────────────┤
│  Arch Linux Base                    │
│  - Linux kernel                     │
│  - Core utilities                   │
│  - Package manager (pacman)         │
├─────────────────────────────────────┤
│  Hardware (x86_64)                  │
└─────────────────────────────────────┘
```

## LiMe Desktop Environment (DE)

### Purpose

A modern, lightweight desktop environment based on Cinnamon with:
- Better customization options
- Integrated AI assistant
- Modern UI components
- Works with all Linux applications

### Key Components

- **Theme Manager** (`lime-theme-manager.c`)
  - Manages UI themes
  - Custom accent colors
  - Font management
  - Transparency/effects

- **AI Integration** (`lime-ai-integration.c`)
  - D-Bus service connection
  - AI assistant sidebar
  - Quick queries
  - System integration

- **Shell Utilities** (`shell-util.c`)
  - App launching
  - System information
  - File management

### Build System

Uses Meson (modern replacement for Autotools):

```
meson setup build
meson compile -C build
meson install -C build
```

### Key Files

- `main.c` - Entry point and main loop
- `cinnamon-global.h/c` - Global state management
- `meson.build` - Build configuration

## LiMe Installer

### Purpose

Graphical installation wizard for LiMe OS with:
- Disk selection and partitioning
- User account creation
- Locale and keyboard configuration
- Package selection
- AI model pre-installation
- Automated post-installation

### Architecture

Built with PyQt5 for cross-platform compatibility:

- **Wizard Flow**
  1. Welcome
  2. Disk Selection
  3. Partitioning
  4. Locale/Keyboard
  5. User Creation
  6. Package Selection
  7. AI Models
  8. Review
  9. Installation
  10. Complete

- **Installation Thread**
  - Runs in background
  - Updates progress bar
  - Handles errors gracefully

### Key Classes

- `LiMeInstaller` - Main window
- `InstallationThread` - Background installation worker
- Various page/dialog classes for each step

## LiMe AI App

### Purpose

Standalone AI assistant with:
- Multiple LLM model support
- Model management (download/delete)
- Chat interface
- System integration
- Configurable temperature/tokens

### Supported Models

Downloaded from HuggingFace:

**Qwen Series** (Alibaba)
- Qwen 2B (small, ~2GB)
- Qwen 7B (medium, ~7GB)
- Qwen 14B (large, ~14GB)

**Nix-AI Series**
- Nix Small
- Nix Medium

**Other Models**
- Mistral 7B
- Neural Chat 7B
- Customizable to add any HF model

### Architecture

- `ModelManager` - Handles model downloads and management
- `LLMInferenceThread` - Background inference
- `LiMeAIApp` - Main Qt5 application
- `ModelManagerDialog` - Model management UI

### Model Storage

Models stored in: `~/.lime/ai/models/`

Configuration: `~/.lime/ai/models/models.json`

## ISO Builder

### Purpose

Automates the complete build process:

1. Checks system requirements
2. Prepares build environment
3. Builds LiMe DE from source
4. Builds AI module
5. Packages installer
6. Generates bootable ISO

### Key Scripts

- `build-iso.sh` - Main build orchestrator
- `build-packages.sh` - Builds individual packages
- `post-install.sh` - Post-installation configuration

### Output

- `out/lime-os-X.X.X-x86_64.iso` - Bootable image
- `out/packages/*.pkg.tar.zst` - Package files

## Package Management

### PKGBUILD Files

Each component has a PKGBUILD for Arch Linux:

- `lime-cinnamon` - Main DE package
- `lime-ai-app` - AI application
- `lime-installer` - Installation wizard
- `lime-welcome` - Welcome app
- `lime-themes` - Theme package
- `lime-control-center` - System settings

### Building Packages

```bash
cd packaging/[package-name]
makepkg -s  # Build with dependencies
pacman -U [package].pkg.tar.zst  # Install
```

## Configuration Files

### GSettings (Dconf)

- `org.cinnamon.gschema.xml` - Settings schema
- `org.cinnamon` - Settings namespace

### Default Configuration

- `config/skel/` - User skeleton files
- `config/lime/` - LiMe-specific config
- `config/desktop/` - Desktop shortcuts

## Development Tips

### Adding a New Theme

1. Create theme directory under `/usr/share/lime/themes/mytheme/`
2. Add `gtk-3.0/gtk.css` for GTK styling
3. Add `cinnamon/style.css` for desktop styles
4. Register in theme manager

### Adding New LLM Models

Edit `lime-ai/src/lime_ai_app.py`:

```python
LLMModel("Model Name", "provider", "size", "huggingface/model-id")
```

### Extending the Installer

Add new pages to `lime-installer/src/installer.py`:

```python
def create_custom_page(self):
    widget = QWidget()
    layout = QVBoxLayout()
    # ... add UI elements
    self.pages.addWidget(widget)
```

### Modifying the DE

Edit C source files in `lime-de/src/`:

1. Make changes
2. Rebuild: `meson compile -C build`
3. Install: `meson install -C build`
4. Test in new session

## Testing

### Unit Testing

```bash
# For Python components
python -m pytest

# For C components
make test
```

### Integration Testing

1. Test in virtual machine
2. Boot from ISO
3. Follow installer through all steps
4. Verify AI models work
5. Check DE functionality

### Performance Testing

- Profile startup time
- Monitor memory usage
- Benchmark AI inference
- Test thermal behavior

## Common Development Tasks

### Create a New Release

1. Update VERSION file
2. Update PKGBUILD pkgrel values
3. Commit changes
4. Tag release: `git tag v0.2.0`
5. Run: `./build-tools/scripts/build-iso.sh`
6. Push to GitHub

### Debug Installer Issues

```bash
python3 -u lime-installer/src/installer.py  # Unbuffered output
```

### Debug DE Issues

```bash
./build/cinnamon --debug
journalctl -u cinnamon -f  # Follow logs
```

### Test AI Models Offline

```bash
python3 lime-ai/src/lime_ai_app.py --offline
```

## Performance Optimization

### For DE

- Use hardware accelerated rendering
- Cache theme assets
- Lazy load applications

### For AI

- Quantized models (smaller, faster)
- CPU offloading
- Batch inference

### For ISO

- Remove unnecessary packages
- Compress filesystem
- Use modular kernel

## Security Considerations

1. Validate all user input
2. Use secure D-Bus communication
3. Prevent privilege escalation
4. Use sandboxing for DE
5. Keep dependencies updated

## Future Enhancements

- [ ] Web app support
- [ ] Plugin system
- [ ] Advanced AI features (voice, image)
- [ ] Cloud sync
- [ ] Custom app store
- [ ] Better hardware detection

