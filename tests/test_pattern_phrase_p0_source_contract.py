#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
SCENES = (ROOT / "scenes.h").read_text(encoding="utf-8")
SYNTH_UI = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
TEE = (ROOT / "src/midi/tee_midi_transport.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


sequencer = between(
    ENGINE,
    "void MiniAcid::processSequencerEvents(uint32_t absoluteTick) {",
    "void MiniAcid::generateAudioBuffer(",
)
require(
    "uint32_t barTick = absoluteTick % 384;" in sequencer,
    "P0 requires the current exact 384-tick bar modulus",
)
require(
    "currentStepIndex = barTick / 24;" in sequencer,
    "P0 requires the current 16 physical steps x 24 ticks scheduler",
)
require(
    "gridSteps" not in sequencer,
    "gridSteps unexpectedly entered the synth scheduler before PHRASE work",
)
require(
    "activePatternRuntimeEvents" in sequencer
    and "eventForSourceStep" in sequencer
    and "->startTick" in sequencer,
    "P2 Synth scheduling is not driven by prepared runtime source-step events",
)
require(
    "activeSynthPattern(0).steps" not in sequencer
    and "activeSynthPattern(1).steps" not in sequencer,
    "mutable SynthPattern timing re-entered the P2 executor",
)

selection = between(
    ENGINE,
    "void MiniAcid::applySongPositionSelection() {",
    "void MiniAcid::advanceSongPlayhead()",
)
require(
    "if (playing) publishPatternAllNotesOff_();" in selection,
    "Song physical-pattern selection must still publish PatternPlayer cleanup",
)

require(
    "uint8_t gridSteps = 16;" in SCENES,
    "FeelSettings.gridSteps persistence owner changed",
)
require(
    "case SynthTab::Notes:" in SYNTH_UI
    and "case SynthTab::Knobs:" in SYNTH_UI
    and "case SynthTab::More:" in SYNTH_UI,
    "current NOTES/KNOBS/MORE Tab contract changed",
)
require(
    "TeeMidiTransport" in TEE
    and "single musical owner" in TEE
    and "Both wires receive byte-identical" in TEE,
    "post-#419 USB/DIN tee ownership contract changed",
)

print("P0 source contract: OK")
