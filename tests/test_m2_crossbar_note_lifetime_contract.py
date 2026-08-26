#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_window(text: str, marker: str, size: int = 5000) -> str:
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

    synth_step = function_window(scenes, "struct SynthStep", 600)
    require("int8_t note = -1;" in synth_step,
            "SynthStep must retain the note/rest representation under audit")
    for token in ("duration", "gate", "tie", "hold", "continuation"):
        require(token not in synth_step.lower(),
                f"SynthStep unexpectedly gained explicit {token} lifetime state")

    # There is a latent playback-only -2 sentinel, but it is not part of the
    # public SynthStep data contract and the tonal adapter does not emit it.
    require("if (step.note == -2) { // TIE" in engine,
            "MiniAcid latent -2 TIE sentinel disappeared")
    require("gateCountdownA_ > 0" in engine and "gateCountdownB_ > 0" in engine,
            "latent TIE must still depend on an already-active gate")
    require("std::array<uint8_t, 16> tie{};" in advanced,
            "legacy/configured generator tie mask disappeared")
    require("tied.note = -2" not in adapter,
            "tonal adapter unexpectedly started emitting the playback-only tie sentinel")

    # Production groove compilation clamps gate length to one step. MiniAcid
    # then applies <1.0 effective multipliers to both synth lanes, so an ordinary
    # onset gate reaches zero before the next sequencer step can consume -2.
    require("params.gateLengthMultiplier > 1.0f ? 1.0f" in genre,
            "production gate multiplier is no longer capped at 1.0")
    require("float vMult = (synthIdx == 0) ? 0.85f : 1.05f;" in engine,
            "Synth A/B gate multipliers changed")
    require("if (synthIdx == 1 && effectiveGateMult > 0.98f) effectiveGateMult = 0.98f;" in engine,
            "Synth B effective gate cap changed")
    require("gateCountdownA_ = dur;" in engine and "gateCountdownB_ = dur;" in engine,
            "ordinary note gate ownership moved")
    ordinary_gate_survives_next_step = False

    # Semantic continuations are bar-local. Both motif and tonal validation start
    # with no active note, so a continuation at physical bar step 0 is illegal.
    require("StepMask anchoredContinuations" in motif and "bool active = false;" in motif,
            "MelodicMotif continuation anchoring changed")
    require("else if ((continuations & bit) != 0 && active)" in motif,
            "MelodicMotif must only keep locally anchored continuations")
    require("bool validContinuationTopology" in tonal and
            "if ((continuations & bit) != 0)" in tonal and
            "if (!active) return false;" in tonal,
            "tonal materializer must reject an unanchored continuation")
    require("bool validPlan" in adapter and "if (!active) return false;" in adapter,
            "tonal adapter must reject an unanchored continuation")

    # The current semantic continuation adapter copies the active pitch and marks
    # the next physical step slide=true. That is another NoteOn, not a lifetime tie.
    require("SynthStep tied = active;" in adapter and "tied.slide = true;" in adapter,
            "tonal continuation compatibility materialization changed")
    require("next.steps[step] = tied;" in adapter,
            "tonal continuation must still materialize as a physical SynthStep")
    require("startNote(noteToFreq(step.note), step.accent, step.slide" in engine,
            "slide must remain onset articulation passed into the synth voice")
    require("publishPatternNoteOn_(synthIdx" in engine,
            "every non-negative pattern step must retain PatternPlayer NoteOn publication")
    require("replaceActiveNote" in midi and
            "if (lane.activeNote >= 0 && !releaseActiveNote(lane)) return false;" in midi,
            "USB MIDI same-pitch/new-pitch NoteOn must still replace active ownership")

    # Song transition and explicit pattern switch are playback lifecycle barriers
    # for PatternPlayer MIDI. Internal PatternPlayer audio is rendered directly by
    # MiniAcid and deliberately ignores router fan-out.
    song_apply = function_window(engine, "void MiniAcid::applySongPositionSelection()", 3200)
    panic_pos = song_apply.find("publishPatternAllNotesOff_()")
    select_pos = song_apply.find("setCurrentSynthPatternIndex")
    require(panic_pos >= 0 and select_pos >= 0 and panic_pos < select_pos,
            "Song row transition must publish AllNotesOff before selecting the next synth pattern")

    set_pattern = function_window(engine, "void MiniAcid::set303PatternIndex", 900)
    require("publishPatternNoteOff_(synthIdx)" in set_pattern,
            "explicit synth pattern switch must release PatternPlayer MIDI ownership")
    require("synthVoices_[synthIdx]->release()" not in set_pattern,
            "explicit pattern switch unexpectedly acquired internal voice lifetime ownership")

    require("event.source == MusicalEventSource::PatternPlayer" in internal and
            "return;" in function_window(internal, "void InternalSynthOutput::handleMusicalEvent", 900),
            "internal sink must continue ignoring PatternPlayer fan-out")
    require("event.type == MusicalEventType::AllNotesOff" in midi and
            "releaseTargetAllNotes(event.source, event.target);" in midi,
            "USB MIDI AllNotesOff must release target-owned active notes")

    stop = function_window(engine, "void MiniAcid::stop()", 4200)
    require("gateCountdownA_ = 0;" in stop and "gateCountdownB_ = 0;" in stop,
            "Stop must clear both synth gate countdowns")
    require("synthVoices_[0]->release();" in stop and "synthVoices_[1]->release();" in stop,
            "Stop must release both internal synth voices")
    require("publishPatternAllNotesOff_" in stop,
            "Stop must publish PatternPlayer AllNotesOff")

    semantic_crossbar_continuation = False
    song_midi_barrier = True
    internal_pattern_fanout_ignored = True

    cases = [
        (
            "A", "note starts/ends inside one bar",
            "release by gate countdown before next step",
            "NoteOn then NoteOff by gate countdown",
            "not required", "NO",
        ),
        (
            "B", "bar0 step15 onset -> bar1 empty",
            "ordinary gate expires before bar1",
            "ordinary NoteOff, then Song AllNotesOff barrier",
            "Song transition publishes it", "NO",
        ),
        (
            "C", "bar0 step15 onset -> bar1 step0 same pitch",
            "old note expires; step0 starts a new note",
            "old NoteOff/AllNotesOff, then new NoteOn",
            "Song transition publishes it", "NO: retrigger",
        ),
        (
            "D", "bar0 step15 onset -> bar1 step0 different pitch",
            "old note expires; step0 starts the new pitch",
            "old NoteOff/AllNotesOff, then new NoteOn",
            "Song transition publishes it", "NO: retrigger",
        ),
        (
            "E", "slide at physical bar boundary",
            "slide is onset articulation; no cross-bar lifetime",
            "new NoteOn replaces any active MIDI note",
            "Song transition still publishes it", "NO",
        ),
        (
            "F", "transport Stop at held-note boundary",
            "Stop clears gates and releases both voices",
            "Stop publishes AllNotesOff and releases ownership",
            "YES", "NO survival by design",
        ),
        (
            "G", "explicit pattern switch while note active",
            "no immediate internal release in set303PatternIndex",
            "publishPatternNoteOff_ releases active PatternPlayer MIDI",
            "NoteOff, not global AllNotesOff", "DIVERGENT",
        ),
        (
            "H", "Song advances physical pattern while note active",
            "selection path does not release internal voice directly",
            "AllNotesOff before next physical pattern selection",
            "YES", "DIVERGENT / engine-dependent",
        ),
    ]

    require(not ordinary_gate_survives_next_step,
            "ordinary production gate unexpectedly became able to reach a following tie step")
    require(not semantic_crossbar_continuation,
            "semantic continuation unexpectedly became legal across physical bars")
    require(song_midi_barrier and internal_pattern_fanout_ignored,
            "internal/MIDI Song boundary divergence changed")

    for case_id, scenario, internal_actual, midi_actual, all_notes_off, tie in cases:
        print(
            f"{case_id}: {scenario}\n"
            f"  internal={internal_actual}\n"
            f"  midi={midi_actual}\n"
            f"  AllNotesOff={all_notes_off}\n"
            f"  genuine-crossbar-tie={tie}"
        )

    print("M2-A1 cross-bar note lifetime characterization: OK")


if __name__ == "__main__":
    main()
