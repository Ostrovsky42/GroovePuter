#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ACCEPTED_MEMORY_SHA = "9fa4bb12e3c4826720f16ace865a2c4de8e4cc0c"


def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one anchor in {path}, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def git_show(path: str) -> str:
    return subprocess.check_output(
        ["git", "show", f"{ACCEPTED_MEMORY_SHA}:{path}"],
        cwd=ROOT,
        text=True,
    )


# 1. Bank the already-implemented LazyCardputerSmfPlayer instead of defeating it
# during setup. The registry/API remains intact for explicit callers and tests.
replace_once(
    ROOT / "GroovePuter.ino",
    '''  // Reserve the SMF task stack and bounded timing buffers before DSP and lazy
  // UI allocations fragment the DRAM-only Cardputer ADV heap.
  screenLog("4c. SMF Runtime...");
  markBootStage(84, "before SMF runtime init");
  if (!beginCardputerSmfPlayerService()) {
    Serial.println("[WARN] SMF runtime unavailable; groovebox remains usable");
  }
  markBootStage(85, "after SMF runtime init");
''',
    '''  // LazyCardputerSmfPlayer registers itself statically. Allocate the SMF task
  // stack and parser/timing storage only on the first player command; eager boot
  // startup was the measured 7.0-7.4 KiB residency loss banked before MEMORY-R1.
  screenLog("4c. SMF Runtime (lazy)...");
  markBootStage(84, "SMF runtime deferred");
  markBootStage(85, "after SMF runtime deferral");
''',
    "lazy SMF setup",
)

# 2. Replace the older M4 FS1 implementation with the final hardware-accepted
# FS1B compiler/source contract, while retaining the established M4 filename.
accepted_fatfs = git_show("scripts/build_fatfs_dynbuffers_candidate.sh")
(ROOT / "scripts/build_fatfs_dynbuffers_candidate.sh").write_text(
    accepted_fatfs, encoding="utf-8"
)

accepted_builder = git_show("scripts/build_cardputer_memory_r1_fs1b.sh")
accepted_builder = accepted_builder.replace(
    "# Build the MEMORY-R1 FS1B Cardputer image without modifying the shared Arduino\n",
    "# Build the accepted FS1B Cardputer image without modifying the shared Arduino\n",
    1,
)
accepted_builder = accepted_builder.replace(
    '${PROJECT_ROOT}/build/cardputer-memory-r1-fs1b',
    '${PROJECT_ROOT}/build/cardputer-adv-dynbuffers',
    1,
)
accepted_builder += '''

# Runtime heap recovery does not waive the product static-DRAM gate.
bash "${SCRIPT_DIR}/check_cardputer_dram_budget.sh" \
  "${BUILD_PATH}/GroovePuter.ino.elf"

echo "=== FS1B dynamic-FatFs Cardputer build PASS ==="
'''
(ROOT / "scripts/build_cardputer_dynbuffers.sh").write_text(
    accepted_builder, encoding="utf-8"
)

# 3. WT1: remove only the proven-dead 4 KiB triangle representation.
replace_once(
    ROOT / "src/dsp/audio_wavetables.h",
    '''  static inline float lookupTriangle(uint32_t phase) {
    uint32_t index = (phase >> 22) & kWavetableMask;
    return triangleTable_[index];
  }
  
''',
    "",
    "WT1 triangle accessor",
)
replace_once(
    ROOT / "src/dsp/audio_wavetables.h",
    "  static float triangleTable_[kWavetableSize];\n",
    "",
    "WT1 triangle declaration",
)
replace_once(
    ROOT / "src/dsp/audio_wavetables.cpp",
    "float Wavetable::triangleTable_[kWavetableSize];\n",
    "",
    "WT1 triangle definition",
)
replace_once(
    ROOT / "src/dsp/audio_wavetables.cpp",
    '''  // Triangle wave
  for (uint32_t i = 0; i < kWavetableSize; i++) {
    if (i < kWavetableSize / 2) {
      triangleTable_[i] = 4.0f * static_cast<float>(i) / static_cast<float>(kWavetableSize) - 1.0f;
    } else {
      triangleTable_[i] = 3.0f - 4.0f * static_cast<float>(i) / static_cast<float>(kWavetableSize);
    }
  }
  
''',
    "",
    "WT1 triangle initialization",
)

# 4. Generic memory instrumentation is passive. Keep heap/stack and operation
# probes, but remove the five-minute autonomous P3 sequenced-source scenario.
instrumenter = ROOT / "scripts/instrument_cardputer_memory_runtime.py"
text = instrumenter.read_text(encoding="utf-8")
old_loop = '''loop_anchor = \'\'\'void loop() {
  M5Cardputer.update();\'\'\'
loop_injection = \'\'\'void loop() {
  M5Cardputer.update();
  pollCardputerMemoryBaseline();\'\'\'
'''
new_loop = '''loop_anchor = \'\'\'void loop() {
  // Diagnostic only; compiles to (void)0 unless GROOVEPUTER_MELODY_CENSUS is set.
  MELODY_CENSUS_TICK(g_miniAcid && g_miniAcid->isPlaying());
  M5Cardputer.update();\'\'\'
loop_injection = \'\'\'void loop() {
  // Diagnostic only; compiles to (void)0 unless GROOVEPUTER_MELODY_CENSUS is set.
  MELODY_CENSUS_TICK(g_miniAcid && g_miniAcid->isPlaying());
  M5Cardputer.update();
  pollCardputerMemoryBaseline();\'\'\'
'''
if text.count(old_loop) != 1:
    raise SystemExit("runtime instrumenter loop anchor drift")
text = text.replace(old_loop, new_loop, 1)
start_marker = "# P3 DRAM characterization."
for_marker = "for anchor, replacement, label in ("
start = text.find(start_marker)
end = text.find(for_marker, start)
if start < 0 or end < 0:
    raise SystemExit("runtime instrumenter P3 block anchors missing")
text = text[:start] + text[end:]
for line in (
    '    (p3_include_anchor, p3_include_injection, "p3-include"),\n',
    '    (p3_setup_anchor, p3_setup_injection, "p3-setup"),\n',
    '    (p3_loop_anchor, p3_loop_injection, "p3-loop"),\n',
):
    if text.count(line) != 1:
        raise SystemExit(f"runtime instrumenter tuple drift: {line.strip()}")
    text = text.replace(line, "", 1)
instrumenter.write_text(text, encoding="utf-8")

# 5. Repair stale test oracles which encoded eager SMF allocation as policy.
replace_once(
    ROOT / "tests/test_source_regressions.py",
    '''    smf_runtime_pos = sketch.index("beginCardputerSmfPlayerService();")
    require(early_sd_pos < smf_runtime_pos < engine_init_pos,
            "SMF task and timing storage must be reserved before DSP/UI fragmentation")

    midi_runtime_pos = sketch.index("registerCardputerUsbMidiSink(")
    require(smf_runtime_pos < midi_runtime_pos < engine_init_pos,
            "MIDI dispatcher stack must be reserved before engine heap fragmentation")
''',
    '''    smf_runtime_pos = sketch.index('screenLog("4c. SMF Runtime (lazy)...")')
    require(early_sd_pos < smf_runtime_pos < engine_init_pos,
            "SMF deferral marker must remain between SD setup and engine init")
    require("beginCardputerSmfPlayerService()" not in sketch,
            "SMF task/timing storage must remain lazy at boot")

    midi_runtime_pos = sketch.index("registerCardputerUsbMidiSink(")
    require(smf_runtime_pos < midi_runtime_pos < engine_init_pos,
            "MIDI dispatcher stack must still start before engine heap fragmentation")
''',
    "lazy SMF source-regression oracle",
)
replace_once(
    ROOT / "tests/_performance_source_regressions_base.py",
    '''    require("beginCardputerSmfPlayerService" in player_registry and
            "beginCardputerSmfPlayerService" in
                (ROOT / "GroovePuter.ino").read_text(encoding="utf-8"),
            "SMF runtime must be reserved explicitly during setup")
''',
    '''    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    require("ensureStarted()" in player_registry and
            "beginCardputerSmfPlayerService()" not in sketch,
            "SMF runtime must remain lazy until the first player command")
''',
    "lazy SMF performance oracle",
)

print("MEMORY-R1 product closure materialized")
