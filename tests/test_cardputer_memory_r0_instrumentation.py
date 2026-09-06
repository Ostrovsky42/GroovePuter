#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INSTRUMENTER = ROOT / "scripts/instrument_cardputer_memory_runtime.py"
BUILD_SCRIPT = ROOT / "scripts/build_cardputer_memory_baseline.sh"

PATCH_TARGETS = (
    "GroovePuter.ino",
    "src/platform/cardputer_smf_player.h",
    "src/platform/cardputer_smf_player.cpp",
    "src/platform/cardputer_smf_player_registry.h",
    "src/platform/cardputer_smf_player_registry.cpp",
    "src/platform/cardputer_usb_midi_transport.h",
    "src/platform/cardputer_usb_midi_transport.cpp",
    "src/ui/pages/phrase_page.cpp",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def copy_fixture(destination: Path) -> None:
    for relative in PATCH_TARGETS:
        source = ROOT / relative
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


build = BUILD_SCRIPT.read_text(encoding="utf-8")
require("MEMORY_RUNTIME_MODE" in build,
        "runtime build must expose the instrumentation mode explicitly")
require("--mode" in build,
        "runtime build must pass the selected mode to the existing instrumenter")

with tempfile.TemporaryDirectory(prefix="grooveputer-memory-r0-test.") as temp:
    fixture = Path(temp) / "GroovePuter"
    fixture.mkdir()
    copy_fixture(fixture)

    result = subprocess.run(
        [sys.executable, str(INSTRUMENTER), "--mode", "r0", str(fixture)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    require(result.returncode == 0,
            "R0 instrumentation mode must transform the authoritative source: "
            + result.stdout)

    ino = (fixture / "GroovePuter.ino").read_text(encoding="utf-8")
    smf = (fixture / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8")
    phrase = (fixture / "src/ui/pages/phrase_page.cpp").read_text(
        encoding="utf-8")

    # R0 observes the real product startup/runtime. It must not run the older
    # synthetic P3 workload or raw key-scan trace.
    for forbidden in (
        "P3DramCharacterization::begin",
        "P3DramCharacterization::poll",
        "P3KeyScanTrace::observe",
    ):
        require(forbidden not in ino,
                f"R0 mode must not execute synthetic diagnostic workload: {forbidden}")
    require("beginPhraseMemoryProbe();" not in phrase,
            "R0 mode must not inject the legacy PHRASE operation probe")

    # The production initialization topology stays present and in the same
    # source flow. R0 may bracket these operations, not remove or defer them.
    for production_anchor in (
        "startAudioTask();",
        "g_miniAcidInstance.preallocateConstrainedDelayBuffers();",
        "g_sceneStorage.initializeStorage();",
        "beginCardputerSmfPlayerService()",
        "registerCardputerUsbMidiSink(",
        "g_miniAcidInstance.init();",
        "g_miniAcid->sampleIndex.scanDirectory(\"/sd/samples\");",
        "new (std::nothrow) MiniAcidDisplay(",
        "drawUI();",
    ):
        require(production_anchor in ino,
                f"R0 must preserve production operation: {production_anchor}")

    # Startup census: phase labels intentionally describe boundaries, not
    # object sizes. SMF timing/task boundaries live inside begin().
    for phase in (
        "r0-before-audio-task",
        "r0-after-audio-task",
        "r0-before-dsp-buffers",
        "r0-after-dsp-buffers",
        "r0-before-sd",
        "r0-after-sd",
        "r0-before-smf",
        "r0-after-smf",
        "r0-before-midi-dispatch",
        "r0-after-midi-dispatch",
        "r0-before-miniacid",
        "r0-after-miniacid",
        "r0-before-samples",
        "r0-after-samples",
        "r0-before-ui-root",
        "r0-after-ui-root",
        "r0-after-first-draw",
        "r0-setup-complete",
    ):
        require(phase in ino, f"R0 startup checkpoint missing: {phase}")

    for phase in (
        "r0-smf-begin",
        "r0-after-smf-timing-document",
        "r0-after-smf-timing-map",
        "r0-after-smf-command-queue",
        "r0-after-smf-task",
    ):
        require(phase in smf, f"R0 SMF checkpoint missing: {phase}")

    # Retained localization is deliberately coarse and must not use
    # markBootStage(), because that helper also prints on every loop edge.
    for stage in (110, 112, 114, 116, 118, 120, 122, 124, 126):
        require(f"recordCardputerMemoryR0Stage({stage});" in ino,
                f"coarse retained R0 stage missing: {stage}")
        require(f"markBootStage({stage}" not in ino,
                f"R0 loop stage {stage} must not emit serial logging")

    # Existing heap/HWM observations remain available in the unified image.
    for field in (
        "freeInternal8",
        "largestInternal8",
        "integrity",
        "loopStackFreeBytes",
        "audioStackFreeBytes",
        "smfStackFreeBytes",
        "dispatchStackFreeBytes",
    ):
        require(field in ino, f"R0 unified telemetry missing: {field}")

print("Cardputer MEMORY-R0 instrumentation contract: PASS")
