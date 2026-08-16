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

bool isMidiInputSource(MusicalEventSource source) {
    return source == MusicalEventSource::MidiInput;
}

float samplerVelocity(uint8_t velocity) {
    if (velocity < 1) velocity = 1;
    if (velocity > 127) velocity = 127;
    return static_cast<float>(velocity) / 127.0f;
}
}  // namespace

int InternalSynthOutput::synthIndex(MusicalEventTarget target) {
    return target == MusicalEventTarget::SynthB ? 1 : 0;
}

void InternalSynthOutput::handleMusicalEvent(const MusicalEvent& event) {
    // PatternPlayer already owns and renders internal voices inside AudioTask.
    // Its router fan-out is additive output only; taking AudioMutationGate here
    // would deadlock the realtime producer and double-trigger.
    if (event.source == MusicalEventSource::PatternPlayer ||
        event.target == MusicalEventTarget::Dx) {
        return;
    }

    if (event.target == MusicalEventTarget::Drums) {
        // Performance sources still honor 0.9.6 INTERNAL/LAYER output ownership.
        // MidiInput is a separate incoming-controller domain: R4 resolves its
        // GM-style note map to a logical lane before this sink and must not be
        // gated by the outbound DeviceProfile/OutputOwnership selection.
        const bool performanceSource = isPerformanceSource(event.source);
        const bool midiInputSource = isMidiInputSource(event.source);
        if (!performanceSource && !midiInputSource) return;
        if (event.channel >= 8) return;
        if (performanceSource &&
            event.type == MusicalEventType::NoteOn &&
            !GroovePuterOutput::allowsInternalNoteOn(event)) {
            return;
        }

        AudioMutationScope mutationScope(mutationGate_);
        const uint8_t lane = event.channel;
        const uint8_t mask = static_cast<uint8_t>(1u << lane);

        switch (event.type) {
            case MusicalEventType::NoteOn: {
                triggerRegisteredLocalDrumVoice(lane, event.velocity);
                if (engine_.sampleStore && engine_.samplerTrack &&
                    engine_.samplerTrack->isEnabled() &&
                    engine_.samplerTrack->pad(lane).id.value != 0) {
                    engine_.samplerTrack->triggerPad(
                        lane,
                        samplerVelocity(event.velocity),
                        *engine_.sampleStore);
                    liveDrumPadMask_ |= mask;
                }
                break;
            }
            case MusicalEventType::NoteOff:
                if ((liveDrumPadMask_ & mask) != 0u) {
                    // Drum one-shots keep their natural tail. A looping sample
                    // follows key ownership and stops on the matching key-up.
                    if (engine_.samplerTrack &&
                        engine_.samplerTrack->pad(lane).loop) {
                        engine_.samplerTrack->stopPad(lane);
                    }
                    liveDrumPadMask_ &= static_cast<uint8_t>(~mask);
                }
                break;
            case MusicalEventType::AllNotesOff:
                if (engine_.samplerTrack) {
                    for (uint8_t pad = 0; pad < 8; ++pad) {
                        const uint8_t padMask =
                            static_cast<uint8_t>(1u << pad);
                        if ((liveDrumPadMask_ & padMask) != 0u) {
                            engine_.samplerTrack->stopPad(pad);
                        }
                    }
                }
                liveDrumPadMask_ = 0;
                break;
        }
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
    // avoid mismatched ownership. MiniAcid remains the PLAY-state voice owner.
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
