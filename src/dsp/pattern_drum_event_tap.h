#pragma once
#ifndef GROOVEPUTER_PATTERN_DRUM_EVENT_TAP_H
#define GROOVEPUTER_PATTERN_DRUM_EVENT_TAP_H

#include <cstdint>
#include <memory>
#include <utility>

#include "mini_drumvoices.h"
#include "../input/musical_event_queue.h"

// The synth PatternPlayer already publishes normalized events from MiniAcid.
// Drum engines have several implementations and every base/retrig/flam/roll hit
// ultimately passes through the same DrumSynthVoice trigger API. This small
// decorator taps that point without adding a second sequencer or touching USB.
// Publication still goes through MusicalEventQueue, which timestamps the hit
// from the current AudioTask render phase.
inline MusicalEventQueue*& patternDrumEventQueueSlot() {
    static MusicalEventQueue* queue = nullptr;
    return queue;
}

class PatternEventQueueHandle {
public:
    PatternEventQueueHandle() = default;

    PatternEventQueueHandle& operator=(MusicalEventQueue* queue) {
        queue_ = queue;
        patternDrumEventQueueSlot() = queue;
        return *this;
    }

    explicit operator bool() const { return queue_ != nullptr; }

    // MiniAcid already calls patternEventQueue_->tryPush(...). Returning the
    // handle itself lets us preserve that source API while adding exactly one
    // Drums lifecycle barrier whenever publishPatternAllNotesOff_() finishes its
    // existing Synth A/B cleanup sequence. No MiniAcid sequencer fork is added.
    PatternEventQueueHandle* operator->() { return this; }
    const PatternEventQueueHandle* operator->() const { return this; }

    bool tryPush(const MusicalEvent& event) {
        if (!queue_) return false;
        const bool primary = queue_->tryPush(event);

        // publishPatternAllNotesOff_() always emits Synth A followed by Synth B.
        // Piggy-back on the second barrier so Song/scene/Stop lifecycle changes
        // invalidate Pattern Drums in the same generation-safe handoff.
        if (event.type == MusicalEventType::AllNotesOff &&
            event.source == MusicalEventSource::PatternPlayer &&
            event.target == MusicalEventTarget::SynthB) {
            const bool drums = queue_->tryPush(MusicalEvent{
                MusicalEventType::AllNotesOff,
                MusicalEventSource::PatternPlayer,
                MusicalEventTarget::Drums,
                0,
                0,
                0,
            });
            return primary && drums;
        }
        return primary;
    }

    operator MusicalEventQueue*() const { return queue_; }

private:
    MusicalEventQueue* queue_{nullptr};
};

inline void publishPatternDrumTrigger(uint8_t logicalVoice, uint8_t velocity) {
    MusicalEventQueue* queue = patternDrumEventQueueSlot();
    if (!queue || logicalVoice >= 8) return;
    if (velocity < 1) velocity = 1;
    if (velocity > 127) velocity = 127;
    queue->tryPush(MusicalEvent{
        MusicalEventType::NoteOn,
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::Drums,
        logicalVoice,
        60,
        velocity,
    });
}

class PatternPublishingDrumVoice {
public:
    PatternPublishingDrumVoice() = default;

    template <typename T>
    PatternPublishingDrumVoice(std::unique_ptr<T> voice)
        : voice_(std::move(voice)) {}

    template <typename T>
    PatternPublishingDrumVoice& operator=(std::unique_ptr<T> voice) {
        voice_ = std::move(voice);
        return *this;
    }

    explicit operator bool() const { return static_cast<bool>(voice_); }
    PatternPublishingDrumVoice* operator->() { return this; }
    const PatternPublishingDrumVoice* operator->() const { return this; }

    void reset() { if (voice_) voice_->reset(); }
    void setSampleRate(float sampleRate) { if (voice_) voice_->setSampleRate(sampleRate); }

    void triggerKick(bool accent = false, uint8_t velocity = 100) {
        if (!voice_) return;
        voice_->triggerKick(accent, velocity);
        publishPatternDrumTrigger(0, velocity);
    }
    void triggerSnare(bool accent = false, uint8_t velocity = 100) {
        if (!voice_) return;
        voice_->triggerSnare(accent, velocity);
        publishPatternDrumTrigger(1, velocity);
    }
    void triggerHat(bool accent = false, uint8_t velocity = 100) {
        if (!voice_) return;
        voice_->triggerHat(accent, velocity);
        publishPatternDrumTrigger(2, velocity);
    }
    void triggerOpenHat(bool accent = false, uint8_t velocity = 100) {
        if (!voice_) return;
        voice_->triggerOpenHat(accent, velocity);
        publishPatternDrumTrigger(3, velocity);
    }
    void triggerMidTom(bool accent = false, uint8_t velocity = 100) {
        if (!voice_) return;
        voice_->triggerMidTom(accent, velocity);
        publishPatternDrumTrigger(4, velocity);
    }
    void triggerHighTom(bool accent = false, uint8_t velocity = 100) {
        if (!voice_) return;
        voice_->triggerHighTom(accent, velocity);
        publishPatternDrumTrigger(5, velocity);
    }
    void triggerRim(bool accent = false, uint8_t velocity = 100) {
        if (!voice_) return;
        voice_->triggerRim(accent, velocity);
        publishPatternDrumTrigger(6, velocity);
    }
    void triggerClap(bool accent = false, uint8_t velocity = 100) {
        if (!voice_) return;
        voice_->triggerClap(accent, velocity);
        publishPatternDrumTrigger(7, velocity);
    }
    void triggerCymbal(bool accent = false, uint8_t velocity = 100) {
        if (voice_) voice_->triggerCymbal(accent, velocity);
    }

    float processKick() { return voice_ ? voice_->processKick() : 0.0f; }
    float processSnare() { return voice_ ? voice_->processSnare() : 0.0f; }
    float processHat() { return voice_ ? voice_->processHat() : 0.0f; }
    float processOpenHat() { return voice_ ? voice_->processOpenHat() : 0.0f; }
    float processMidTom() { return voice_ ? voice_->processMidTom() : 0.0f; }
    float processHighTom() { return voice_ ? voice_->processHighTom() : 0.0f; }
    float processRim() { return voice_ ? voice_->processRim() : 0.0f; }
    float processClap() { return voice_ ? voice_->processClap() : 0.0f; }
    float processCymbal() { return voice_ ? voice_->processCymbal() : 0.0f; }

    const Parameter& parameter(DrumParamId id) const { return voice_->parameter(id); }
    void setParameter(DrumParamId id, float value) {
        if (voice_) voice_->setParameter(id, value);
    }
    void setLoFiMode(bool enabled) { if (voice_) voice_->setLoFiMode(enabled); }
    void setLoFiAmount(float amount) { if (voice_) voice_->setLoFiAmount(amount); }

private:
    std::unique_ptr<DrumSynthVoice> voice_;
};

#endif  // GROOVEPUTER_PATTERN_DRUM_EVENT_TAP_H
