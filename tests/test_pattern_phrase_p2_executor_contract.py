#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


header = read("src/dsp/miniacid_engine.h")
engine = read("src/dsp/miniacid_engine.cpp")

# EXEC-0: startup publication is a playback precondition, not a lazy repair.
# Resident Scene truth is applied first, then the complete compact bank is
# rebuilt/published for PatternPagingService::activePageIndex(), and only then
# MiniAcid publishes the runtime page identity that AudioTask may select.
init = between(engine, "void MiniAcid::init()", "void MiniAcid::reset()")
for required in (
    "applySceneStateFromManager()",
    "rebuildPatternRuntimeEventBank()",
    "PatternPagingService::activePageIndex()",
    "setCurrentPage",
):
    require(required in init,
            f"startup does not establish prepared Pattern playback precondition: {required}")
require(init.index("applySceneStateFromManager()") <
        init.index("rebuildPatternRuntimeEventBank()") <
        init.index("setCurrentPage"),
        "startup publication order must be Scene -> compact bank -> runtime page")

# EXEC-1: RuntimeSynthPlaybackState is the sole Pattern lifetime decision owner.
for required in (
    '"../phrase/runtime_synth_playback.h"',
    "PhraseRuntime::RuntimeSynthPlaybackState patternPlaybackState_",
    "consumePatternPlaybackActions_",
):
    require(required in header or required in engine,
            f"missing common P2 executor ownership token: {required}")

# The sequencer must consume immutable prepared events, not mutable SynthStep
# material, for Synth A/B onset scheduling.
sequencer = between(engine,
                    "void MiniAcid::processSequencerEvents",
                    "void MiniAcid::generateAudioBuffer")
require("activePatternRuntimeEvents" in sequencer,
        "sequencer does not consume prepared immutable Pattern events")
for forbidden in (
    "activeSynthPattern(0).steps",
    "activeSynthPattern(1).steps",
):
    require(forbidden not in sequencer,
            f"sequencer still schedules Synth PATTERN from mutable steps: {forbidden}")

# Onset acceptance keeps the legacy RNG order: ghost first, then probability.
# Projection remains deterministic; only runtime can call rand().
trigger = between(engine,
                  "void MiniAcid::triggerSynthStep_",
                  "void MiniAcid::triggerDrumVoice_")
for required in (
    "RuntimeSynthEvent",
    "kEventGhost",
    "probability",
    "rand()",
    "acceptOnset",
    "consumePatternPlaybackActions_",
):
    require(required in trigger,
            f"executor onset path missing runtime-event ownership token: {required}")
require(trigger.index("kEventGhost") < trigger.index("probability"),
        "runtime onset decision must preserve ghost -> probability order")
require("activeSynthPattern" not in trigger,
        "executor onset still reads mutable SynthPattern material")

# One action consumer fans a single owner decision out to both backends.
consumer = between(engine,
                   "void MiniAcid::consumePatternPlaybackActions_",
                   "void MiniAcid::")
for required in (
    "RuntimeSynthPlaybackActionType::Start",
    "RuntimeSynthPlaybackActionType::Release",
    "RuntimeSynthPlaybackActionType::Retrigger",
    "startNote",
    "->release()",
    "publishPatternNoteOn_",
    "publishPatternNoteOff_",
):
    require(required in consumer,
            f"common action consumer missing backend fanout token: {required}")

# RETRIG is one owner decision but must be observed by BOTH backends as the same
# logical Release -> Start boundary. It must not extend the final lifetime.
retrigger_case = between(consumer,
                         "RuntimeSynthPlaybackActionType::Retrigger",
                         "default:")
require("->release()" in retrigger_case and
        "publishPatternNoteOff_" in retrigger_case,
        "RETRIG does not release both internal and MIDI lifetime first")
require("startNote" in retrigger_case and
        "publishPatternNoteOn_" in retrigger_case,
        "RETRIG does not restart both internal and MIDI from one decision")
require(retrigger_case.index("->release()") < retrigger_case.index("startNote"),
        "RETRIG internal trace must be Release -> Start")
require(retrigger_case.index("publishPatternNoteOff_") <
        retrigger_case.index("publishPatternNoteOn_"),
        "RETRIG MIDI trace must be NoteOff -> NoteOn")

# Legacy sample countdown fields may temporarily remain for compatibility, but
# after cutover they are dead state: no backend authority and no lifetime writes.
audio = between(engine,
                "void MiniAcid::generateAudioBuffer",
                "void MiniAcid::randomize303Pattern")
for forbidden in (
    "if (gateCountdownA_ > 0 && --gateCountdownA_ <= 0)",
    "if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0)",
):
    require(forbidden not in audio,
            "legacy gate countdown still owns backend release after P2 cutover")
for forbidden in (
    "gateCountdownA_ = dur",
    "gateCountdownB_ = dur",
    "gateCountdownA_ +=",
    "gateCountdownB_ +=",
):
    require(forbidden not in trigger,
            "legacy gate countdown still owns onset/TIE lifetime after P2 cutover")

# Legacy RETRIG counters may still schedule the boundary during this first
# executor commit, but they must route through RuntimeSynthPlaybackState and the
# common consumer instead of touching either backend directly.
retrigger_audio = between(audio,
                          "// Retrig Logic",
                          "for (int v = 0; v < NUM_DRUM_VOICES; ++v)")
require("acceptRetrigger" in retrigger_audio,
        "legacy RETRIG timing does not enter the common P2 lifetime owner")
require("consumePatternPlaybackActions_" in retrigger_audio,
        "legacy RETRIG timing bypasses common backend fanout")
for forbidden in (
    "synthVoices_[0]->startNote",
    "synthVoices_[1]->startNote",
    "publishPatternNoteOn_(0",
    "publishPatternNoteOn_(1",
):
    require(forbidden not in retrigger_audio,
            f"RETRIG retains backend-specific authority: {forbidden}")

print("P2 executor cutover ownership contract: OK")
