#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INSTRUMENT = ROOT / "tests/fs1d/instrument_pending_residency.py"
BUILD = ROOT / "tests/fs1d/build_pending_residency.sh"
INO = ROOT / "GroovePuter.ino"

instrument_text = INSTRUMENT.read_text(encoding="utf-8")
build_text = BUILD.read_text(encoding="utf-8")
source_text = INO.read_text(encoding="utf-8")

required = (
    "pendingMaterialAddress(0)",
    "pendingMaterialAddress(1)",
    "heap_caps_get_free_size",
    "heap_caps_get_minimum_free_size",
    "heap_caps_get_largest_free_block",
    "uxTaskGetStackHighWaterMark(g_audioTaskHandle)",
    "perfStats.audioUnderruns.load",
    "[FS1D_SAMPLE]",
    "pendingA_present=",
    "pendingB_present=",
    "pendingA_address=",
    "pendingB_address=",
    "pending_distinct=",
    "free_internal=",
    "minimum_free_internal=",
    "largest_internal=",
    "task_stack_hwm_bytes=",
    "audio_underruns=",
)
for needle in required:
    if needle not in instrument_text:
        raise SystemExit(f"FS1D instrumentation missing {needle!r}")

# The verifier must remain an observer. These tokens would indicate that the
# diagnostic itself started constructing the future NEXT path or a stress load.
# MELODY_CENSUS_TICK is intentionally not checked here because the instrumenter
# uses the existing production call as a patch anchor. Its before/after count is
# checked below instead, which proves the FS1D probe did not add a census call.
for forbidden in (
    "RuntimeSynthEventBuffer",
    "stagePendingMaterial(",
    "activatePendingMaterial(",
    "GROOVEPUTER_MELODY_CENSUS",
    "heap_caps_malloc",
    "malloc(",
    "calloc(",
    "realloc(",
):
    if forbidden in instrument_text:
        raise SystemExit(f"FS1D instrumentation contains forbidden token {forbidden!r}")

for needle in (
    "mktemp -d /tmp/grooveputer-fs1d.",
    "rsync -a --delete",
    "instrument_pending_residency.py",
    "build_cardputer_dynbuffers.sh",
):
    if needle not in build_text:
        raise SystemExit(f"FS1D build wrapper missing {needle!r}")

with tempfile.TemporaryDirectory(prefix="fs1d-source-test-") as tmp:
    root = Path(tmp)
    shutil.copy2(INO, root / "GroovePuter.ino")
    subprocess.run(
        [sys.executable, str(INSTRUMENT), str(root)],
        check=True,
        cwd=ROOT,
    )
    instrumented = (root / "GroovePuter.ino").read_text(encoding="utf-8")

if instrumented.count("[FS1D_SAMPLE]") != 1:
    raise SystemExit("FS1D sample formatter must be injected exactly once")
if instrumented.count("pollFs1dPendingResidency();") != 1:
    raise SystemExit("FS1D poll hook must be injected exactly once")
if instrumented.count("MELODY_CENSUS_TICK(") != source_text.count("MELODY_CENSUS_TICK("):
    raise SystemExit("FS1D must not add or remove the existing melody-census call")
if "15000" not in instrumented:
    raise SystemExit("FS1D telemetry must remain low-rate")

print("FS1D pending-residency instrumentation source proof PASS")
