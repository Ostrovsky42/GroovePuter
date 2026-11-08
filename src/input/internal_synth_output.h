#pragma once

#include <cstdint>

#include "musical_event_router.h"
#include "src/audio/audio_mutation_gate.h"

class MiniAcid;

// Physical output sink and fixed-size arbitration owner for internal Synth A/B.
// PatternPlayer keeps the physical Pattern path inside MiniAcid/AudioTask; this
// sink only projects the highest-priority currently-active live candidate when
// Pattern does not own the voice.
class InternalSynthOutput final : public IMusicalEventSink {
public:
    struct MonoArbitrationState {
        struct Candidate {
            bool active{false};
            MusicalEventSource source{MusicalEventSource::MidiInput};
            uint8_t note{0};
            uint8_t velocity{0};
        };

        bool patternOwned{false};
        Candidate generatedCandidate{};
        Candidate directCandidate{};
        Candidate otherLiveCandidate{};
        Candidate currentlyProjectedLiveCandidate{};

        void applyLiveEvent(const MusicalEvent& event) {
            if (event.type == MusicalEventType::AllNotesOff) {
                panic();
                return;
            }

            Candidate* candidate = candidateForSource(event.source);
            if (candidate == nullptr) return;

            if (event.type == MusicalEventType::NoteOn) {
                *candidate = Candidate{
                    true, event.source, event.note, event.velocity};
                return;
            }

            if (event.type == MusicalEventType::NoteOff &&
                candidate->active &&
                candidate->source == event.source &&
                candidate->note == event.note) {
                *candidate = Candidate{};
            }
        }

        void setPatternOwned(bool owned) { patternOwned = owned; }

        void panic() {
            generatedCandidate = Candidate{};
            directCandidate = Candidate{};
            otherLiveCandidate = Candidate{};
        }

        Candidate selectedCandidate() const {
            if (patternOwned) return Candidate{};
            if (generatedCandidate.active) return generatedCandidate;
            if (directCandidate.active) return directCandidate;
            if (otherLiveCandidate.active) return otherLiveCandidate;
            return Candidate{};
        }

    private:
        Candidate* candidateForSource(MusicalEventSource source) {
            switch (source) {
                case MusicalEventSource::Arpeggiator:
                    return &generatedCandidate;
                case MusicalEventSource::PerformanceKeyboard:
                    return &directCandidate;
                case MusicalEventSource::MidiInput:
                    return &otherLiveCandidate;
                case MusicalEventSource::PatternPlayer:
                case MusicalEventSource::PerformanceKeyboardPoly:
                    return nullptr;
            }
            return nullptr;
        }
    };

    InternalSynthOutput(MiniAcid& engine, AudioMutationGate& mutationGate)
        : engine_(engine), mutationGate_(mutationGate) {}

    void handleMusicalEvent(const MusicalEvent& event) override;
    void syncPatternOwnership();

private:
    static int synthIndex(MusicalEventTarget target);
    static bool sameCandidate(const MonoArbitrationState::Candidate& lhs,
                              const MonoArbitrationState::Candidate& rhs);
    void applyPatternOwnershipLocked(int voice, bool owned);
    void reconcileLiveProjectionLocked(int voice);

    MiniAcid& engine_;
    AudioMutationGate& mutationGate_;
    MonoArbitrationState monoState_[2]{};
    // Only tracks sampler voices started by this PERFORM sink. It is not a MIDI
    // note-owner table; one bit corresponds to one normalized drum lane 0..7.
    uint8_t liveDrumPadMask_{0};
};
