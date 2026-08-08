#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT = (ROOT / "src/audio/audio_control_snapshot.h").read_text(encoding="utf-8")
GATE = (ROOT / "src/audio/audio_mutation_gate.h").read_text(encoding="utf-8")
VOICE_H = (ROOT / "src/dsp/swappable_synth_voice.h").read_text(encoding="utf-8")
VOICE_CPP = (ROOT / "src/dsp/swappable_synth_voice.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require("AudioControlSnapshotBuffer" in SNAPSHOT,
        "missing immutable audio control snapshot buffer")
require("slots_[2]" in SNAPSHOT,
        "snapshot contract must remain double-buffered")
require("std::vector" not in SNAPSHOT and "std::unique_ptr" not in SNAPSHOT,
        "audio control snapshot runtime must stay allocation-free")
require("new " not in SNAPSHOT,
        "audio control snapshot runtime must not allocate")

apply_pos = GATE.find(".applyPendingAtAudioBoundary();")
pause_pos = GATE.find("if (!pauseRequested_.load")
require(apply_pos >= 0 and pause_pos >= 0 and apply_pos < pause_pos,
        "routine controls must be applied before structural pause acknowledgement")
require("captureAllAfterStructuralMutation" in GATE,
        "guarded direct setters must refresh the control snapshot baseline")
require("setStructuralMutationActive(true)" in GATE and
        "setStructuralMutationActive(false)" in GATE,
        "AudioMutationGate must classify its outer scope as structural")

require("SynthParameterControlSnapshot" in VOICE_H,
        "SwappableSynthVoice must own a synth-parameter snapshot")
require("controlSnapshots_" in VOICE_H,
        "SwappableSynthVoice must own the double-buffered control store")
require("shouldQueueRoutineControls" in VOICE_CPP,
        "routine synth parameters must choose the snapshot path")
require("applyPendingControlSnapshotAtAudioBoundary_" in VOICE_CPP,
        "snapshot must have an explicit block-boundary consumer")
require("captureControlStateAfterStructuralMutation_" in VOICE_CPP,
        "structural mutation must refresh the snapshot baseline")

process_start = VOICE_CPP.index("float SwappableSynthVoice::process()")
parameter_start = VOICE_CPP.index("uint8_t SwappableSynthVoice::parameterCount()")
process_body = VOICE_CPP[process_start:parameter_start]
require("applyPendingControlSnapshotAtAudioBoundary_" not in process_body,
        "do not poll/apply control snapshots per sample")
require("controlSnapshots_.read()" not in process_body,
        "audio snapshot reads belong at block boundary, not process()")

setter_start = VOICE_CPP.index("void SwappableSynthVoice::setParameterNormalized")
getter_start = VOICE_CPP.index("float SwappableSynthVoice::getParameterNormalized")
setter_body = VOICE_CPP[setter_start:getter_start]
require("publishControlShadow_();" in setter_body,
        "routine setter must publish a complete control snapshot")
require("if (runtime.shouldQueueRoutineControls())" in setter_body,
        "routine setter must avoid direct DSP mutation while renderer is live")

print("audio control snapshot source regressions: PASS")
