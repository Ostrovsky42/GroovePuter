#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
failures: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


ino = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
require(
    "beginCardputerSmfPlayerService()" not in ino,
    "Lazy SMF regression: setup still eagerly calls beginCardputerSmfPlayerService()",
)

registry = (ROOT / "src/platform/cardputer_smf_player_registry.cpp").read_text(encoding="utf-8")
require("ensureStarted()" in registry, "Lazy SMF owner missing ensureStarted()")

# The historical source-regression modules contain broader stale assertions that
# are repaired by the separate test-liveness workstream. MEMORY-R1 owns only the
# two eager-SMF expectations it changes, so guard those exact oracles here.
source_oracle = (ROOT / "tests/test_source_regressions.py").read_text(encoding="utf-8")
require(
    'sketch.index("beginCardputerSmfPlayerService();")' not in source_oracle,
    "Lazy SMF source oracle still requires boot-time player startup",
)
require(
    "SMF task/timing storage must remain lazy at boot" in source_oracle,
    "Lazy SMF source oracle was not updated to the accepted ownership contract",
)
performance_oracle = (
    ROOT / "tests/_performance_source_regressions_base.py"
).read_text(encoding="utf-8")
require(
    '"beginCardputerSmfPlayerService" in\n                (ROOT / "GroovePuter.ino")' not in performance_oracle,
    "Lazy SMF performance oracle still requires setup ownership",
)
require(
    "SMF runtime must remain lazy until the first player command" in performance_oracle,
    "Lazy SMF performance oracle was not updated to the accepted ownership contract",
)

fatfs = (ROOT / "scripts/build_fatfs_dynbuffers_candidate.sh").read_text(encoding="utf-8")
for token in (
    "858a988d6eb90f54661abe282c523879c6ad0116",
    "CONFIG_FATFS_PER_FILE_CACHE",
    "CONFIG_WL_SECTOR_SIZE",
    "ff_memalloc",
    "ff_memfree",
    "shlex",
):
    require(token in fatfs, f"FS1B hardened candidate contract missing {token}")

cardputer_builder = (ROOT / "scripts/build_cardputer_dynbuffers.sh").read_text(encoding="utf-8")
for token in ("ld_libs", "-lfatfs", "libfatfs.a"):
    require(token in cardputer_builder, f"FS1B final link replacement missing {token}")
require(
    "compiler.sdk.path" not in cardputer_builder,
    "FS1B still uses the older SDK-overlay link path instead of in-place repeated-slot replacement",
)

wavetable_h = (ROOT / "src/dsp/audio_wavetables.h").read_text(encoding="utf-8")
wavetable_cpp = (ROOT / "src/dsp/audio_wavetables.cpp").read_text(encoding="utf-8")
for token in ("lookupTriangle", "triangleTable_"):
    require(token not in wavetable_h + wavetable_cpp, f"WT1 dead triangle representation survived: {token}")
for token in ("lookupSaw", "sawTable_", "lookupSquare", "squareTable_", "lookupSine", "sineTable_"):
    require(token in wavetable_h + wavetable_cpp, f"WT1 scope breach: surviving wavetable owner missing {token}")

instrumenter = (ROOT / "scripts/instrument_cardputer_memory_runtime.py").read_text(encoding="utf-8")
for token in (
    "P3_DRAM_CHARACTERIZATION",
    "P3DramCharacterization::",
    "P3_AUDIBLE_AB",
    "P3AudibleAB::",
):
    require(token not in instrumenter, f"Generic memory runtime still auto-drives musical state: {token}")

# Exercise the transformer, not only its source text. The generic runtime image
# may add passive telemetry/probes, but must both apply to the live M4 source and
# avoid injecting an autonomous P3 player.
with tempfile.TemporaryDirectory(prefix="memory-r1-closure-") as tmp:
    staged = Path(tmp) / "GroovePuter"
    shutil.copytree(
        ROOT,
        staged,
        ignore=shutil.ignore_patterns(".git", "build", ".pytest_cache", "__pycache__"),
    )
    try:
        subprocess.run(
            [sys.executable, str(staged / "scripts/instrument_cardputer_memory_runtime.py"), str(staged)],
            check=True,
            cwd=staged,
        )
    except subprocess.CalledProcessError as exc:
        failures.append(
            f"Generic memory runtime instrumenter no longer applies to live M4 source (exit={exc.returncode})"
        )
    else:
        staged_ino = (staged / "GroovePuter.ino").read_text(encoding="utf-8")
        require("[MEM-BASE]" in staged_ino, "Runtime instrumentation lost passive memory telemetry")
        for token in ("P3DramCharacterization", "P3AudibleAB", "P3_DRAM_CHARACTERIZATION", "P3_AUDIBLE_AB"):
            require(token not in staged_ino, f"Instrumented runtime autonomously contains {token}")

if failures:
    print("MEMORY-R1 PRODUCT CLOSURE: RED")
    for failure in failures:
        print(f" - {failure}")
    raise SystemExit(1)

print("MEMORY-R1 PRODUCT CLOSURE: PASS")
