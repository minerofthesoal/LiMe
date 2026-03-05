#!/usr/bin/env python3
"""LiMe AI workbench helpers for training, finetuning, dataset and paper generation."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, List


class AIWorkbench:
    def __init__(self, project_root: Path):
        self.project_root = Path(project_root)
        self.models_dir = self.project_root / "models"
        self.datasets_dir = self.project_root / "datasets"
        self.papers_dir = self.project_root / "papers"

    def scaffold(self) -> None:
        for d in [self.models_dir, self.datasets_dir, self.papers_dir]:
            d.mkdir(parents=True, exist_ok=True)

    def create_training_config(self, model_name: str, dataset_name: str) -> Path:
        self.scaffold()
        cfg = {
            "model": model_name,
            "dataset": dataset_name,
            "format": "gguf",
            "training": {
                "epochs": 3,
                "batch_size": 4,
                "learning_rate": 2e-5,
                "finetune": True,
            },
            "huggingface": {
                "repo_id": f"lime-os/{model_name}",
                "private": False,
                "token_env": "HF_TOKEN",
            },
        }
        target = self.models_dir / f"{model_name}.train.json"
        target.write_text(json.dumps(cfg, indent=2))
        return target

    def create_dataset_manifest(self, dataset_name: str, sources: List[str]) -> Path:
        self.scaffold()
        manifest = {
            "name": dataset_name,
            "sources": sources,
            "license": "custom",
            "created_by": "LiMe AI Workbench",
        }
        target = self.datasets_dir / f"{dataset_name}.manifest.json"
        target.write_text(json.dumps(manifest, indent=2))
        return target

    def create_paper_template(self, title: str) -> Path:
        self.scaffold()
        safe = title.lower().replace(" ", "-")
        content = f"# {title}\n\n## Abstract\n\n## Dataset\n\n## Method\n\n## Results\n\n## Reproducibility\n"
        target = self.papers_dir / f"{safe}.md"
        target.write_text(content)
        return target


def main() -> None:
    bench = AIWorkbench(Path.cwd())
    bench.scaffold()
    bench.create_training_config("lime-base", "lime-demo")
    bench.create_dataset_manifest("lime-demo", ["local://sample"])
    bench.create_paper_template("LiMe AI Baseline Report")


if __name__ == "__main__":
    main()
