#include "internal_synth_output.h"

#include "src/dsp/miniacid_engine.h"
#include "src/output/output_ownership.h"

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

bool isPerformanceSource(MusicalEventSource source) {
    return source == MusicalEventSource::PerformanceKeyboard ||
           source == MusicalEventSource::PerformanceKeyboardPoly ||
           source == MusicalEventSource::Arpeggiator;
}
}  // namespace

int InternalSynthOutput::synthIndex(MusicalEventTarget target) {
    return target == MusicalEventTarget::SynthB ? 1 : 0;
}

void InternalSynthOutput::handleMusicalEvent(const MusicalEvent& event) {
    // PatternPlayer already owns and renders the internal voices inside the
    // audio task. Its router fan-out is for additive outputs; taking the control
    // mutation gate here would deadlock the audio producer and double-trigger.
    if (event.source == MusicalEventSource::PatternPlayer ||
        event.target == MusicalEventTarget::Drums ||
        event.target == MusicalEventTarget::Dx) {
        return;
    }

    // <=0.9.5 PERFORM remains MIDI-only while the track has no explicit output
    // mode. Once a project/user selects INTERNAL or LAYER, direct keyboard and
    // generated performance tools may drive the existing monophonic local
    // engine. MIDI mode rejects only new local NoteOn; NoteOff/AllNotesOff stay
    // cleanup-critical so a Layer -> MIDI transition cannot strand a local note.
    if (isPerformanceSource(event.source) &&
        event.type == MusicalEventType::NoteOn &&
        !GroovePuterOutput::allowsInternalNoteOn(event)) {
        return;
    }

    // Keep the existing internal live-input path for any non-PERFORM source
    // that explicitly targets Synth A/B. Clamp NoteOn and NoteOff identically to
    // avoid mismatched ownership.
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
