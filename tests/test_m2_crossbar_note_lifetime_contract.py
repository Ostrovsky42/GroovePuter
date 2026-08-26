#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def window(text: str, marker: str, size: int = 5000) -> str:
    start = text.find(marker)
    require(start >= 0, f"missing source marker: {marker}")
    return text[start:start + size]


def main() -> None:
    scenes = (ROOT / "scenes.h").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    genre = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")
    advanced = (ROOT / "src/dsp/advanced_pattern_generator.h").read_text(encoding="utf-8")
    adapter = (ROOT / "src/generation/migration/tonal_pattern_adapter.cpp").read_text(encoding="utf-8")
    tonal = (ROOT / "src/generation/tonal/tonal_materializer.cpp").read_text(encoding="utf-8")
    motif = (ROOT / "src/generation/roles/melodic_motif.cpp").read_text(encoding="utf-8")
    internal = (ROOT / "src/input/internal_synth_output.cpp").read_text(encoding="utf-8")
    midi = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")

    synth_step = window(scenes, "struct SynthStep", 600)
    require("int8_t note = -1;" in synth_step, "SynthStep note/rest contract moved")
    for token in ("duration", "gate", "tie", "hold", "continuation"):
        require(token not in synth_step.lower(),
                f"SynthStep unexpectedly gained explicit {token} lifetime state")

    # Legacy -2 exists only as playback/editor compatibility. It is not emitted
    # by the tonal adapter and depends on an already-live gate.
    require("if (step.note == -2) { // TIE" in engine,
            "MiniAcid legacy -2 playback sentinel moved")
    require("gateCountdownA_ > 0" in engine and "gateCountdownB_ > 0" in engine,
            "legacy -2 no longer depends on an active gate")
    require("std::array<uint8_t, 16> tie{};" in advanced,
            "legacy generator tie mask moved")
    require("tied.note = -2" not in adapter,
            "tonal adapter unexpectedly emits legacy -2")

    # Ordinary production gate length remains shorter than one step.
    require("params.gateLengthMultiplier > 1.0f ? 1.0f" in genre,
            "production gate multiplier cap moved")
    require("float vMult = (synthIdx == 0) ? 0.85f : 1.05f;" in engine,
            "per-lane gate multiplier moved")
    require("if (synthIdx == 1 && effectiveGateMult > 0.98f) effectiveGateMult = 0.98f;" in engine,
            "Synth B effective gate cap moved")
    require("gateCountdownA_ = dur;" in engine and "gateCountdownB_ = dur;" in engine,
            "sequencer gate owner moved")

    # Semantic continuation topology is bar-local and cannot begin a new bar.
    require("StepMask anchoredContinuations" in motif and "bool active = false;" in motif,
            "motif continuation anchoring moved")
    require("else if ((continuations & bit) != 0 && active)" in motif,
            "motif continuation no longer requires a local anchor")
    require("bool validContinuationTopology" in tonal and "if (!active) return false;" in tonal,
            "tonal continuation validation moved")
    require("bool validPlan" in adapter and "if (!active) return false;" in adapter,
            "adapter continuation validation moved")

    # Current continuation compatibility materializes another pitched step with
    # slide articulation; it is not an independent lifetime token.
    require("SynthStep tied = active;" in adapter and "tied.slide = true;" in adapter,
            "continuation adapter semantics moved")
    require("next.steps[step] = tied;" in adapter,
            "continuation no longer materializes as a SynthStep")
    require("startNote(noteToFreq(step.note), step.accent, step.slide" in engine,
            "slide no longer reaches synth startNote as articulation")
    require("publishPatternNoteOn_(synthIdx" in engine,
            "pitched pattern steps no longer publish PatternPlayer NoteOn")
    require("replaceActiveNote" in midi and
            "if (lane.activeNote >= 0 && !releaseActiveNote(lane)) return false;" in midi,
            "USB MIDI replacement ownership moved")

    song_apply = window(engine, "void MiniAcid::applySongPositionSelection()", 3200)
    panic_pos = song_apply.find("publishPatternAllNotesOff_()")
    select_pos = song_apply.find("setCurrentSynthPatternIndex")
    require(panic_pos >= 0 and select_pos >= 0 and panic_pos < select_pos,
            "Song transition must publish MIDI cleanup before physical selection")

    set_pattern = window(engine, "void MiniAcid::set303PatternIndex", 900)
    require("publishPatternNoteOff_(idx)" in set_pattern,
            "explicit synth-pattern switch lost PatternPlayer MIDI cleanup")
    require("synthVoices_[idx]->release()" not in set_pattern,
            "explicit pattern switch unexpectedly acquired internal voice cleanup")

    internal_handler = window(internal, "void InternalSynthOutput::handleMusicalEvent", 900)
    require("event.source == MusicalEventSource::PatternPlayer" in internal_handler and
            "return;" in internal_handler,
            "InternalSynthOutput no longer ignores PatternPlayer fan-out")
    require("event.type == MusicalEventType::AllNotesOff" in midi and
            "releaseTargetAllNotes(event.source, event.target);" in midi,
            "USB MIDI AllNotesOff ownership moved")

    stop = window(engine, "void MiniAcid::stop()", 4200)
    require("gateCountdownA_ = 0;" in stop and "gateCountdownB_ = 0;" in stop,
            "Stop no longer clears both gate countdowns")
    require("synthVoices_[0]->release();" in stop and "synthVoices_[1]->release();" in stop,
            "Stop no longer releases both internal voices")
    require("publishPatternAllNotesOff_" in stop,
            "Stop no longer publishes PatternPlayer AllNotesOff")

    cases = {
        "A": "ordinary onset/release is gate-owned inside one bar",
        "B": "final-step ordinary onset cannot express a hold into an empty bar",
        "C": "same-pitch next-bar onset is a retrigger, not a continuation",
        "D": "different-pitch next-bar onset is a replacement/retrigger",
        "E": "slide is onset articulation, not cross-pattern lifetime",
        "F": "Stop is a hard lifetime barrier for internal and MIDI paths",
        "G": "explicit pattern switch has different internal/MIDI cleanup paths",
        "H": "Song pattern advance has different internal/MIDI cleanup paths",
    }
    for case_id, summary in cases.items():
        print(f"SOURCE {case_id}: {summary}")
    print("M2-A1 source characterization: OK")


if __name__ == "__main__":
    main()
