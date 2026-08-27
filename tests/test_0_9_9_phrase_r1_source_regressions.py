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


merge_base = git("merge-base", "HEAD", BASE)
if merge_base != BASE:
    fail(f"predecessor mismatch: {merge_base}")

changed_src = {
    line
    for line in git("diff", "--name-only", BASE, "HEAD", "--", "src/").splitlines()
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
engine_h = (ROOT / "src/dsp/miniacid_engine.h").read_text()
engine_cpp = (ROOT / "src/dsp/miniacid_engine.cpp").read_text()

required_helper = [
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
]
for fragment in required_helper:
    if fragment not in helper:
        fail(f"runtime owner fragment missing: {fragment}")

for forbidden in [
    "PickupPhrase",
    "PICKUP PHRASE",
    "Pivot",
    "GenreSettings",
    "recipe",
    "archetype",
    "patternAddress",
    "Harmonic",
    "ChordProgression",
    "BassRhythm",
    "ChordRhythm",
    "MelodicMotif",
    "gateLengthMultiplier",
    "holdSteps",
    "durationBars",
    "maxCrossBarSteps",
    "milliseconds",
    "std::vector",
    "std::map",
    "std::unordered_map",
    "std::mutex",
]:
    if forbidden in helper:
        fail(f"musical/dynamic policy leaked into runtime owner: {forbidden}")

for pattern in [
    r"\bmalloc\s*\(",
    r"\bcalloc\s*\(",
    r"\brealloc\s*\(",
    r"\bnew\s+[A-Za-z_:]",
    r"\bdelete\s+",
]:
    if re.search(pattern, helper):
        fail(f"heap operation in runtime owner: {pattern}")

required_engine = [
    "PhraseCrossBarLifetimeExecutor",
    "setPhraseCrossBarLifetimeContext",
    "suppressOrdinaryGateExpiry()",
    "consumeTerminatorBeforeNoteOn()",
    "armOutgoingNote(",
    "advanceOrdinarySequentialBoundary()",
    "publishPatternAllNotesOff_(bool preserveCrossBarHeldSynthB)",
    "heldCrossedBoundary()",
    "invalidatePhraseCrossBarLifetime_()",
]
for fragment in required_engine:
    if fragment not in engine_h + "\n" + engine_cpp:
        fail(f"MiniAcid execution wiring missing: {fragment}")

# Exact old-note release must precede the incoming physical NoteOn.
term = engine_cpp.find("consumeTerminatorBeforeNoteOn()")
release = engine_cpp.find("releasePatternSynthBHeldNote_(heldNote);", term)
new_on = engine_cpp.find("synthVoices_[synthIdx]->startNote", release)
new_publish = engine_cpp.find("publishPatternNoteOn_(synthIdx", new_on)
if not (0 <= term < release < new_on < new_publish):
    fail("terminator Release-before-NoteOn ordering changed")

# Gate expiry suppression must be the only exception around predecessor B expiry.
gate = engine_cpp.find("if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0)")
suppress = engine_cpp.find("suppressOrdinaryGateExpiry()", gate)
legacy_release = engine_cpp.find("synthVoices_[1]->release()", suppress)
legacy_off = engine_cpp.find("publishPatternNoteOff_(1)", legacy_release)
if not (0 <= gate < suppress < legacy_release < legacy_off):
    fail("Synth-B gate expiry is not narrow fail-open-to-legacy logic")

# Ordinary transition may preserve only Synth B; broad cleanup remains in place.
selective = engine_cpp.find("publishPatternAllNotesOff_(bool preserveCrossBarHeldSynthB)")
skip_b = engine_cpp.find("idx == 1 && preserveCrossBarHeldSynthB", selective)
if not (0 <= selective < skip_b):
    fail("selective transition cleanup missing")

# PatternPlayer policy remains in MiniAcid; backends remain pure executors.
internal = (ROOT / "src/input/internal_synth_output.cpp").read_text()
if "event.source == MusicalEventSource::PatternPlayer" not in internal:
    fail("InternalSynthOutput PatternPlayer bypass changed")

for forbidden in [
    "InternalCrossBarPolicy",
    "MidiCrossBarPolicy",
    "CrossBarPolicy",
    "PhraseCrossBarLifetimeExecutor",
]:
    for path in [ROOT / "src/input/internal_synth_output.cpp",
                 ROOT / "src/midi/usb_midi_output.cpp"]:
        if forbidden in path.read_text():
            fail(f"backend-specific R1 policy leaked into {path.relative_to(ROOT)}")

# No temporary write helpers or one-shot workflow may survive the production tree.
for path in [
    ROOT / "tools/r1_patch_miniacid.py",
    ROOT / ".github/workflows/r1_apply_runtime_patch.yml",
]:
    if path.exists():
        fail(f"temporary patch artifact survived: {path.relative_to(ROOT)}")

print("T1 exact predecessor/source scope: OK")
print("R1 generation firewall: OK")
print("R1 one backend-neutral lifetime owner: OK")
print("R1 backend policy duplication guard: OK")
print("R1 harmony/topology/name independence: OK")
print("R1 realtime/no-heap source guard: OK")
