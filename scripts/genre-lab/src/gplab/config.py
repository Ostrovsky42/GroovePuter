from __future__ import annotations

from pathlib import Path

import yaml

REQUIRED = ("family", "variants")


def load_config(path) -> dict:
    cfg = yaml.safe_load(Path(path).read_text(encoding="utf-8"))
    for k in REQUIRED:
        if k not in cfg:
            raise ValueError(f"{path}: missing '{k}'")
    for v in cfg["variants"]:
        if not v.startswith(cfg["family"] + "."):
            raise ValueError(f"variant '{v}' must start with '{cfg['family']}.'")
    return cfg
