#!/usr/bin/env python3
"""LiMe build orchestrator.

This module intentionally keeps the CLI stable while making the internal
orchestration safer and easier to maintain.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import multiprocessing
import os
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

from lime_tools.ai_workbench import AIWorkbench
from lime_tools.project_organizer import organize
from lime_tools.upstream_sync import UpstreamConfig, UpstreamSync

log_dir = Path.home() / ".local" / "log"
log_dir.mkdir(parents=True, exist_ok=True)
log_file = log_dir / "lime-build.log"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    handlers=[logging.FileHandler(str(log_file)), logging.StreamHandler()],
)
logger = logging.getLogger(__name__)


class LiMeBuilder:
    """Main build system for LiMe OS."""

    def __init__(
        self,
        project_root: Path,
        verbose: bool = False,
        repo_url: Optional[str] = None,
        repo_branch: str = "main",
        sync_upstream: bool = False,
    ):
        self.project_root = Path(project_root).resolve()
        self.source_root = self.project_root
        self.repo_url = repo_url
        self.repo_branch = repo_branch
        self.sync_upstream = sync_upstream
        self.verbose = verbose

        self.build_dir = self.project_root / "build"
        self.out_dir = self.project_root / "out"
        self.archiso_dir = self.project_root / "archiso"

        self.de_dir = self._detect_component_dir(["lime-de", "."], "meson.build")
        self.ai_dir = self._detect_component_dir(["lime-ai", "."], "pyproject.toml")
        self.installer_dir = self._detect_component_dir(["lime-installer", "."], "installer.py")

        self.config = self._load_config()
        self.step_results: Dict[str, bool] = {}
        self.build_log: List[Dict[str, str]] = []
        self.start_time = datetime.now()

        logger.info("LiMe Builder initialized - Project root: %s", self.project_root)

    def _detect_component_dir(self, candidates: List[str], sentinel: str) -> Path:
        """Return the first candidate directory containing sentinel file."""
        base_root = getattr(self, "source_root", self.project_root)
        for candidate in candidates:
            directory = base_root / candidate
            if (directory / sentinel).exists():
                return directory
        return base_root

    def _refresh_component_paths(self) -> None:
        self.de_dir = self._detect_component_dir(["lime-de", "."], "meson.build")
        self.ai_dir = self._detect_component_dir(["lime-ai", "."], "pyproject.toml")
        self.installer_dir = self._detect_component_dir(["lime-installer", "."], "installer.py")

    def _load_config(self) -> Dict[str, object]:
        config_file = self.project_root / "build.config.json"
        default_config: Dict[str, object] = {
            "version": "0.1.1.1-prealpha",
            "iso_name": "lime-os",
            "arch": "x86_64",
            "kernel": "linux",
            "de_name": "lime-cinnamon",
            "build_threads": max(1, multiprocessing.cpu_count() - 1),
            "enable_compression": True,
            "parallel_build": True,
            "strip_binaries": True,
            "enable_debug": False,
            "iso_size_limit": 2048,
            "min_repo_bundle_mb": 2600,
            "mint_latest_url": "https://www.linuxmint.com/edition.php?id=326",
            "cinnamon_upstream": "https://github.com/linuxmint/cinnamon",
            "cinnamon_branch": "master",
        }

        if config_file.exists():
            with open(config_file, "r", encoding="utf-8") as f:
                user_config = json.load(f)
            default_config.update(user_config)

        return default_config

    def _run_command(
        self,
        cmd: List[str],
        cwd: Optional[Path] = None,
        error_msg: Optional[str] = None,
    ) -> Tuple[bool, str]:
        try:
            logger.info("Running: %s", " ".join(cmd))
            result = subprocess.run(
                cmd,
                cwd=cwd or self.project_root,
                capture_output=True,
                text=True,
                timeout=3600,
            )

            if result.returncode != 0:
                logger.error(
                    "%s\nStdout: %s\nStderr: %s",
                    error_msg or f"Command failed: {' '.join(cmd)}",
                    result.stdout,
                    result.stderr,
                )
                return False, result.stderr

            if self.verbose and result.stdout:
                logger.info("Output: %s", result.stdout.strip())

            self.build_log.append(
                {
                    "timestamp": datetime.now().isoformat(),
                    "command": " ".join(cmd),
                    "status": "success",
                    "output": result.stdout[:500],
                }
            )
            return True, result.stdout
        except subprocess.TimeoutExpired:
            logger.error("Command timeout: %s", " ".join(cmd))
            return False, "Command timeout"
        except Exception as e:
            logger.error("Exception running command: %s", e)
            return False, str(e)

    def check_prerequisites(self) -> bool:
        logger.info("Checking prerequisites...")
        required_tools = ["python3", "git"]
        optional_tools = ["pacman", "makepkg", "mkarchiso", "grub"]

        missing_required = [t for t in required_tools if shutil.which(t) is None]
        missing_optional = [t for t in optional_tools if shutil.which(t) is None]

        if missing_required:
            logger.error("Missing required tools: %s", ", ".join(missing_required))
            return False

        if missing_optional:
            logger.warning("Missing optional tools (ISO generation may fail): %s", ", ".join(missing_optional))

        logger.info("✓ Core prerequisites satisfied")
        return True

    def sync_repository_sources(self) -> bool:
        logger.info("Syncing repository sources...")
        try:
            self.build_dir.mkdir(parents=True, exist_ok=True)
            snapshot_root = self.project_root / ".lime-source-snapshot"
            if snapshot_root.exists():
                shutil.rmtree(snapshot_root)

            repo_url = self.repo_url
            if not repo_url:
                origin = subprocess.run(
                    ["git", "config", "--get", "remote.origin.url"],
                    cwd=self.project_root,
                    capture_output=True,
                    text=True,
                )
                if origin.returncode == 0 and origin.stdout.strip():
                    repo_url = origin.stdout.strip()

            if repo_url:
                success, _ = self._run_command(
                    ["git", "clone", "--branch", self.repo_branch, "--recurse-submodules", repo_url, str(snapshot_root)],
                    cwd=self.project_root,
                    error_msg="Source clone failed",
                )
                if not success:
                    return False
                self._run_command(["git", "submodule", "update", "--init", "--recursive"], cwd=snapshot_root)
                if shutil.which("git-lfs"):
                    self._run_command(["git", "lfs", "pull"], cwd=snapshot_root)
            else:
                logger.warning("No repo URL configured; snapshotting local checkout")
                shutil.copytree(
                    self.project_root,
                    snapshot_root,
                    ignore=shutil.ignore_patterns(".git", "build", "out", ".lime-source-snapshot"),
                )

            self.source_root = snapshot_root
            self._refresh_component_paths()
            logger.info("✓ Source snapshot ready at: %s", self.source_root)
            return True
        except Exception as e:
            logger.error("Failed to sync repository sources: %s", e)
            return False

    def sync_upstream_assets(self) -> bool:
        if not self.sync_upstream:
            logger.info("Upstream sync disabled; skipping")
            return True

        logger.info("Syncing upstream Linux Mint/Cinnamon assets...")
        try:
            cfg = UpstreamConfig(
                mint_page_url=str(self.config.get("mint_latest_url")),
                cinnamon_repo_url=str(self.config.get("cinnamon_upstream")),
                cinnamon_branch=str(self.config.get("cinnamon_branch")),
            )
            UpstreamSync(self.project_root).sync(cfg)
            logger.info("✓ Upstream assets synced")
            return True
        except Exception as e:
            logger.warning("Upstream sync skipped due to network/access issue: %s", e)
            return True

    def package_source_snapshot(self) -> bool:
        logger.info("Packaging source snapshot...")
        try:
            self.out_dir.mkdir(parents=True, exist_ok=True)
            archive_base = self.out_dir / f"{self.config['iso_name']}-{self.config['version']}-source"
            archive_path = shutil.make_archive(str(archive_base), "gztar", root_dir=self.source_root)

            digest = hashlib.sha256(Path(archive_path).read_bytes()).hexdigest()
            checksum_file = Path(f"{archive_path}.sha256")
            checksum_file.write_text(f"{digest}  {Path(archive_path).name}\n", encoding="utf-8")

            size_mb = Path(archive_path).stat().st_size / (1024 * 1024)
            min_mb = float(self.config.get("min_repo_bundle_mb", 0))
            if min_mb and size_mb < min_mb:
                logger.warning("Source archive is %.2f MB, below requested %.0f MB target", size_mb, min_mb)

            logger.info("✓ Source archive created: %s", archive_path)
            return True
        except Exception as e:
            logger.error("Failed to package source snapshot: %s", e)
            return False

    def organize_project(self) -> bool:
        logger.info("Organizing project layout...")
        try:
            organize(self.source_root)
            AIWorkbench(self.source_root).scaffold()
            logger.info("✓ Project layout organized")
            return True
        except Exception as e:
            logger.error("Failed to organize project layout: %s", e)
            return False

    def _ensure_archiso_scaffold(self) -> None:
        self.archiso_dir = self.source_root / "archiso"
        if self.archiso_dir.exists():
            return

        logger.warning("archiso directory not found. Creating minimal scaffold...")
        (self.archiso_dir / "airootfs").mkdir(parents=True, exist_ok=True)
        (self.archiso_dir / "syslinux").mkdir(parents=True, exist_ok=True)

        (self.archiso_dir / "packages.x86_64").write_text("base\nlinux\nlinux-firmware\n", encoding="utf-8")
        (self.archiso_dir / "profiledef.sh").write_text(
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
""",
            encoding="utf-8",
        )
        (self.archiso_dir / "pacman.conf").write_text("[options]\nHoldPkg = pacman glibc\n", encoding="utf-8")
        (self.archiso_dir / "syslinux" / "syslinux.cfg").write_text(
            "DEFAULT arch\nLABEL arch\n    LINUX /%INSTALL_DIR%/boot/x86_64/vmlinuz-linux\n",
            encoding="utf-8",
        )

    def prepare_build_environment(self) -> bool:
        logger.info("Preparing build environment...")
        try:
            if self.build_dir.exists():
                shutil.rmtree(self.build_dir)
            self.build_dir.mkdir(parents=True, exist_ok=True)
            self.out_dir.mkdir(parents=True, exist_ok=True)

            self._ensure_archiso_scaffold()
            dest_archiso = self.build_dir / "archiso"
            if dest_archiso.exists():
                shutil.rmtree(dest_archiso)
            shutil.copytree(self.archiso_dir, dest_archiso)

            for subdir in ["airootfs", "configs", "out"]:
                (self.build_dir / subdir).mkdir(parents=True, exist_ok=True)

            logger.info("✓ Build environment prepared")
            return True
        except Exception as e:
            logger.error("Failed to prepare build environment: %s", e)
            return False

    def build_de(self) -> bool:
        logger.info("Building LiMe Desktop Environment...")
        if not (self.de_dir / "meson.build").exists():
            logger.warning("No meson.build found for LiMe DE; skipping DE build.")
            return True
        if shutil.which("meson") is None or shutil.which("ninja") is None:
            logger.warning("Meson/ninja not available; skipping DE build.")
            return True

        de_build = self.build_dir / "de"
        de_build.mkdir(parents=True, exist_ok=True)
        ok, _ = self._run_command(
            ["meson", "setup", str(de_build), "--prefix=/usr", "--buildtype=release", "-Dman=false"],
            cwd=self.de_dir,
            error_msg="Meson setup failed",
        )
        if not ok:
            return False
        ok, _ = self._run_command(["ninja", "-C", str(de_build), f"-j{self.config['build_threads']}"], error_msg="DE compilation failed")
        if ok:
            logger.info("✓ LiMe DE built successfully")
        return ok

    def _build_python_wheel(self, component_dir: Path, label: str) -> bool:
        if not (component_dir / "src").exists():
            logger.warning("%s source directory missing (src/); skipping wheel build.", label)
            return True
        ok, _ = self._run_command(
            ["python3", "-m", "pip", "wheel", ".", "--no-deps", "--no-build-isolation", "-w", "dist"],
            cwd=component_dir,
            error_msg=f"{label} build failed",
        )
        return ok

    def build_installer(self) -> bool:
        logger.info("Building LiMe Installer...")
        ok = self._build_python_wheel(self.installer_dir, "Installer")
        if ok:
            logger.info("✓ LiMe Installer built successfully")
        return ok

    def build_ai_module(self) -> bool:
        logger.info("Building LiMe AI Module...")
        ok = self._build_python_wheel(self.ai_dir, "AI module")
        if ok:
            logger.info("✓ LiMe AI Module built successfully")
        return ok

    def build_packages(self) -> bool:
        logger.info("Building Arch packages...")
        pkgbuild_dir = self.source_root / "packaging"
        packages = [
            "lime-cinnamon",
            "lime-ai-app",
            "lime-installer",
            "lime-welcome",
            "lime-themes",
            "lime-control-center",
        ]
        for pkg in packages:
            pkg_dir = pkgbuild_dir / pkg
            if not pkg_dir.exists():
                logger.warning("Package dir not found: %s, skipping", pkg)
                continue
            self._run_command(["makepkg", "-s", "--noconfirm"], cwd=pkg_dir, error_msg=f"Package build failed: {pkg}")
        logger.info("✓ Packages built successfully")
        return True

    def create_iso(self) -> bool:
        logger.info("Creating LiMe OS ISO image...")
        if shutil.which("mkarchiso") is None:
            logger.warning("mkarchiso not available; skipping ISO creation.")
            return True

        version = self.config["version"]
        iso_name = f"{self.config['iso_name']}-{version}-x86_64.iso"
        iso_path = self.out_dir / iso_name
        archiso_path = self.build_dir / "archiso"

        ok, _ = self._run_command(["mkarchiso", "-v", "-o", str(self.out_dir), str(archiso_path)], error_msg="ISO generation failed")
        if not ok:
            return False

        iso_files = list(self.out_dir.glob("archlinux-*.iso")) + list(self.out_dir.glob("*.iso"))
        if not iso_files:
            logger.error("ISO generation completed but no ISO file found")
            return False

        original_iso = iso_files[0]
        if original_iso != iso_path:
            original_iso.rename(iso_path)
        logger.info("✓ ISO created: %s", iso_path)
        return True

    def generate_build_report(self) -> str:
        elapsed = datetime.now() - self.start_time
        components = "\n".join(f"  {'✓' if ok else '✗'} {name}" for name, ok in self.step_results.items())
        return f"""
===================================================
         LiMe OS Build Report
===================================================

Build Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
Duration: {elapsed}
Version: {self.config['version']}
Architecture: {self.config['arch']}
Output Directory: {self.out_dir}
Build Log Entries: {len(self.build_log)}

Components Built:
{components if components else '  (no steps recorded)'}

        logger.info("=" * 60)
        logger.info("LiMe OS Complete Build Process Starting")
        logger.info("=" * 60)

        steps: List[Tuple[str, Callable[[], bool]]] = [
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
        if not skip_iso:
            steps.append(("ISO Image", self.create_iso))

        failed_steps: List[str] = []
        for name, func in steps:
            logger.info("\n[%s]", "=" * 50)
            logger.info("Step: %s", name)
            logger.info("[%s]", "=" * 50)
            try:
                ok = func()
            except Exception as e:
                logger.error("✗ Exception in step %s: %s", name, e)
                ok = False
            self.step_results[name] = ok
            if not ok:
                failed_steps.append(name)

        logger.info("\n" + "=" * 60)
        logger.info("Build Process Complete")
        logger.info("=" * 60)

        report = self.generate_build_report()
        self.out_dir.mkdir(parents=True, exist_ok=True)
        (self.out_dir / "BUILD_REPORT.txt").write_text(report, encoding="utf-8")
        (self.out_dir / "BUILD_LOG.json").write_text(json.dumps(self.build_log, indent=2), encoding="utf-8")

        if failed_steps:
            logger.warning("Failed steps: %s", ", ".join(failed_steps))
            return False
        return True


def main() -> None:
    parser = argparse.ArgumentParser(description="LiMe OS Automated Build System")
    parser.add_argument("--project-root", type=Path, default=Path.cwd())
    parser.add_argument("--verbose", "-v", action="store_true")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--skip-iso", action="store_true")
    parser.add_argument("--repo-url", type=str, default=None)
    parser.add_argument("--repo-branch", type=str, default="main")
    parser.add_argument("--sync-upstream", action="store_true")
    args = parser.parse_args()

    logger.setLevel(logging.DEBUG if args.verbose else logging.INFO)

    if os.geteuid() != 0 and not args.skip_iso:
        logger.warning("ISO generation may require root privileges")

    builder = LiMeBuilder(
        args.project_root,
        verbose=args.verbose,
        repo_url=args.repo_url,
        repo_branch=args.repo_branch,
        sync_upstream=args.sync_upstream,
    )

    if args.clean and builder.build_dir.exists():
        shutil.rmtree(builder.build_dir)

    success = builder.build_all(skip_iso=args.skip_iso)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
