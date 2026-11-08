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
    finish = text.index(end, begin + len(start))
    return text[begin:finish]


header = read("src/dsp/miniacid_engine.h")
engine = read("src/dsp/miniacid_engine.cpp")
generation_impl = read("src/generation/migration/quantized_generation_commit_impl.h")
generation_owner = read("src/generation/migration/quantized_generation_undo_owner_impl.h")
tb303 = read("src/dsp/mini_tb303.cpp")

# EXEC-0: startup publication is a playback precondition, not a lazy repair.
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

# EXEC-0B: playing generation already has one pending audible owner.
pending_struct = between(generation_impl,
                         "struct PendingGeneration",
                         "enum class SlotState")
require("RuntimePatternEventBuffer" in pending_struct,
        "pending generation does not retain prepared old-audible Pattern events")
require("runtime" in pending_struct.lower(),
        "pending generation compact event snapshot lacks an explicit runtime role")

fill_snapshot = between(generation_owner,
                        "inline void fillAudibleActivationSnapshot",
                        "inline void armActivationSlot")
require("activePatternRuntimeEvents" in fill_snapshot,
        "audible activation snapshot does not capture old prepared runtime material")

apply_persistent = between(generation_owner,
                           "inline void applyPreparedGenerationPersistent",
                           "inline void activatePreparedGenerationRuntime")
require("rebuildPatternRuntimeEventBank" in apply_persistent,
        "FULL generation COMMIT does not rebuild resident projections after swing change")
require("refreshPatternRuntimeEvents" in apply_persistent,
        "synth-only generation COMMIT does not refresh its resident runtime slot")

require("pendingAudibleSynthRuntime" in generation_owner,
        "existing pending owner lacks a prepared runtime-event accessor")

active_runtime = between(engine,
                         "MiniAcid::activePatternRuntimeEvents",
                         "void MiniAcid::")
require("pendingAudibleSynthRuntime" in active_runtime,
        "runtime Pattern selector ignores old-audible pending generation events")
require(active_runtime.index("pendingAudibleSynthRuntime") <
        active_runtime.index("selectForPage"),
        "pending audible runtime overlay must win before resident bank selection")

# EXEC-1: RuntimeSynthPlaybackState is the sole Pattern lifetime decision owner.
for required in (
    '"../phrase/runtime_synth_playback.h"',
    "PhraseRuntime::RuntimeSynthPlaybackState patternPlaybackState_",
    "consumePatternPlaybackActions_",
):
    require(required in header or required in engine,
            f"missing common P2 executor ownership token: {required}")

sequencer = between(engine,
                    "void MiniAcid::processSequencerEvents",
                    "void MiniAcid::generateAudioBuffer")
require("activePatternRuntimeEvents" in sequencer,
        "sequencer does not consume prepared immutable Pattern events")
require("eventForSourceStep" in sequencer,
        "sequencer lost physical source-step identity needed for legacy RNG order")
for forbidden in (
    "activeSynthPattern(0).steps",
    "activeSynthPattern(1).steps",
):
    require(forbidden not in sequencer,
            f"sequencer still schedules Synth PATTERN from mutable steps: {forbidden}")

for required in (
    "for (int sIdx = nominalStep - 1; sIdx <= nominalStep + 1; ++sIdx)",
    "triggerSynthStep_(0",
    "triggerSynthStep_(1",
    "triggerDrumVoice_",
):
    require(required in sequencer,
            f"legacy source-step trigger ordering token missing: {required}")
require(sequencer.index("triggerSynthStep_(0") <
        sequencer.index("triggerSynthStep_(1") <
        sequencer.index("triggerDrumVoice_"),
        "PATTERN executor changed legacy A -> B -> drums trigger/RNG ordering")

trigger = between(engine,
                  "void MiniAcid::triggerSynthStep_",
                  "void MiniAcid::triggerDrumVoice_")
for required in (
    "RuntimeSynthEvent",
    "kEventGhost",
    "event.probability",
    "rand()",
    "acceptOnset",
    "consumePatternPlaybackActions_",
):
    require(required in trigger,
            f"executor onset path missing runtime-event ownership token: {required}")
require(trigger.index("kEventGhost") < trigger.index("event.probability"),
        "runtime onset decision must preserve ghost -> probability order")
require("activeSynthPattern" not in trigger,
        "executor onset still reads mutable SynthPattern material")

consumer = between(engine,
                   "void MiniAcid::consumePatternPlaybackActions_",
                   "uint32_t MiniAcid::currentAbsoluteSubtick_")
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

# TB303 slide is an existing musical contract, not an implementation detail.
tb303_start = between(tb303,
                      "void TB303Voice::startNote",
                      "void TB303Voice::release")
require("slideFlag && gate && isVoiceActive()" in tb303_start,
        "TB303 legato-slide characterization changed unexpectedly")
require("slideReplacement" in consumer,
        "common consumer has no explicit legato-slide replacement translation")
require("skipInternalRelease" in consumer,
        "common consumer does not preserve internal gate for slide replacement")
require("publishPatternNoteOff_" in consumer and
        "publishPatternNoteOn_" in consumer,
        "slide replacement must still close/reopen Pattern MIDI ownership")

# SONG empty-track silence is also existing behavior. songPatternIndexForTrack()
# is the authoritative gate: in SONG mode a -1 track must not fall back to the
# current PATTERN selection just because the Scene still retains that index.
require("songPatternIndexForTrack" in active_runtime,
        "runtime selector bypasses authoritative SONG/PATTERN source gating")
require("if (patternIndex < 0) return patternRuntimeBank_.empty();" in active_runtime,
        "empty SONG synth track can resurrect the PATTERN-mode fallback")

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

for required in (
    "currentAbsoluteSubtick_",
    "releaseDue",
    "consumePatternPlaybackActions_",
):
    require(required in audio,
            f"audio loop does not consume common runtime expiry: {required}")
require("tickPhaseAccum_" in between(engine,
                                     "uint32_t MiniAcid::currentAbsoluteSubtick_",
                                     "void MiniAcid::triggerSynthStep_"),
        "subtick lifetime clock is not derived from the existing transport phase")

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
