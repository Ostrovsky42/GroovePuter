#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/tonal/chord_quality_projector.h").read_text()
SOURCE = (ROOT / "src/generation/tonal/chord_quality_projector.cpp").read_text()
MONO = (ROOT / "src/dsp/mono_synth_voice.h").read_text()
SWAPPABLE = (ROOT / "src/dsp/swappable_synth_voice.h").read_text()


def without_cpp_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


HEADER_CODE = without_cpp_comments(HEADER)
SOURCE_CODE = without_cpp_comments(SOURCE)

# P1 pitch-set projection is intentionally incapable of creating timing.
for forbidden in (
    "StepMask",
    "SynthPattern",
    "DrumPattern",
    "onsets",
    "continuations",
    "retrigger",
):
    assert forbidden not in HEADER_CODE, (
        f"timing/physical owner leaked into P1 API: {forbidden}"
    )

# Absolute MIDI remains delegated to the existing Tonal Projector.
assert "projectTonalIntent(projection)" in SOURCE_CODE
assert "TonalProjectionRequest projection" in SOURCE_CODE
assert "scaleDegreeToSemitone" in SOURCE_CODE

# No hidden arpeggiator, MIDI sender or physical synth adapter belongs here.
for forbidden in (
    "adaptTonalPlanToSynthPattern",
    "sendNoteOn",
    "sendNoteOff",
    "MusicalEventQueue",
    "liveNoteOn",
    "startNote(",
):
    assert forbidden not in SOURCE_CODE, (
        f"physical/timing side effect leaked into P1 projector: {forbidden}"
    )

# The current physical synth boundary is explicitly monophonic. P1 must not
# claim universal internal-audio feasibility until a later physical contract
# proves otherwise.
assert "class IMonoSynthVoice" in MONO
assert "virtual void startNote(float freqHz" in MONO
assert "class SwappableSynthVoice final : public IMonoSynthVoice" in SWAPPABLE

# Keep the feasibility cap explicit and small.
assert "constexpr uint8_t kMaxChordQualityTones = 4" in HEADER_CODE
assert "midiNotes[kMaxChordQualityTones]" in HEADER_CODE
assert "TriadPolarity" in HEADER_CODE

print("P1 chord-quality ownership source regressions: OK")
