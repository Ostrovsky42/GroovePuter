#pragma once
#ifndef GROOVEPUTER_MIDI_TRANSPORT_CLOCK_PUBLISHER_H
#define GROOVEPUTER_MIDI_TRANSPORT_CLOCK_PUBLISHER_H

#include <cmath>
#include <cstdint>

#include "scheduled_midi_transport_event_queue.h"

// Converts GroovePuter's current sequencer phase into MIDI 24 PPQN transport
// events on the same AudioTask block timeline used by Pattern MIDI. The phase is
// re-anchored every block; no wall-clock polling loop or accumulated floating
// clock is used.
class MidiTransportClockPublisher {
public:
    static constexpr int kMidiClocksPerQuarter = 24;
    static constexpr int kSequencerStepsPerQuarter = 4;
    static constexpr int kMidiClocksPerStep =
        kMidiClocksPerQuarter / kSequencerStepsPerQuarter;

    void beginBlock(ScheduledMidiTransportEventQueue& queue,
                    uint32_t blockSequence,
                    uint16_t blockFrames,
                    float startPhaseSteps,
                    float bpm,
                    float sampleRate,
                    bool transportPlaying,
                    bool restartFromBeginning = true) {
        if (transportPlaying && !previousTransportPlaying_) {
            queue.tryPushLifecycle(
                restartFromBeginning
                    ? MidiTransportEventType::Start
                    : MidiTransportEventType::Continue,
                blockSequence,
                0);
        } else if (!transportPlaying && previousTransportPlaying_) {
            queue.tryPushLifecycle(MidiTransportEventType::Stop,
                                   blockSequence,
                                   0);
        }
        previousTransportPlaying_ = transportPlaying;

        if (!transportPlaying || blockFrames == 0 ||
            !std::isfinite(bpm) || bpm <= 0.0f ||
            !std::isfinite(sampleRate) || sampleRate <= 0.0f) {
            return;
        }

        const double phase = normalizePhase(startPhaseSteps);
        const double samplesPerStep =
            (static_cast<double>(sampleRate) * 60.0) /
            (static_cast<double>(bpm) *
             static_cast<double>(kSequencerStepsPerQuarter));
        const double startPulsePosition =
            phase * static_cast<double>(kMidiClocksPerStep);

        // A phase that is already on a clock boundary owns frame 0 of this
        // block. The previous block excludes frame == blockFrames, so the same
        // pulse cannot be emitted twice at a block boundary.
        constexpr double kBoundaryEpsilon = 1.0e-7;
        int64_t pulseIndex = static_cast<int64_t>(
            std::ceil(startPulsePosition - kBoundaryEpsilon));

        for (;; ++pulseIndex) {
            const double deltaSteps =
                (static_cast<double>(pulseIndex) - startPulsePosition) /
                static_cast<double>(kMidiClocksPerStep);
            if (deltaSteps < -kBoundaryEpsilon) continue;

            const double frameExact = deltaSteps * samplesPerStep;
            if (frameExact >= static_cast<double>(blockFrames)) break;

            long frame = static_cast<long>(std::lround(frameExact));
            if (frame < 0) frame = 0;
            if (frame >= static_cast<long>(blockFrames)) break;

            queue.tryPushClock(blockSequence,
                               static_cast<uint16_t>(frame));
        }
    }

    void reset() {
        previousTransportPlaying_ = false;
    }

    bool previousTransportPlaying() const {
        return previousTransportPlaying_;
    }

private:
    static double normalizePhase(float phaseSteps) {
        if (!std::isfinite(phaseSteps)) return 0.0;
        double normalized = std::fmod(static_cast<double>(phaseSteps), 16.0);
        if (normalized < 0.0) normalized += 16.0;
        return normalized;
    }

    bool previousTransportPlaying_{false};
};

#endif  // GROOVEPUTER_MIDI_TRANSPORT_CLOCK_PUBLISHER_H
