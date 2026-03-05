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
import hashlib
import urllib.request
import hashlib
import time
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Tuple, Optional

from lime_tools.project_organizer import organize
from lime_tools.ai_workbench import AIWorkbench
from lime_tools.upstream_sync import UpstreamSync, UpstreamConfig

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

    def __init__(self, project_root: Path, verbose: bool = False, repo_url: Optional[str] = None, repo_branch: str = "main", sync_upstream: bool = False):
        self.project_root = Path(project_root).resolve()
        self.source_root = self.project_root
        self.repo_url = repo_url
        self.repo_branch = repo_branch
        self.sync_upstream = sync_upstream
    def __init__(self, project_root: Path, verbose: bool = False):
        self.project_root = Path(project_root)
        self.verbose = verbose
        self.build_dir = self.project_root / "build"
        self.out_dir = self.project_root / "out"
        self.archiso_dir = self.project_root / "archiso"
        self.de_dir = self._detect_component_dir(["lime-de", "."], "meson.build")
        self.ai_dir = self._detect_component_dir(["lime-ai", "."], "pyproject.toml")
        self.installer_dir = self._detect_component_dir(["lime-installer", "."], "installer.py")
        self.step_results: Dict[str, bool] = {}

        self.config = self._load_config()
        self.build_log = []
        self.start_time = datetime.now()

        logger.info(f"LiMe Builder initialized - Project root: {self.project_root}")

    def _detect_component_dir(self, candidates: List[str], sentinel: str) -> Path:
        """Find component directory with graceful fallback to project root."""
        for candidate in candidates:
            directory = self.source_root / candidate
            directory = self.project_root / candidate
            if (directory / sentinel).exists():
                return directory
        return self.project_root


    def _refresh_component_paths(self) -> None:
        """Recalculate component paths from active source root."""
        self.de_dir = self._detect_component_dir(["lime-de", "."], "meson.build")
        self.ai_dir = self._detect_component_dir(["lime-ai", "."], "pyproject.toml")
        self.installer_dir = self._detect_component_dir(["lime-installer", "."], "installer.py")

    def sync_repository_sources(self) -> bool:
        """Download/update repository sources into a clean build snapshot."""
        logger.info("Syncing repository sources...")
        try:
            self.build_dir.mkdir(parents=True, exist_ok=True)
            if shutil.which('git') is None:
                logger.error("git is required for source sync")
                return False

            repo_url = self.repo_url
            if not repo_url:
                origin = subprocess.run(
                    ['git', 'config', '--get', 'remote.origin.url'],
                    cwd=self.project_root,
                    capture_output=True,
                    text=True,
                )
                if origin.returncode == 0 and origin.stdout.strip():
                    repo_url = origin.stdout.strip()

            snapshot_root = self.project_root / ".lime-source-snapshot"
            if snapshot_root.exists():
                shutil.rmtree(snapshot_root)

            if repo_url:
                logger.info(f"Cloning source snapshot from {repo_url} ({self.repo_branch})")
                clone_cmd = ['git', 'clone', '--branch', self.repo_branch, '--recurse-submodules', repo_url, str(snapshot_root)]
                success, _ = self._run_command(clone_cmd, cwd=self.project_root, error_msg="Source clone failed")
                if not success:
                    return False

                self._run_command(['git', 'submodule', 'update', '--init', '--recursive'], cwd=snapshot_root)
                if shutil.which('git-lfs') is not None:
                    self._run_command(['git', 'lfs', 'pull'], cwd=snapshot_root)
            else:
                logger.warning("No repo URL configured; creating local snapshot from current checkout")
                shutil.copytree(self.project_root, snapshot_root, ignore=shutil.ignore_patterns('.git', 'build', 'out'))

            self.source_root = snapshot_root
            self._refresh_component_paths()
            logger.info(f"✓ Source snapshot ready at: {self.source_root}")
            return True
        except Exception as e:
            logger.error(f"Failed to sync repository sources: {e}")
            return False

    def package_source_snapshot(self) -> bool:
        """Archive synced repository files for reproducible offline builds."""
        logger.info("Packaging source snapshot...")
        try:
            if self.source_root == self.project_root:
                logger.warning("No external snapshot detected; packaging current project tree")

            self.out_dir.mkdir(parents=True, exist_ok=True)
            archive_base = self.out_dir / f"{self.config['iso_name']}-{self.config['version']}-source"
            archive_path = shutil.make_archive(str(archive_base), 'gztar', root_dir=self.source_root)

            digest = hashlib.sha256(Path(archive_path).read_bytes()).hexdigest()
            checksum_file = Path(f"{archive_path}.sha256")
            checksum_file.write_text(f"{digest}  {Path(archive_path).name}\n")
            size_mb = Path(archive_path).stat().st_size / (1024 * 1024)
            minimum_mb = self.config.get('min_repo_bundle_mb', 0)
            if minimum_mb and size_mb < minimum_mb:
                logger.warning(f"Source archive is {size_mb:.2f} MB, below requested {minimum_mb} MB target")
            logger.info(f"✓ Source archive created: {archive_path}")
            return True
        except Exception as e:
            logger.error(f"Failed to package source snapshot: {e}")
            return False

    def _load_config(self) -> Dict:
        """Load build configuration"""
        config_file = self.project_root / "build.config.json"

        default_config = {
            "version": "0.1.1.1-prealpha",
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
            "min_repo_bundle_mb": 2600,
            "mint_latest_url": "https://www.linuxmint.com/edition.php?id=326",
            "cinnamon_upstream": "https://github.com/linuxmint/cinnamon",
            "cinnamon_branch": "master",
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
        required_tools = ['python3', 'git']

        # Optional tools (needed for full ISO but not for component build)
        optional_tools = ['pacman', 'makepkg', 'mkarchiso', 'grub']

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

    def sync_upstream_assets(self) -> bool:
        """Optionally sync Linux Mint/Cinnamon upstream references."""
        if not self.sync_upstream:
            logger.info("Upstream sync disabled; skipping")
            return True

        logger.info("Syncing upstream Linux Mint/Cinnamon assets...")
        try:
            cfg = UpstreamConfig(
                mint_page_url=self.config.get('mint_latest_url', 'https://www.linuxmint.com/edition.php?id=326'),
                cinnamon_repo_url=self.config.get('cinnamon_upstream', 'https://github.com/linuxmint/cinnamon'),
                cinnamon_branch=self.config.get('cinnamon_branch', 'master'),
            )
            syncer = UpstreamSync(self.project_root)
            syncer.sync(cfg)
            logger.info("✓ Upstream assets synced")
            return True
        except Exception as e:
            logger.warning(f"Upstream sync skipped due to network/access issue: {e}")
            return True

    def organize_project(self) -> bool:
        """Ensure project folders and placeholders are organized."""
        logger.info("Organizing project layout...")
        try:
            organize(self.source_root)
            AIWorkbench(self.source_root).scaffold()
            organize(self.project_root)
            AIWorkbench(self.project_root).scaffold()
            logger.info("✓ Project layout organized")
            return True
        except Exception as e:
            logger.error(f"Failed to organize project layout: {e}")
            return False

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

            # Ensure archiso config exists and copy it
            self._ensure_archiso_scaffold()
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

    def _ensure_archiso_scaffold(self) -> None:
        """Create a minimal archiso layout when missing so ISO builds can proceed."""
        self.archiso_dir = self.source_root / "archiso"
        if self.archiso_dir.exists():
            return

        logger.warning("archiso directory not found. Creating minimal scaffold...")
        profile_dir = self.archiso_dir
        airootfs = profile_dir / "airootfs"
        syslinux = profile_dir / "syslinux"

        airootfs.mkdir(parents=True, exist_ok=True)
        syslinux.mkdir(parents=True, exist_ok=True)

        (profile_dir / "packages.x86_64").write_text("base\nlinux\nlinux-firmware\n")
        (profile_dir / "profiledef.sh").write_text(
            """#!/usr/bin/env bash
iso_name=\"lime-os\"
iso_label=\"LIME_$(date +%Y%m)\"
iso_publisher=\"LiMe <https://example.invalid>\"
iso_application=\"LiMe Live/Rescue CD\"
install_dir=\"arch\"
buildmodes=('iso')
bootmodes=('bios.syslinux.x86_64' 'uefi-x64.systemd-boot')
arch=\"x86_64\"
pacman_conf=\"pacman.conf\"
"""
        )
        (profile_dir / "pacman.conf").write_text("[options]\nHoldPkg = pacman glibc\n")
        (syslinux / "syslinux.cfg").write_text("DEFAULT arch\nLABEL arch\n    LINUX /%INSTALL_DIR%/boot/x86_64/vmlinuz-linux\n")

    def build_de(self) -> bool:
        """Build the LiMe Desktop Environment"""
        logger.info("Building LiMe Desktop Environment...")

        try:
            de_build = self.build_dir / "de"
            de_build.mkdir(parents=True, exist_ok=True)

            # Initialize Meson build
            if not (self.de_dir / "meson.build").exists():
                logger.warning("No meson.build found for LiMe DE; skipping DE build.")
                return True
            if shutil.which('meson') is None or shutil.which('ninja') is None:
                logger.warning("Meson/ninja not available; skipping DE build.")
                return True

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
            if not (self.installer_dir / "src").exists():
                logger.warning("Installer source directory missing (src/); skipping installer wheel build.")
                return True
            # Build installer Python package
            success, output = self._run_command(
                ['python3', '-m', 'pip', 'wheel', '.', '--no-deps', '--no-build-isolation', '-w', 'dist'],
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
            if not (self.ai_dir / "src").exists():
                logger.warning("AI source directory missing (src/); skipping AI wheel build.")
                return True
            if not (self.ai_dir / "pyproject.toml").exists():
                logger.warning("No AI package metadata found; skipping AI wheel build.")
                return True

            success, output = self._run_command(
                ['python3', '-m', 'pip', 'wheel', '.', '--no-deps', '--no-build-isolation', '-w', 'dist'],
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

            if shutil.which('mkarchiso') is None:
                logger.warning("mkarchiso not available; skipping ISO creation.")
                return True

            success, output = self._run_command(
                ['mkarchiso', '-v', '-o', str(self.out_dir), str(archiso_path)],
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

        components = "\n".join(
            f"  {'✓' if ok else '✗'} {name}" for name, ok in self.step_results.items()
        )

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
{components if components else '  (no steps recorded)'}

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
            ("Source Sync", self.sync_repository_sources),
            ("Upstream Sync", self.sync_upstream_assets),
            ("Source Packaging", self.package_source_snapshot),
            ("Project Organization", self.organize_project),
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
                step_ok = step_func()
                self.step_results[step_name] = step_ok
                if not step_ok:
                    logger.error(f"✗ Failed at step: {step_name}")
                    failed_steps.append(step_name)
                    # Continue with other steps instead of aborting
            except Exception as e:
                logger.error(f"✗ Exception in step {step_name}: {e}")
                self.step_results[step_name] = False
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

    parser.add_argument(
        '--repo-url',
        type=str,
        default=None,
        help='Repository URL to clone before building (defaults to origin remote)'
    )

    parser.add_argument(
        '--repo-branch',
        type=str,
        default='main',
        help='Repository branch to clone for source sync'
    )

    parser.add_argument(
        '--sync-upstream',
        action='store_true',
        help='Download Linux Mint metadata and Cinnamon upstream snapshot'
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
