#include "internal_synth_output.h"

#include "src/dsp/miniacid_engine.h"

namespace {
uint8_t clampInternalLiveNote(uint8_t note) {
    if (note < MiniAcid::kMin303Note) {
        return static_cast<uint8_t>(MiniAcid::kMin303Note);
    }
    if (note > MiniAcid::kMax303Note) {
        return static_cast<uint8_t>(MiniAcid::kMax303Note);
    }
    return note;
}
}  // namespace

int InternalSynthOutput::synthIndex(MusicalEventTarget target) {
    return target == MusicalEventTarget::SynthB ? 1 : 0;
}

void InternalSynthOutput::handleMusicalEvent(const MusicalEvent& event) {
    // PatternPlayer already owns and renders the internal voices inside the
    // audio task. Its router fan-out is for additive outputs; taking the control
    // mutation gate here would deadlock the audio producer and double-trigger.
    //
    // PERFORM keyboard ownership is external-MIDI-only for Synth A/B/DX.
    // Direct MONO, direct POLY and generated performance tools must never play
    // the internal Synth A/B voices. Those voices remain sequencer/pattern
    // instruments instead of doubling every Cardputer keyboard press locally.
    if (event.source == MusicalEventSource::PatternPlayer ||
        event.source == MusicalEventSource::PerformanceKeyboard ||
        event.source == MusicalEventSource::PerformanceKeyboardPoly ||
        event.source == MusicalEventSource::Arpeggiator ||
        event.target == MusicalEventTarget::Drums ||
        event.target == MusicalEventTarget::Dx) {
        return;
    }

    // Keep the existing internal live-input path for any non-PERFORM source
    // that explicitly targets Synth A/B (for example future/local MIDI input).
    // Clamp NoteOn and NoteOff identically to avoid mismatched ownership.
    AudioMutationScope mutationScope(mutationGate_);
    const int voice = synthIndex(event.target);
    const uint8_t internalNote = clampInternalLiveNote(event.note);
    switch (event.type) {
        case MusicalEventType::NoteOn:
            engine_.liveNoteOn(voice, internalNote, event.velocity);
            break;
        case MusicalEventType::NoteOff:
            engine_.liveNoteOff(voice, internalNote);
            break;
        case MusicalEventType::AllNotesOff: {
            const int liveNote = engine_.liveNote(voice);
            if (liveNote >= 0) {
                engine_.liveNoteOff(voice, static_cast<uint8_t>(liveNote));
            }
            break;
        }
    }
}
