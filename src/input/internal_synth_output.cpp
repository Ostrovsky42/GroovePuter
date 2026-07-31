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
    // Drums is an external USB-MIDI target; GroovePuter has no live chromatic
    // internal drum-note contract, so it must not alias to Synth A.
    if (event.source == MusicalEventSource::PatternPlayer ||
        event.target == MusicalEventTarget::Drums) {
        return;
    }

    // USB MIDI may use the wider performance-keyboard range. The current
    // internal synth engines retain their established 24..71 safety range, so
    // clamp NoteOn and NoteOff identically to avoid mismatched/stuck ownership.
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
            // This event belongs to one logical target. Release only a note
            // currently owned by live-style input on that voice; never interrupt
            // a PatternPlayer-owned Synth A/B voice while transport is running.
            const int liveNote = engine_.liveNote(voice);
            if (liveNote >= 0) {
                engine_.liveNoteOff(voice, static_cast<uint8_t>(liveNote));
            }
            break;
        }
    }
}
