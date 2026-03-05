#!/bin/bash
# LiMe OS Development Setup
# Run this to set up a development environment

set -e

echo "Setting up LiMe OS development environment..."

# Install required packages
echo "Installing prerequisites..."
sudo pacman -Syu --noconfirm
sudo pacman -S --noconfirm \
    base-devel \
    git \
    meson \
    ninja \
    pkg-config \
    gnome-common \
    archiso \
    grub \
    efibootmgr \
    gtk3 \
    clutter \
    muffin

# Install Python dependencies
echo "Installing Python dependencies..."
pip install --user \
    PyQt5 \
    transformers \
    torch \
    pyyaml

# Make scripts executable
echo "Making scripts executable..."
chmod +x build-tools/scripts/*.sh

# Create necessary directories
echo "Creating directories..."
mkdir -p out build packaging/{lime-cinnamon,lime-ai-app,lime-installer,lime-welcome,lime-themes}

# Initialize git hooks if in git repo
if [ -d .git ]; then
    echo "Setting up git hooks..."
    mkdir -p .git/hooks

    # Pre-commit hook to check code formatting
    cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
echo "Running pre-commit checks..."
# Add linting/formatting checks here
EOF
    chmod +x .git/hooks/pre-commit
fi

echo "✓ Development environment ready!"
echo ""
echo "Next steps:"
echo "  1. Review docs/BUILD.md for build instructions"
echo "  2. Run: sudo ./build-tools/scripts/build-iso.sh"
echo "  3. Test in VM or physical hardware"
echo ""
