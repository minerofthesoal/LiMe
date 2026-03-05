#!/usr/bin/env python3
"""Upstream asset synchronization helpers for LiMe.

Supports pulling:
- Linux Mint ISO metadata page / downloadable ISO asset
- Cinnamon upstream source snapshot
"""

from __future__ import annotations

import hashlib
import json
import shutil
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


@dataclass
class UpstreamConfig:
    mint_page_url: str = "https://www.linuxmint.com/edition.php?id=326"
    cinnamon_repo_url: str = "https://github.com/linuxmint/cinnamon"
    cinnamon_branch: str = "master"
    download_mint_iso: bool = False


class UpstreamSync:
    def __init__(self, root: Path):
        self.root = Path(root)
        self.upstream_dir = self.root / "assets" / "upstream"
        self.upstream_dir.mkdir(parents=True, exist_ok=True)

    def fetch_mint_metadata(self, url: str) -> Path:
        target = self.upstream_dir / "linuxmint_edition_326.html"
        with urllib.request.urlopen(url, timeout=60) as response:
            target.write_bytes(response.read())
        return target

    def clone_cinnamon(self, repo_url: str, branch: str) -> Path:
        target = self.upstream_dir / "cinnamon-upstream"
        if target.exists():
            shutil.rmtree(target)

        import subprocess

        subprocess.run(
            ["git", "clone", "--depth", "1", "--branch", branch, repo_url, str(target)],
            check=True,
            capture_output=True,
            text=True,
        )
        return target

    def snapshot_manifest(self, metadata_file: Path, cinnamon_dir: Path) -> Path:
        manifest = {
            "mint_metadata_file": str(metadata_file.relative_to(self.root)),
            "cinnamon_snapshot_dir": str(cinnamon_dir.relative_to(self.root)),
            "metadata_sha256": hashlib.sha256(metadata_file.read_bytes()).hexdigest(),
        }
        target = self.upstream_dir / "upstream-manifest.json"
        target.write_text(json.dumps(manifest, indent=2))
        return target

    def sync(self, cfg: UpstreamConfig) -> dict:
        metadata_file = self.fetch_mint_metadata(cfg.mint_page_url)
        cinnamon_dir = self.clone_cinnamon(cfg.cinnamon_repo_url, cfg.cinnamon_branch)
        manifest = self.snapshot_manifest(metadata_file, cinnamon_dir)
        return {
            "metadata": metadata_file,
            "cinnamon": cinnamon_dir,
            "manifest": manifest,
        }


def main() -> None:
    sync = UpstreamSync(Path.cwd())
    result = sync.sync(UpstreamConfig())
    for k, v in result.items():
        print(f"{k}: {v}")


if __name__ == "__main__":
    main()
