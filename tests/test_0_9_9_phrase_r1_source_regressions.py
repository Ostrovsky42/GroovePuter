#!/usr/bin/env python3
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = "2f9b6c7659bb4e0560c129ee33951f7adfcba8a4"

ALLOWED_SRC = {
    "src/dsp/miniacid_engine.h",
    "src/dsp/miniacid_engine.cpp",
    "src/dsp/phrase_crossbar_lifetime_runtime.h",
}
FROZEN_BACKENDS = [
    "src/input/internal_synth_output.h",
    "src/input/internal_synth_output.cpp",
    "src/midi/usb_midi_output.h",
    "src/midi/usb_midi_output.cpp",
    "src/input/musical_event.h",
]
FROZEN_GENERATION = [
    "src/generation/migration/phrase_execution.h",
    "src/generation/migration/phrase_execution.cpp",
    "src/generation/migration/phrase_semantic_result.h",
    "src/generation/composition/phrase_harmonic_clock_projection.h",
    "src/generation/composition/phrase_harmonic_timeline.h",
    "src/generation/roles/chord_progression.h",
    "src/generation/roles/chord_progression.cpp",
    "src/generation/roles/harmonic_rhythm.h",
]


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def fail(message: str) -> None:
    print(f"R1 SOURCE GUARD FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def block(text: str, start: str, end: str) -> str:
    if start not in text or end not in text.split(start, 1)[1]:
        fail(f"cannot isolate source block: {start}")
    return text.split(start, 1)[1].split(end, 1)[0]


def strip_cpp_comments(text: str) -> str:
    return re.sub(r"//.*?$|/\*.*?\*/", "", text,
                  flags=re.MULTILINE | re.DOTALL)


merge_base = git("merge-base", "HEAD", BASE)
if merge_base != BASE:
    fail(f"predecessor mismatch: {merge_base}")

changed_src = {
    line for line in git("diff", "--name-only", BASE, "HEAD", "--", "src/").splitlines()
    if line
}
if changed_src != ALLOWED_SRC:
    fail("unexpected src delta: " + ", ".join(sorted(changed_src)))
if git("diff", "--name-only", BASE, "HEAD", "--", "src/generation/"):
    fail("generation source changed")
for path in FROZEN_BACKENDS + FROZEN_GENERATION:
    if git("diff", "--name-only", BASE, "HEAD", "--", path):
        fail(f"frozen owner changed: {path}")

helper = (ROOT / "src/dsp/phrase_crossbar_lifetime_runtime.h").read_text()
helper_code = strip_cpp_comments(helper)
engine_h = (ROOT / "src/dsp/miniacid_engine.h").read_text()
engine_cpp = (ROOT / "src/dsp/miniacid_engine.cpp").read_text()

for fragment in (
    "enum class LogicalBoundaryDecision",
    "Release = 0",
    "Continue",
    "PhraseCrossBarLifetimeContext",
    "PhraseCrossBarHeldState",
    "PhraseCrossBarLifetimeExecutor",
    "phraseGenerationIdentity",
    "currentPhraseBarOrdinal",
    "continuesMask",
    "entersMask",
    "armOutgoingNote",
    "suppressOrdinaryGateExpiry",
    "advanceOrdinarySequentialBoundary",
    "consumeTerminatorBeforeNoteOn",
    "hardBarrierRelease",
):
    if fragment not in helper:
        fail(f"runtime owner fragment missing: {fragment}")

for forbidden in (
    "PickupPhrase", "PICKUP PHRASE", "Pivot", "GenreSettings", "recipe",
    "archetype", "patternAddress", "Harmonic", "ChordProgression",
    "BassRhythm", "ChordRhythm", "MelodicMotif", "gateLengthMultiplier",
    "holdSteps", "durationBars", "maxCrossBarSteps", "milliseconds",
    "std::vector", "std::map", "std::unordered_map", "std::mutex",
):
    if forbidden in helper_code:
        fail(f"musical/dynamic policy leaked into runtime owner: {forbidden}")
for pattern in (
    r"\bmalloc\s*\(",
    r"\bcalloc\s*\(",
    r"\brealloc\s*\(",
    r"\bnew\s+(?:\(|[A-Za-z_:][A-Za-z0-9_:<>]*\s*[\(\[])",
    r"\bdelete\s+",
):
    if re.search(pattern, helper_code):
        fail(f"heap operation in runtime owner: {pattern}")

for fragment in (
    "PhraseCrossBarLifetimeExecutor",
    "setPhraseCrossBarLifetimeContext",
    "suppressOrdinaryGateExpiry()",
    "consumeTerminatorBeforeNoteOn()",
    "armOutgoingNote(",
    "advanceOrdinarySequentialBoundary()",
    "publishPatternAllNotesOff_(bool preserveCrossBarHeldSynthB)",
    "heldCrossedBoundary()",
    "clearPhraseCrossBarLifetime_()",
    "releasePhraseCrossBarLifetime_(",
    "predecessorPatternCleanupFollows",
):
    if fragment not in engine_h + "\n" + engine_cpp:
        fail(f"MiniAcid execution wiring missing: {fragment}")
if "invalidatePhraseCrossBarLifetime_" in engine_h + engine_cpp:
    fail("old ambiguous hard-barrier helper survived")

clear_body = block(
    engine_cpp,
    "void MiniAcid::clearPhraseCrossBarLifetime_() {",
    "}\n\nvoid MiniAcid::releasePhraseCrossBarLifetime_",
)
if "hardBarrierRelease()" not in clear_body:
    fail("state-only clear no longer clears executor")
for forbidden in (
    "synthVoices_[1]->release()",
    "publishPatternNoteOff_",
    "publishPatternAllNotesOff_",
):
    if forbidden in clear_body:
        fail(f"state-only clear performs physical cleanup: {forbidden}")

release_body = block(
    engine_cpp,
    "void MiniAcid::releasePhraseCrossBarLifetime_(",
    "}\n\nint MiniAcid::liveNote",
)
for required in (
    "hardBarrierRelease()",
    "if (synthVoices_[1]) synthVoices_[1]->release();",
    "if (!predecessorPatternCleanupFollows) publishPatternNoteOff_(1);",
):
    if required not in release_body:
        fail(f"explicit hard-barrier release law missing: {required}")
if release_body.count("synthVoices_[1]->release()") != 1:
    fail("explicit hard-barrier helper must release internal Synth-B exactly once")

# Existing authoritative full-cleanup paths must clear R1 metadata only. Use
# explicit next-function delimiters so nested control-flow braces cannot shorten
# the inspected body.
transport_blocks = {
    "start": ("void MiniAcid::start() {", "\n}\n\nvoid MiniAcid::stop"),
    "stop": ("void MiniAcid::stop() {", "\n}\n\nvoid MiniAcid::pauseTransport"),
    "pauseTransport": (
        "void MiniAcid::pauseTransport() {",
        "\n}\n\nvoid MiniAcid::continueTransport",
    ),
    "continueTransport": (
        "void MiniAcid::continueTransport() {",
        "\n}\n\nvoid MiniAcid::liveNoteOn",
    ),
}
for function_name, (start, end) in transport_blocks.items():
    body = block(engine_cpp, start, end)
    if "clearPhraseCrossBarLifetime_();" not in body:
        fail(f"{function_name} does not use state-only clear")
    if "releasePhraseCrossBarLifetime_(" in body:
        fail(f"{function_name} duplicates internal held release")
    if "publishPatternAllNotesOff_();" not in body:
        fail(f"{function_name} lost authoritative PatternPlayer cleanup")

reset_body = block(
    engine_cpp,
    "void MiniAcid::reset() {",
    "\n}\n\nvoid MiniAcid::start",
)
if "clearPhraseCrossBarLifetime_();" not in reset_body:
    fail("reset does not clear R1 metadata before authoritative reset")
if (
    "synthVoices_[1]->reset();" not in reset_body
    or "publishPatternAllNotesOff_();" not in reset_body
):
    fail("reset authoritative cleanup changed")

for required in (
    "if (idx == 1 && muted) releasePhraseCrossBarLifetime_(true);",
    "releasePhraseCrossBarLifetime_(songMode_ && playing);",
    "if (!ordinaryPhraseTransition) releasePhraseCrossBarLifetime_(true);",
    "releasePatternSynthBHeldNote_(result.noteToRelease, false);",
):
    if required not in engine_cpp:
        fail(f"predecessor-cleanup parity wiring missing: {required}")

for required in (
    "if (patternEventQueue_ != queue) releasePhraseCrossBarLifetime_(false);",
    "bool MiniAcid::setPhraseCrossBarLifetimeContext(",
    "void MiniAcid::clearPhraseCrossBarLifetimeContext()",
):
    if required not in engine_cpp:
        fail(f"R1-owned cleanup path missing: {required}")
set_context = block(
    engine_cpp,
    "bool MiniAcid::setPhraseCrossBarLifetimeContext(",
    "\n}\n\nvoid MiniAcid::clearPhraseCrossBarLifetimeContext",
)
clear_context = block(
    engine_cpp,
    "void MiniAcid::clearPhraseCrossBarLifetimeContext() {",
    "\n}\n\nvoid MiniAcid::publishPatternNoteOn_",
)
if (
    "releasePhraseCrossBarLifetime_(false);" not in set_context
    or "releasePhraseCrossBarLifetime_(false);" not in clear_context
):
    fail("context replacement does not own exact old-note cleanup")

term = engine_cpp.find("consumeTerminatorBeforeNoteOn()")
release = engine_cpp.find("releasePatternSynthBHeldNote_(heldNote);", term)
new_on = engine_cpp.find("synthVoices_[synthIdx]->startNote", release)
new_publish = engine_cpp.find("publishPatternNoteOn_(synthIdx", new_on)
if not (0 <= term < release < new_on < new_publish):
    fail("terminator Release-before-NoteOn ordering changed")

gate = engine_cpp.find("if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0)")
suppress = engine_cpp.find("suppressOrdinaryGateExpiry()", gate)
legacy_release = engine_cpp.find("synthVoices_[1]->release()", suppress)
legacy_off = engine_cpp.find("publishPatternNoteOff_(1)", legacy_release)
if not (0 <= gate < suppress < legacy_release < legacy_off):
    fail("Synth-B gate expiry is not narrow fail-closed-to-legacy logic")

selective = engine_cpp.find(
    "publishPatternAllNotesOff_(bool preserveCrossBarHeldSynthB)"
)
skip_b = engine_cpp.find(
    "idx == 1 && preserveCrossBarHeldSynthB", selective
)
if not (0 <= selective < skip_b):
    fail("selective transition cleanup missing")

internal = (ROOT / "src/input/internal_synth_output.cpp").read_text()
if "event.source == MusicalEventSource::PatternPlayer" not in internal:
    fail("InternalSynthOutput PatternPlayer bypass changed")
for forbidden in (
    "InternalCrossBarPolicy",
    "MidiCrossBarPolicy",
    "CrossBarPolicy",
    "PhraseCrossBarLifetimeExecutor",
):
    for path in (
        ROOT / "src/input/internal_synth_output.cpp",
        ROOT / "src/midi/usb_midi_output.cpp",
    ):
        if forbidden in path.read_text():
            fail(f"backend-specific R1 policy leaked into {path.relative_to(ROOT)}")

for path in (
    ROOT / "tools/r1_patch_miniacid.py",
    ROOT / ".github/workflows/r1_apply_runtime_patch.yml",
):
    if path.exists():
        fail(f"temporary patch artifact survived: {path.relative_to(ROOT)}")

print("T1 exact C2 predecessor ancestry: OK")
print("T2 src/generation delta ZERO: OK")
print("T3 bounded runtime/no-heap source guard: OK")
print("R1 hard-barrier release parity source guard: OK")
print("R1 backend ownership/ABI firewall: OK")
