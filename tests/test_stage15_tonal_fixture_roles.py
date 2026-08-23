#!/usr/bin/env python3
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CURRENT = ROOT / "tests/data/stage15_tonal_enabled_baseline.tsv"
PRE_F13 = ROOT / "tests/data/stage15_tonal_enabled_pre_f13_baseline.tsv"
F13_FROZEN = ROOT / "tests/data/stage15_tonal_enabled_f13_baseline.tsv.gz.b64"
BASELINE_WORKFLOW = ROOT / ".github/workflows/stage15-tonal-baseline.yml"
F08_WORKFLOW = ROOT / ".github/workflows/0-9-9-f08-harmonic-rhythm.yml"
BOUNDARY = ROOT / "tests/test_stage15_tonal_corpus_boundary.py"
F13_TEST = ROOT / "tests/test_stage15_tonal_f13_corpus.py"

CURRENT_NAME = "stage15_tonal_enabled_baseline.tsv"
PRE_F13_NAME = "stage15_tonal_enabled_pre_f13_baseline.tsv"
F13_FROZEN_NAME = "stage15_tonal_enabled_f13_baseline.tsv.gz.b64"

EXPECTED_PRE_F13_BLOB = "4bd456235b49a98deee2dc6778287353ad7721b5"
EXPECTED_F13_FROZEN_BLOB = "64fd07afc99dc4652f798fd41a56213768a6f42f"


def git_blob_sha(path: Path) -> str:
    data = path.read_bytes()
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def consumers(needle: str):
    hits = set()
    skip_parts = {".git", "build", ".pio"}
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        if any(part in skip_parts for part in path.parts):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        if needle in text:
            hits.add(path.relative_to(ROOT).as_posix())
    return hits


assert CURRENT.exists()
assert PRE_F13.exists()
assert F13_FROZEN.exists()
assert CURRENT != PRE_F13
assert PRE_F13 != F13_FROZEN
assert CURRENT != F13_FROZEN

assert git_blob_sha(PRE_F13) == EXPECTED_PRE_F13_BLOB, (
    "historical PRE-F13 fixture changed: "
    f"{git_blob_sha(PRE_F13)} != {EXPECTED_PRE_F13_BLOB}"
)
assert git_blob_sha(F13_FROZEN) == EXPECTED_F13_FROZEN_BLOB, (
    "frozen F13 snapshot changed: "
    f"{git_blob_sha(F13_FROZEN)} != {EXPECTED_F13_FROZEN_BLOB}"
)
assert CURRENT.read_bytes() != PRE_F13.read_bytes(), (
    "current Stage15 golden collapsed back onto historical PRE-F13 evidence"
)

current_consumers = consumers(CURRENT_NAME)
pre_f13_consumers = consumers(PRE_F13_NAME)
f13_consumers = consumers(F13_FROZEN_NAME)

expected_current_consumers = {
    ".github/workflows/stage15-tonal-baseline.yml",
    "tests/test_stage15_tonal_fixture_roles.py",
}
expected_pre_f13_consumers = {
    ".github/workflows/stage15-tonal-baseline.yml",
    "tests/test_stage15_tonal_corpus_boundary.py",
    "tests/test_stage15_tonal_fixture_roles.py",
}
expected_f13_consumers = {
    ".github/workflows/0-9-9-f08-harmonic-rhythm.yml",
    ".github/workflows/stage15-tonal-baseline.yml",
    "tests/test_stage15_tonal_fixture_roles.py",
}

assert current_consumers == expected_current_consumers, (
    f"unexpected CURRENT GOLDEN consumers: {sorted(current_consumers)}"
)
assert pre_f13_consumers == expected_pre_f13_consumers, (
    f"unexpected HISTORICAL PRE-F13 consumers: {sorted(pre_f13_consumers)}"
)
assert f13_consumers == expected_f13_consumers, (
    f"unexpected FROZEN F13 SNAPSHOT consumers: {sorted(f13_consumers)}"
)

baseline_workflow = BASELINE_WORKFLOW.read_text(encoding="utf-8")
f08_workflow = F08_WORKFLOW.read_text(encoding="utf-8")
boundary = BOUNDARY.read_text(encoding="utf-8")
f13_test = F13_TEST.read_text(encoding="utf-8")

historical_start = baseline_workflow.index(
    "- name: Verify historical PRE-F13 to F-13 expression ownership delta"
)
historical_end = baseline_workflow.index(
    "- name: Compare current accepted Stage15 tonal golden", historical_start
)
historical_block = baseline_workflow[historical_start:historical_end]
assert PRE_F13_NAME in historical_block
assert "stage15_tonal_enabled_f13_expected.tsv" in historical_block
assert CURRENT_NAME not in historical_block

current_start = baseline_workflow.index(
    "- name: Compare current accepted Stage15 tonal golden"
)
current_end = baseline_workflow.index(
    "- name: Upload frozen F-13 tonal corpus", current_start
)
current_block = baseline_workflow[current_start:current_end]
assert CURRENT_NAME in current_block
assert "stage15_tonal_enabled_actual.tsv" in current_block
assert PRE_F13_NAME not in current_block
assert F13_FROZEN_NAME not in current_block

assert F13_FROZEN_NAME in f08_workflow
assert PRE_F13_NAME not in f08_workflow
assert CURRENT_NAME not in f08_workflow
assert "classify_0_9_9_f08_stage15_drift.py" in f08_workflow

assert PRE_F13_NAME in boundary
assert CURRENT_NAME not in boundary

assert "if articulation_changes == 0:" in f13_test
assert "if full_changes == 0:" in f13_test
assert "expected removal of inherited destination dynamics to be observable" in f13_test
assert "expected self-contained tonal steps to replace inherited destination state" in f13_test

print(
    "Stage15 tonal fixture roles: OK "
    "(CURRENT GOLDEN / HISTORICAL PRE-F13 / FROZEN F13 SNAPSHOT separated)"
)
