#!/usr/bin/env python3
"""Project layout organizer for LiMe sources and assets."""

from pathlib import Path
from typing import Dict, List

EXPECTED_LAYOUT: Dict[str, List[str]] = {
    "build": ["README.md"],
    "packager": ["README.md"],
    "ui": ["README.md"],
    "install": ["README.md"],
    "assets/backgrounds": ["README.md"],
    "models": ["README.md"],
    "datasets": ["README.md"],
}

README_TEMPLATE = """# {name}\n\nThis directory is managed by the LiMe organizer for v0.1.1-prealpha.\n"""


def organize(project_root: Path) -> None:
    for directory, files in EXPECTED_LAYOUT.items():
        dir_path = project_root / directory
        dir_path.mkdir(parents=True, exist_ok=True)
        for file_name in files:
            target = dir_path / file_name
            if not target.exists():
                target.write_text(README_TEMPLATE.format(name=directory))


if __name__ == "__main__":
    organize(Path.cwd())
