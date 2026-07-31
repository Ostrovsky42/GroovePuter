#pragma once
#ifndef GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
#define GROOVEPUTER_MUSICAL_EVENT_QUEUE_H

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "../midi/scheduled_musical_event_queue.h"

// Compatibility facade used by MiniAcid's existing PatternPlayer publication
// API. MiniAcid still emits normalized MusicalEvent values at the exact point
// where the per-sample renderer triggers them. AudioTask brackets each render
// block here, and this facade converts the current sequencer phase into the
// ScheduledMusicalEvent frame offset consumed by MidiDispatchTask.
//
// This keeps USB/TinyUSB concerns out of the DSP engine and avoids any heap,
// lock or Arduino-loop dependency in the realtime producer path.
class MusicalEventQueue final : public ScheduledMusicalEventQueue {
public:
    using PhaseReader = float (*)(void* context);
    using ScheduledMusicalEventQueue::tryPop;
    using ScheduledMusicalEventQueue::tryPush;

    void setPhaseReader(PhaseReader reader, void* context) {
        phaseReader_ = reader;
        phaseReaderContext_ = context;
    }

    void beginMidiRenderBlock(uint32_t blockSequence,
                              uint16_t blockFrames,
                              float startPhaseSteps,
                              float bpm,
                              float sampleRate,
                              bool transportPlaying) {
        renderBlockSequence_ = blockSequence;
        renderBlockFrames_ = blockFrames;
        renderBpm_ = bpm > 0.0f ? bpm : 120.0f;
        renderSampleRate_ = sampleRate > 0.0f ? sampleRate : 44100.0f;

        const float normalizedStart = normalizePhase(startPhaseSteps);
        // MiniAcid::start() deliberately places currentTick_ at tick 383 and
        // forces the first tick on sample zero. Treat that rising-edge state as
        // the exact end of the bar, otherwise phase arithmetic would place the
        // first step roughly one PPQN tick late.
        if (transportPlaying && !previousTransportPlaying_ &&
            normalizedStart > 15.0f) {
            renderStartPhaseSteps_ = 16.0f;
        } else {
            renderStartPhaseSteps_ = normalizedStart;
        }

        previousTransportPlaying_ = transportPlaying;
        renderBlockActive_ = true;
    }

    void endMidiRenderBlock() {
        renderBlockActive_ = false;
    }

    bool tryPush(const MusicalEvent& event) {
        if (!renderBlockActive_) {
            return suppressNonRealtimeEvent(event);
        }

        // The base queue turns AllNotesOff into an immediate generation barrier;
        // it does not need a meaningful frame position.
        if (event.type == MusicalEventType::AllNotesOff) {
            return ScheduledMusicalEventQueue::tryPush(
                event, renderBlockSequence_, 0);
        }

        if (phaseReader_ == nullptr || renderBlockFrames_ == 0) {
            return suppressNonRealtimeEvent(event);
        }

        const float currentPhase = normalizePhase(
            phaseReader_(phaseReaderContext_));
        float deltaSteps = currentPhase - renderStartPhaseSteps_;
        if (deltaSteps < 0.0f) deltaSteps += 16.0f;

        const float samplesPerStep =
            (renderSampleRate_ * 60.0f) / (renderBpm_ * 4.0f);
        long frame = static_cast<long>(
            std::lround(deltaSteps * samplesPerStep)) - 1L;
        if (frame < 0) frame = 0;
        if (frame >= static_cast<long>(renderBlockFrames_)) {
            frame = static_cast<long>(renderBlockFrames_) - 1L;
        }

        return ScheduledMusicalEventQueue::tryPush(
            event,
            renderBlockSequence_,
            static_cast<uint16_t>(frame));
    }

    uint32_t droppedCount() const {
        return droppedNoteOnCount() + droppedCriticalCount();
    }

private:
    static float normalizePhase(float phaseSteps) {
        if (!std::isfinite(phaseSteps)) return 0.0f;
        float normalized = std::fmod(phaseSteps, 16.0f);
        if (normalized < 0.0f) normalized += 16.0f;
        return normalized;
    }

    PhaseReader phaseReader_{nullptr};
    void* phaseReaderContext_{nullptr};
    uint32_t renderBlockSequence_{0};
    uint16_t renderBlockFrames_{0};
    float renderStartPhaseSteps_{0.0f};
    float renderBpm_{120.0f};
    float renderSampleRate_{44100.0f};
    bool renderBlockActive_{false};
    bool previousTransportPlaying_{false};
};

#endif  // GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
