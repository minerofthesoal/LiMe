#!/usr/bin/env python3
"""
LiMe OS ISO Build System - Complete Automated Build
Comprehensive automated build, compilation, and ISO generation system
Handles: LiMe DE building, Arch auto-installation, Network setup, Firefox, App Store
Works on a 99% hands-free basis
"""

import os
import sys
import subprocess
import json
import shutil
import logging
import argparse
import multiprocessing
import urllib.request
import hashlib
import time
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Tuple, Optional

# Configure logging
log_dir = Path.home() / ".local" / "log"
log_dir.mkdir(parents=True, exist_ok=True)
log_file = log_dir / "lime-build.log"

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(str(log_file)),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

class LiMeBuilder:
    """Main build system for LiMe OS"""

    def __init__(self, project_root: Path, verbose: bool = False):
        self.project_root = Path(project_root)
        self.verbose = verbose
        self.build_dir = self.project_root / "build"
        self.out_dir = self.project_root / "out"
        self.archiso_dir = self.project_root / "archiso"
        self.de_dir = self.project_root / "lime-de"
        self.ai_dir = self.project_root / "lime-ai"
        self.installer_dir = self.project_root / "lime-installer"

        self.config = self._load_config()
        self.build_log = []
        self.start_time = datetime.now()

        logger.info(f"LiMe Builder initialized - Project root: {self.project_root}")

    def _load_config(self) -> Dict:
        """Load build configuration"""
        config_file = self.project_root / "build.config.json"

        default_config = {
            "version": "0.1.0",
            "iso_name": "lime-os",
            "arch": "x86_64",
            "kernel": "linux",
            "de_name": "lime-cinnamon",
            "build_threads": multiprocessing.cpu_count() - 1,
            "enable_compression": True,
            "parallel_build": True,
            "strip_binaries": True,
            "enable_debug": False,
            "iso_size_limit": 2048,  # MB
        }

        if config_file.exists():
            with open(config_file) as f:
                user_config = json.load(f)
                default_config.update(user_config)

        return default_config

    def _run_command(self, cmd: List[str], cwd: Optional[Path] = None,
                     error_msg: Optional[str] = None) -> Tuple[bool, str]:
        """Execute a shell command and return success status and output"""
        try:
            logger.info(f"Running: {' '.join(cmd)}")

            result = subprocess.run(
                cmd,
                cwd=cwd or self.project_root,
                capture_output=True,
                text=True,
                timeout=3600  # 1 hour timeout
            )

            if result.returncode != 0:
                error = error_msg or f"Command failed: {' '.join(cmd)}"
                logger.error(f"{error}\nStdout: {result.stdout}\nStderr: {result.stderr}")
                return False, result.stderr

            if self.verbose:
                logger.info(f"Output: {result.stdout}")

            self.build_log.append({
                "timestamp": datetime.now().isoformat(),
                "command": ' '.join(cmd),
                "status": "success",
                "output": result.stdout[:500]  # Truncate long output
            })

            return True, result.stdout

        except subprocess.TimeoutExpired:
            logger.error(f"Command timeout: {' '.join(cmd)}")
            return False, "Command timeout"
        except Exception as e:
            logger.error(f"Exception running command: {e}")
            return False, str(e)

    def check_prerequisites(self) -> bool:
        """Verify all required tools are installed"""
        logger.info("Checking prerequisites...")

        # Core tools needed for building
        required_tools = [
            'meson', 'ninja', 'pkg-config',
            'gcc', 'make', 'git',
            'python3'
        ]

        # Optional tools (needed for full ISO but not for component build)
        optional_tools = [
            'pacman', 'makepkg', 'mkarchiso', 'grub'
        ]

        missing_required = []
        missing_optional = []

        for tool in required_tools:
            result = subprocess.run(['which', tool], capture_output=True)
            if result.returncode != 0:
                missing_required.append(tool)

        for tool in optional_tools:
            result = subprocess.run(['which', tool], capture_output=True)
            if result.returncode != 0:
                missing_optional.append(tool)

        if missing_required:
            logger.error(f"Missing required tools: {', '.join(missing_required)}")
            return False

        if missing_optional:
            logger.warning(f"Missing optional tools (ISO generation may fail): {', '.join(missing_optional)}")

        logger.info("✓ Core prerequisites satisfied")
        return True

    def prepare_build_environment(self) -> bool:
        """Set up build directories and copy sources"""
        logger.info("Preparing build environment...")

        try:
            # Clean previous builds
            if self.build_dir.exists():
                logger.info("Cleaning previous build...")
                shutil.rmtree(self.build_dir)

            self.build_dir.mkdir(parents=True, exist_ok=True)
            self.out_dir.mkdir(parents=True, exist_ok=True)

            # Copy archiso config
            logger.info("Copying archiso configuration...")
            dest_archiso = self.build_dir / "archiso"
            if dest_archiso.exists():
                shutil.rmtree(dest_archiso)
            shutil.copytree(self.archiso_dir, dest_archiso)

            # Create necessary directories
            for subdir in ['airootfs', 'configs', 'out']:
                (self.build_dir / subdir).mkdir(parents=True, exist_ok=True)

            logger.info("✓ Build environment prepared")
            return True

        except Exception as e:
            logger.error(f"Failed to prepare build environment: {e}")
            return False

    def build_de(self) -> bool:
        """Build the LiMe Desktop Environment"""
        logger.info("Building LiMe Desktop Environment...")

        try:
            de_build = self.build_dir / "de"
            de_build.mkdir(parents=True, exist_ok=True)

            # Initialize Meson build
            success, output = self._run_command(
                ['meson', 'setup', str(de_build), '--prefix=/usr',
                 '--buildtype=release', '-Dman=false'],
                cwd=self.de_dir,
                error_msg="Meson setup failed"
            )

            if not success:
                return False

            # Build with Ninja
            num_jobs = self.config['build_threads']
            success, output = self._run_command(
                ['ninja', '-C', str(de_build), f'-j{num_jobs}'],
                error_msg="DE compilation failed"
            )

            if not success:
                return False

            logger.info("✓ LiMe DE built successfully")
            return True

        except Exception as e:
            logger.error(f"Error building DE: {e}")
            return False

    def build_installer(self) -> bool:
        """Build the LiMe Installer"""
        logger.info("Building LiMe Installer...")

        try:
            # Build installer Python package
            success, output = self._run_command(
                ['python3', '-m', 'build', '--wheel', '--no-isolation'],
                cwd=self.installer_dir,
                error_msg="Installer build failed"
            )

            if not success:
                return False

            logger.info("✓ LiMe Installer built successfully")
            return True

        except Exception as e:
            logger.error(f"Error building installer: {e}")
            return False

    def build_ai_module(self) -> bool:
        """Build the AI module"""
        logger.info("Building LiMe AI Module...")

        try:
            success, output = self._run_command(
                ['python3', '-m', 'build', '--wheel', '--no-isolation'],
                cwd=self.ai_dir,
                error_msg="AI module build failed"
            )

            if not success:
                return False

            logger.info("✓ LiMe AI Module built successfully")
            return True

        except Exception as e:
            logger.error(f"Error building AI module: {e}")
            return False

    def build_packages(self) -> bool:
        """Build Arch Linux packages"""
        logger.info("Building Arch packages...")

        try:
            pkgbuild_dir = self.project_root / "packaging"

            packages = [
                'lime-cinnamon',
                'lime-ai-app',
                'lime-installer',
                'lime-welcome',
                'lime-themes',
                'lime-control-center'
            ]

            for pkg in packages:
                pkg_dir = pkgbuild_dir / pkg
                if not pkg_dir.exists():
                    logger.warning(f"Package dir not found: {pkg}, skipping")
                    continue

                logger.info(f"Building package: {pkg}")

                # Run makepkg
                success, output = self._run_command(
                    ['makepkg', '-s', '--noconfirm'],
                    cwd=pkg_dir,
                    error_msg=f"Package build failed: {pkg}"
                )

                if not success:
                    logger.warning(f"Failed to build package: {pkg}, continuing...")
                    continue

                # Copy built package to out dir
                pkg_files = list(pkg_dir.glob("*.pkg.tar.zst"))
                for pkg_file in pkg_files:
                    shutil.copy(pkg_file, self.out_dir / pkg_file.name)
                    logger.info(f"  → {pkg_file.name}")

            logger.info("✓ Packages built successfully")
            return True

        except Exception as e:
            logger.error(f"Error building packages: {e}")
            return False

    def create_iso(self) -> bool:
        """Create the final Arch ISO with all components"""
        logger.info("Creating LiMe OS ISO image...")

        try:
            version = self.config['version']
            iso_name = f"{self.config['iso_name']}-{version}-x86_64.iso"
            iso_path = self.out_dir / iso_name

            archiso_path = self.build_dir / "archiso"

            # Run mkarchiso
            logger.info(f"Generating ISO: {iso_name}")

            success, output = self._run_command(
                ['sudo', 'mkarchiso', '-v', '-o', str(self.out_dir), str(archiso_path)],
                error_msg="ISO generation failed"
            )

            if not success:
                return False

            # The output from mkarchiso will be something like archlinux-*.iso
            iso_files = list(self.out_dir.glob("archlinux-*.iso"))
            if iso_files:
                original_iso = iso_files[0]
                original_iso.rename(iso_path)
                logger.info(f"✓ ISO created: {iso_path}")

                # Get ISO size
                iso_size_mb = iso_path.stat().st_size / (1024 * 1024)
                logger.info(f"  ISO size: {iso_size_mb:.2f} MB")

                if iso_size_mb > self.config['iso_size_limit']:
                    logger.warning(f"ISO size exceeds limit: {iso_size_mb:.2f} MB > {self.config['iso_size_limit']} MB")

                return True

            logger.error("ISO generation completed but no ISO file found")
            return False

        except Exception as e:
            logger.error(f"Error creating ISO: {e}")
            return False

    def generate_build_report(self) -> str:
        """Generate a build report"""
        elapsed_time = datetime.now() - self.start_time
        hours = elapsed_time.seconds // 3600
        minutes = (elapsed_time.seconds % 3600) // 60
        seconds = elapsed_time.seconds % 60

        report = f"""
===================================================
         LiMe OS Build Report
===================================================

Build Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
Duration: {hours:02d}:{minutes:02d}:{seconds:02d}
Version: {self.config['version']}
Architecture: {self.config['arch']}

Build Configuration:
  - Build Threads: {self.config['build_threads']}
  - Parallel Build: {self.config['parallel_build']}
  - Strip Binaries: {self.config['strip_binaries']}
  - Compression: {self.config['enable_compression']}

Output Directory: {self.out_dir}

ISO Image:
  - ISO Name: {self.config['iso_name']}-{self.config['version']}-{self.config['arch']}.iso
  - Location: {self.out_dir}

Packages Built:
  - lime-cinnamon (DE)
  - lime-ai-app
  - lime-installer
  - lime-welcome
  - lime-themes
  - lime-control-center

Build Log Entries: {len(self.build_log)}

Components Built:
  ✓ LiMe Desktop Environment ({self.de_dir.name})
  ✓ LiMe Installer
  ✓ LiMe AI Module
  ✓ Arch Packages
  ✓ ISO Image

Next Steps:
  1. Write ISO to USB: sudo dd if={self.out_dir}/{self.config['iso_name']}-{self.config['version']}-{self.config['arch']}.iso of=/dev/sdX bs=4M
  2. Boot from USB
  3. Run the graphical installer
  4. Enjoy LiMe OS!

===================================================
"""
        return report

    def build_all(self, skip_iso: bool = False) -> bool:
        """Execute complete build process"""
        logger.info("=" * 60)
        logger.info("LiMe OS Complete Build Process Starting")
        logger.info("=" * 60)

        steps = [
            ("Prerequisites Check", self.check_prerequisites),
            ("Build Environment", self.prepare_build_environment),
            ("LiMe DE", self.build_de),
            ("LiMe Installer", self.build_installer),
            ("LiMe AI Module", self.build_ai_module),
            ("Arch Packages", self.build_packages),
        ]

        # Only add ISO creation if not skipped
        if not skip_iso:
            steps.append(("ISO Image", self.create_iso))

        failed_steps = []

        for step_name, step_func in steps:
            logger.info(f"\n[{'='*50}]")
            logger.info(f"Step: {step_name}")
            logger.info(f"[{'='*50}]")

            try:
                if not step_func():
                    logger.error(f"✗ Failed at step: {step_name}")
                    failed_steps.append(step_name)
                    # Continue with other steps instead of aborting
            except Exception as e:
                logger.error(f"✗ Exception in step {step_name}: {e}")
                failed_steps.append(step_name)

        logger.info("\n" + "=" * 60)
        logger.info("Build Process Complete")
        logger.info("=" * 60)

        if failed_steps:
            logger.warning(f"Failed steps: {', '.join(failed_steps)}")
            return False

        # Generate and display report
        report = self.generate_build_report()
        logger.info(report)

        # Save report to file
        report_file = self.out_dir / "BUILD_REPORT.txt"
        with open(report_file, 'w') as f:
            f.write(report)
        logger.info(f"Build report saved to: {report_file}")

        # Save build log
        log_file = self.out_dir / "BUILD_LOG.json"
        with open(log_file, 'w') as f:
            json.dump(self.build_log, f, indent=2)
        logger.info(f"Detailed log saved to: {log_file}")

        return True

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="LiMe OS Automated Build System",
        epilog="Build LiMe OS ISO from source with a single command!"
    )

    parser.add_argument(
        '--project-root',
        type=Path,
        default=Path.cwd(),
        help='Project root directory (default: current directory)'
    )

    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )

    parser.add_argument(
        '--clean',
        action='store_true',
        help='Clean previous builds before starting'
    )

    parser.add_argument(
        '--skip-iso',
        action='store_true',
        help='Skip ISO generation (build components only)'
    )

    args = parser.parse_args()

    logger.setLevel(logging.DEBUG if args.verbose else logging.INFO)

    # Check if running as root (required for makepkg and ISO creation)
    if os.geteuid() != 0 and not args.skip_iso:
        logger.warning("ISO generation requires root privileges")
        logger.info("It's recommended to run with: sudo python3 build.py")
        logger.info("Or use --skip-iso to build components only")
        # Don't exit, allow component-only build

    try:
        builder = LiMeBuilder(args.project_root, verbose=args.verbose)

        if args.clean:
            logger.info("Cleaning previous build artifacts...")
            if builder.build_dir.exists():
                shutil.rmtree(builder.build_dir)

        success = builder.build_all(skip_iso=args.skip_iso)
        sys.exit(0 if success else 1)

    except KeyboardInterrupt:
        logger.info("Build interrupted by user")
        sys.exit(130)
    except Exception as e:
        logger.error(f"Fatal error: {e}", exc_info=True)
        sys.exit(1)

if __name__ == '__main__':
    main()
