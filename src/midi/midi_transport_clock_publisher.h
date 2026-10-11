#pragma once
#ifndef GROOVEPUTER_MIDI_TRANSPORT_CLOCK_PUBLISHER_H
#define GROOVEPUTER_MIDI_TRANSPORT_CLOCK_PUBLISHER_H

#include <cmath>
#include <cstdint>
#include <limits>

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
            deferredClock_ = false;
            return;
        }

        const bool emittedDeferredClock = deferredClock_;
        deferredClock_ = false;
        if (emittedDeferredClock) {
            queue.tryPushClock(blockSequence, 0);
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
        // startPhaseSteps is a float across a 16-step bar. Allow its rounding
        // error in pulse units when recognizing an exact clock boundary.
        constexpr double kBoundaryEpsilon =
            std::numeric_limits<float>::epsilon() * 16.0 * kMidiClocksPerStep;
        int64_t pulseIndex = static_cast<int64_t>(
            std::ceil(startPulsePosition - kBoundaryEpsilon));

        for (;; ++pulseIndex) {
            const double deltaSteps =
                (static_cast<double>(pulseIndex) - startPulsePosition) /
                static_cast<double>(kMidiClocksPerStep);
            if (deltaSteps < -kBoundaryEpsilon) continue;

            const double frameExact = deltaSteps * samplesPerStep;
            if (frameExact >= static_cast<double>(blockFrames)) break;

            // A pulse rounded onto this block's frame zero was already emitted
            // above. A float phase anchor may still place that pulse near zero.
            if (emittedDeferredClock && frameExact < 1.0) continue;

            long frame = static_cast<long>(std::lround(frameExact));
            if (frame < 0) frame = 0;
            // Preserve nearest-sample timing when an in-block pulse rounds
            // onto the next block. Re-anchoring phase alone would lose it.
            if (frame >= static_cast<long>(blockFrames)) {
                deferredClock_ = true;
                break;
            }

            queue.tryPushClock(blockSequence,
                               static_cast<uint16_t>(frame));
        }
    }

    void reset() {
        previousTransportPlaying_ = false;
        deferredClock_ = false;
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
    bool deferredClock_{false};
};

#endif  // GROOVEPUTER_MIDI_TRANSPORT_CLOCK_PUBLISHER_H
