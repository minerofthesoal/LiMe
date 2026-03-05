#!/usr/bin/env python3
"""LiMe OS build orchestrator.

This script now supports both the historical multi-folder layout and the
current flat repository layout used in this pre-alpha branch.
"""

from __future__ import annotations

import argparse
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

    def __init__(self, project_root: Path, verbose: bool = False):
        self.project_root = Path(project_root).resolve()
        self.verbose = verbose
        self.build_dir = self.project_root / "build"
        self.out_dir = self.project_root / "out"

        self.layout = self._detect_layout()
        self.config = self._load_config()
        self.start_time = datetime.now()
        self.build_log: List[Dict[str, str]] = []

        logger.info("LiMe Builder initialized - Project root: %s", self.project_root)
        logger.info("Detected layout: %s", self.layout["name"])

    def _detect_layout(self) -> Dict[str, Path]:
        """Detect repository layout and resolve component paths."""
        nested = {
            "name": "nested",
            "archiso": self.project_root / "archiso",
            "de": self.project_root / "lime-de",
            "installer": self.project_root / "lime-installer",
            "ai": self.project_root / "lime-ai",
            "packaging": self.project_root / "packaging",
        }

        flat = {
            "name": "flat",
            "archiso": self.project_root / "archiso",
            "de": self.project_root,
            "installer": self.project_root,
            "ai": self.project_root,
            "packaging": self.project_root / "packaging",
        }

        nested_de = (nested["de"] / "meson.build").exists() or (nested["de"] / "src" / "meson.build").exists()
        nested_installer = (nested["installer"] / "pyproject.toml").exists() or (nested["installer"] / "setup.py").exists()
        nested_ai = (nested["ai"] / "pyproject.toml").exists() or (nested["ai"] / "setup.py").exists()

        if nested_de or nested_installer or nested_ai:
            return nested
        return flat

    def _load_config(self) -> Dict[str, object]:
        default_config = {
            "version": "0.1-prealpha",
            "iso_name": "lime-os",
            "arch": "x86_64",
            "build_threads": max(1, multiprocessing.cpu_count() - 1),
            "iso_size_limit": 4096,
            "strict_prerequisites": False,
        }

        config_file = self.project_root / "build.config.json"
        if config_file.exists():
            default_config.update(json.loads(config_file.read_text()))
        return default_config

    def _run_command(
        self,
        cmd: List[str],
        cwd: Optional[Path] = None,
        error_msg: Optional[str] = None,
        timeout: int = 3600,
    ) -> Tuple[bool, str]:
        try:
            logger.info("Running: %s", " ".join(cmd))
            result = subprocess.run(
                cmd,
                cwd=cwd or self.project_root,
                capture_output=True,
                text=True,
                timeout=timeout,
            )
            if result.returncode != 0:
                logger.error("%s\nStdout: %s\nStderr: %s", error_msg or "Command failed", result.stdout, result.stderr)
                return False, result.stderr.strip()

            if self.verbose and result.stdout.strip():
                logger.info(result.stdout.strip())

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
            return False, f"Timeout: {' '.join(cmd)}"
        except Exception as exc:
            return False, str(exc)

    def check_prerequisites(self) -> bool:
        logger.info("Checking prerequisites...")
        required = ["python3", "meson", "ninja", "pkg-config", "gcc", "make"]
        optional = ["mkarchiso", "makepkg"]

        missing_required = [tool for tool in required if shutil.which(tool) is None]
        missing_optional = [tool for tool in optional if shutil.which(tool) is None]

        if missing_required:
            if self.config.get("strict_prerequisites"):
                logger.error("Missing required tools: %s", ", ".join(missing_required))
                return False
            logger.warning("Missing build tools for some components: %s", ", ".join(missing_required))

        if missing_optional:
            logger.warning("Missing optional tools (package/ISO generation may be skipped): %s", ", ".join(missing_optional))

        logger.info("✓ Prerequisite checks complete")
        return True

    def prepare_build_environment(self) -> bool:
        logger.info("Preparing build environment...")
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self.out_dir.mkdir(parents=True, exist_ok=True)

        archiso_src = self.layout["archiso"]
        archiso_dest = self.build_dir / "archiso"
        if archiso_dest.exists():
            shutil.rmtree(archiso_dest)

        if archiso_src.exists():
            shutil.copytree(archiso_src, archiso_dest)
        else:
            logger.warning("No archiso/ directory found, creating minimal profile scaffold")
            (archiso_dest / "airootfs").mkdir(parents=True, exist_ok=True)
            (archiso_dest / "packages.x86_64").write_text("base\nlinux\n")
            (archiso_dest / "profiledef.sh").write_text("iso_name=\"lime-os\"\n")

        logger.info("✓ Build environment prepared")
        return True

    def build_de(self) -> bool:
        logger.info("Building LiMe Desktop Environment...")
        de_root = self.layout["de"]
        de_build = self.build_dir / "de"

        meson_file = de_root / "meson.build"
        if not meson_file.exists():
            logger.warning("No meson.build found for DE at %s; marking as skipped", de_root)
            return True

        if shutil.which("meson") is None or shutil.which("ninja") is None:
            logger.warning("meson/ninja not available; skipping DE build")
            return True

        de_build.mkdir(parents=True, exist_ok=True)
        success, _ = self._run_command(
            ["meson", "setup", str(de_build), "--prefix=/usr", "--buildtype=release"],
            cwd=de_root,
            error_msg="Meson setup failed",
        )
        if not success:
            return False

        success, _ = self._run_command(
            ["ninja", "-C", str(de_build), f"-j{self.config['build_threads']}"]
        )
        if not success:
            return False

        logger.info("✓ LiMe DE built successfully")
        return True

    def build_installer(self) -> bool:
        logger.info("Building LiMe Installer...")
        installer_root = self.layout["installer"]
        if not (installer_root / "pyproject.toml").exists() and not (installer_root / "setup.py").exists():
            logger.warning("No installer project metadata found in %s", installer_root)
            return True

        success, _ = self._run_command(
            ["python3", "setup.py", "bdist_wheel"],
            cwd=installer_root,
            error_msg="Installer build failed",
        )
        if success:
            logger.info("✓ LiMe Installer built successfully")
        return success

    def build_ai_module(self) -> bool:
        logger.info("Building LiMe AI module...")
        ai_root = self.layout["ai"]

        # Flat layout ships AI capabilities as scripts and C modules; nothing to package yet.
        if self.layout["name"] == "flat":
            logger.info("Flat layout detected; AI module packaging not required")
            return True

        if not (ai_root / "pyproject.toml").exists() and not (ai_root / "setup.py").exists():
            logger.warning("No AI package metadata found in %s", ai_root)
            return True

        success, _ = self._run_command(
            ["python3", "setup.py", "bdist_wheel"],
            cwd=ai_root,
            error_msg="AI module build failed",
        )
        if success:
            logger.info("✓ LiMe AI Module built successfully")
        return success

    def create_iso(self) -> bool:
        logger.info("Creating ISO image...")
        mkarchiso = shutil.which("mkarchiso")
        if mkarchiso is None:
            logger.warning("mkarchiso not found; skipping ISO creation")
            return True

        archiso_path = self.build_dir / "archiso"
        success, _ = self._run_command([mkarchiso, "-v", "-o", str(self.out_dir), str(archiso_path)], error_msg="ISO generation failed")
        if not success:
            return False

        produced_isos = sorted(self.out_dir.glob("*.iso"), key=lambda p: p.stat().st_mtime, reverse=True)
        if not produced_isos:
            logger.error("mkarchiso reported success but no ISO was produced")
            return False

        target_name = f"{self.config['iso_name']}-{self.config['version']}-{self.config['arch']}.iso"
        target = self.out_dir / target_name
        if produced_isos[0] != target:
            produced_isos[0].rename(target)

        logger.info("✓ ISO created: %s", target)
        return True

    def build_all(self, skip_iso: bool = False) -> bool:
        steps: List[Tuple[str, Callable[[], bool]]] = [
            ("Prerequisites Check", self.check_prerequisites),
            ("Build Environment", self.prepare_build_environment),
            ("LiMe DE", self.build_de),
            ("LiMe Installer", self.build_installer),
            ("LiMe AI Module", self.build_ai_module),
        ]
        if not skip_iso:
            steps.append(("ISO Image", self.create_iso))

        failures: List[str] = []
        for step_name, fn in steps:
            logger.info("\n[%s]", step_name)
            if not fn():
                failures.append(step_name)

        if failures:
            logger.warning("Failed steps: %s", ", ".join(failures))
            return False

        report_path = self.out_dir / "BUILD_REPORT.txt"
        report = self.generate_build_report()
        report_path.write_text(report)
        (self.out_dir / "BUILD_LOG.json").write_text(json.dumps(self.build_log, indent=2))
        logger.info("Build report saved to: %s", report_path)
        return True

    def generate_build_report(self) -> str:
        elapsed = datetime.now() - self.start_time
        return (
            "LiMe OS Build Report\n"
            f"Date: {datetime.now():%Y-%m-%d %H:%M:%S}\n"
            f"Duration: {elapsed}\n"
            f"Layout: {self.layout['name']}\n"
            f"Version: {self.config['version']}\n"
            f"Output: {self.out_dir}\n"
        )


def main() -> None:
    parser = argparse.ArgumentParser(description="LiMe OS build orchestrator")
    parser.add_argument("--project-root", type=Path, default=Path.cwd())
    parser.add_argument("--verbose", "-v", action="store_true")
    parser.add_argument("--skip-iso", action="store_true")
    args = parser.parse_args()

    logger.setLevel(logging.DEBUG if args.verbose else logging.INFO)
    builder = LiMeBuilder(args.project_root, verbose=args.verbose)
    ok = builder.build_all(skip_iso=args.skip_iso)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
