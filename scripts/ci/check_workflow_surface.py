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


ledger = root / "docs" / "ci" / "WORKFLOW_MIGRATION.tsv"
rows = ledger.read_text(encoding="utf-8").splitlines()
if not rows or rows[0] != "legacy_workflow\tdisposition\tnew_owner":
    raise SystemExit("invalid CI migration ledger header")

legacy_rows = [row.split("\t", 2) for row in rows[1:] if row.strip()]
if len(legacy_rows) != 111:
    raise SystemExit(f"CI migration ledger must contain 111 legacy workflows, got {len(legacy_rows)}")

legacy_names = [row[0] for row in legacy_rows]
if len(set(legacy_names)) != len(legacy_names):
    raise SystemExit("CI migration ledger contains duplicate legacy workflow names")

still_active = sorted(set(legacy_names) & actual)
if still_active:
    raise SystemExit(f"legacy workflows leaked back into active surface: {still_active}")

print("CI migration ledger OK: 111 legacy workflows accounted for")
