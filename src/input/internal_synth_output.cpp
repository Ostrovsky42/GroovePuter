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

bool isInternalMonoPerformanceSource(MusicalEventSource source) {
    return source == MusicalEventSource::PerformanceKeyboard ||
           source == MusicalEventSource::Arpeggiator;
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

bool InternalSynthOutput::sameCandidate(
    const MonoArbitrationState::Candidate& lhs,
    const MonoArbitrationState::Candidate& rhs) {
    if (lhs.active != rhs.active) return false;
    if (!lhs.active) return true;
    return lhs.source == rhs.source && lhs.note == rhs.note;
}

void InternalSynthOutput::reconcileLiveProjectionLocked(int voice) {
    MonoArbitrationState& state = monoState_[voice];
    const MonoArbitrationState::Candidate next = state.selectedCandidate();
    const MonoArbitrationState::Candidate current =
        state.currentlyProjectedLiveCandidate;

    if (sameCandidate(current, next)) {
        state.currentlyProjectedLiveCandidate = next;
        return;
    }

    if (current.active) {
        engine_.liveNoteOff(voice, clampInternalLiveNote(current.note));
    }
    if (next.active) {
        engine_.liveNoteOn(
            voice, clampInternalLiveNote(next.note), next.velocity);
    }
    state.currentlyProjectedLiveCandidate = next;
}

void InternalSynthOutput::applyPatternOwnershipLocked(int voice, bool owned) {
    MonoArbitrationState& state = monoState_[voice];
    if (state.patternOwned == owned) return;

    state.setPatternOwned(owned);
    if (owned) {
        // Pattern has already started the physical voice in AudioTask. Clear
        // only live projection identity: releasing here would release Pattern.
        if (state.currentlyProjectedLiveCandidate.active) {
            engine_.suspendLiveNoteProjection(voice);
            state.currentlyProjectedLiveCandidate =
                MonoArbitrationState::Candidate{};
        }
        return;
    }

    // Pattern already released the physical voice. Re-project only a candidate
    // that is still logically active now; no displaced-note history exists.
    reconcileLiveProjectionLocked(voice);
}

void InternalSynthOutput::syncPatternOwnership() {
    const bool synthAOwned = engine_.patternOwnsInternalSynth(0);
    const bool synthBOwned = engine_.patternOwnsInternalSynth(1);
    if (monoState_[0].patternOwned == synthAOwned &&
        monoState_[1].patternOwned == synthBOwned) {
        return;
    }

    AudioMutationScope mutationScope(mutationGate_);
    applyPatternOwnershipLocked(0, engine_.patternOwnsInternalSynth(0));
    applyPatternOwnershipLocked(1, engine_.patternOwnsInternalSynth(1));
}

void InternalSynthOutput::handleMusicalEvent(const MusicalEvent& event) {
    // PatternPlayer owns and renders internal voices inside AudioTask. Its
    // normalized events are additive external-output data only; Pattern
    // ownership reaches this arbiter through syncPatternOwnership().
    if (event.source == MusicalEventSource::PatternPlayer ||
        event.target == MusicalEventTarget::Dx) {
        return;
    }

    if (event.target == MusicalEventTarget::Drums) {
        // Preserve the existing local drum/sampler path unchanged. This closure
        // applies mono arbitration only to Synth A/B.
        if (!isPerformanceSource(event.source)) return;
        if (event.channel >= 8) return;
        if (event.type == MusicalEventType::NoteOn &&
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

    // Manual POLY remains an external-MIDI concern. The internal Synth A/B
    // engines receive one deterministic mono projection only.
    if (event.source == MusicalEventSource::PerformanceKeyboardPoly) return;

    // Legacy output mode suppresses only new local PERFORM NoteOn. Cleanup and
    // OTHER LIVE retain the existing behavior.
    if (isInternalMonoPerformanceSource(event.source) &&
        event.type == MusicalEventType::NoteOn &&
        !GroovePuterOutput::allowsInternalNoteOn(event)) {
        return;
    }

    AudioMutationScope mutationScope(mutationGate_);
    const int voice = synthIndex(event.target);
    applyPatternOwnershipLocked(
        voice, engine_.patternOwnsInternalSynth(voice));

    MonoArbitrationState& state = monoState_[voice];
    state.applyLiveEvent(event);
    reconcileLiveProjectionLocked(voice);
}
