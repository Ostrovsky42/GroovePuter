#include "internal_synth_output.h"

#include "src/dsp/miniacid_engine.h"

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

    // The logical channel is intentionally ignored by the internal engine.
    AudioMutationScope mutationScope(mutationGate_);
    const int voice = synthIndex(event.target);
    switch (event.type) {
        case MusicalEventType::NoteOn:
            engine_.liveNoteOn(voice, event.note, event.velocity);
            break;
        case MusicalEventType::NoteOff:
            engine_.liveNoteOff(voice, event.note);
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
