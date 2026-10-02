#!/usr/bin/env python3
from pathlib import Path

ALLOWED = {
    "core.yml",
    "cardputer-adv.yml",
    "midi-targets.yml",
    "memory.yml",
    "release.yml",
    "nightly.yml",
    "research-manual.yml",
}

root = Path(__file__).resolve().parents[2]
workflow_dir = root / ".github" / "workflows"
actual = {
    p.name
    for p in workflow_dir.iterdir()
    if p.is_file() and p.suffix in {".yml", ".yaml"}
}

missing = sorted(ALLOWED - actual)
unexpected = sorted(actual - ALLOWED)

if missing or unexpected:
    if missing:
        print("Missing canonical workflows:")
        for item in missing:
            print(f"  - {item}")
    if unexpected:
        print("Unexpected workflow surface:")
        for item in unexpected:
            print(f"  - {item}")
    raise SystemExit(1)

print(f"CI workflow surface OK: {len(actual)} canonical workflows")
