#pragma once
#ifndef GROOVEPUTER_SCHEDULED_MUSICAL_EVENT_QUEUE_H
#define GROOVEPUTER_SCHEDULED_MUSICAL_EVENT_QUEUE_H

#include <cstddef>
#include <cstdint>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include "midi_realtime_word.h"
#include "scheduled_musical_event.h"
#include "../output/output_ownership.h"

// Single-producer/single-consumer handoff from AudioTask PatternPlayer events to
// MidiDispatchTask. Publication never allocates or blocks. Lifecycle changes
// invalidate target generations and request a target-scoped wire cleanup.
class ScheduledMusicalEventQueue {
public:
    static constexpr std::size_t kStorageSize = 128;
    static constexpr std::size_t kCapacity = kStorageSize - 1;
    static constexpr uint8_t kSynthAMask = 1u << 0;
    static constexpr uint8_t kSynthBMask = 1u << 1;
    static constexpr uint8_t kDrumsMask = 1u << 2;

    bool tryPush(const MusicalEvent& event,
                 uint32_t blockSequence,
                 uint16_t frameOffset) {
        if (!isRealtimeProducerContext()) {
            return suppressNonRealtimeEvent(event);
        }

        // AllNotesOff is a lifecycle barrier rather than an ordinary scheduled
        // note. Advance the target generation immediately so already queued
        // events cannot be replayed after Stop, scene/Song changes or recovery.
        // MidiDispatchTask consumes the pending target-scoped panic before it
        // dispatches any event from the following generation.
        if (event.type == MusicalEventType::AllNotesOff) {
            invalidateTarget(event.target);
            return true;
        }

        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        if (next == tail_.loadAcquire()) {
            if (event.type == MusicalEventType::NoteOn) {
                droppedNoteOn_.incrementRelaxed();
            } else {
                droppedCritical_.incrementRelaxed();
                invalidateTarget(event.target);
            }
            return false;
        }

        ScheduledMusicalEvent scheduled{};
        scheduled.event = event;
        scheduled.blockSequence = blockSequence;
        scheduled.frameOffset = frameOffset;
        scheduled.generation = generationFor(event.target);
        scheduled.publicationSequence = publicationSequence_.incrementRelaxed();
        events_[head] = scheduled;
        head_.storeRelease(next);
        return true;
    }

    bool tryPop(ScheduledMusicalEvent& scheduled) {
        while (true) {
            const uint32_t tail = tail_.loadRelaxed();
            if (tail == head_.loadAcquire()) return false;

            scheduled = events_[tail];
            tail_.storeRelease((tail + 1u) % kStorageSize);

            // A scheduled NoteOn may predate a live output-mode change. Drop it
            // at the consumer boundary if the external side is no longer owned.
            // NoteOff remains valid cleanup and must never be filtered here.
            if (scheduled.event.type == MusicalEventType::NoteOn &&
                !GroovePuterOutput::allowsMidiNoteOn(scheduled.event)) {
                continue;
            }
            return true;
        }
    }

    void invalidateTarget(MusicalEventTarget target) {
        switch (target) {
            case MusicalEventTarget::SynthA:
                synthAGeneration_.incrementRelaxed();
                pendingSynthAEpoch_.incrementRelaxed();
                break;
            case MusicalEventTarget::SynthB:
                synthBGeneration_.incrementRelaxed();
                pendingSynthBEpoch_.incrementRelaxed();
                break;
            case MusicalEventTarget::Drums:
                drumsGeneration_.incrementRelaxed();
                pendingDrumsEpoch_.incrementRelaxed();
                break;
            case MusicalEventTarget::Dx:
                // PatternPlayer does not publish DX in the accepted routing
                // model. Fail closed instead of aliasing its lifecycle to A/B.
                break;
        }
    }

    uint32_t generationFor(MusicalEventTarget target) const {
        switch (target) {
            case MusicalEventTarget::SynthA:
                return synthAGeneration_.loadAcquire();
            case MusicalEventTarget::SynthB:
                return synthBGeneration_.loadAcquire();
            case MusicalEventTarget::Drums:
                return drumsGeneration_.loadAcquire();
            case MusicalEventTarget::Dx:
                return 0;
        }
        return 0;
    }

    uint8_t takePendingAllNotesOffMask() {
        uint8_t mask = 0;
        const uint32_t synthAEpoch = pendingSynthAEpoch_.loadAcquire();
        const uint32_t synthBEpoch = pendingSynthBEpoch_.loadAcquire();
        const uint32_t drumsEpoch = pendingDrumsEpoch_.loadAcquire();
        if (synthAEpoch != consumedSynthAEpoch_) {
            consumedSynthAEpoch_ = synthAEpoch;
            mask |= kSynthAMask;
        }
        if (synthBEpoch != consumedSynthBEpoch_) {
            consumedSynthBEpoch_ = synthBEpoch;
            mask |= kSynthBMask;
        }
        if (drumsEpoch != consumedDrumsEpoch_) {
            consumedDrumsEpoch_ = drumsEpoch;
            mask |= kDrumsMask;
        }

        // OutputOwnership increments these bounded epochs only when a transition
        // removes the external side. The existing dispatcher turns the mask into
        // source/target-scoped NoteOff cleanup; no direct USB call is added here.
        const uint32_t outputEpochs = packedOutputDisableEpochs();
        const uint32_t changed = outputEpochs ^ consumedOutputDisableEpochs_;
        if ((changed & 0x000000FFu) != 0u) mask |= kSynthAMask;
        if ((changed & 0x0000FF00u) != 0u) mask |= kSynthBMask;
        if ((changed & 0x00FF0000u) != 0u) mask |= kDrumsMask;
        consumedOutputDisableEpochs_ = outputEpochs;
        return mask;
    }

    // Used by the compatibility facade when publication happens outside the
    // realtime AudioTask render bracket (notably synchronous WAV export and
    // control-plane lifecycle calls). NoteOn is suppressed; critical events
    // invalidate the target and request final cleanup.
    bool suppressNonRealtimeEvent(const MusicalEvent& event) {
        suppressedNonRealtime_.incrementRelaxed();
        if (event.type != MusicalEventType::NoteOn) {
            invalidateTarget(event.target);
        }
        return false;
    }

    // Call only when AudioTask publication is quiescent under AudioMutationGate.
    void discardPending() {
        const uint32_t head = head_.loadAcquire();
        tail_.storeRelease(head);
        consumedSynthAEpoch_ = pendingSynthAEpoch_.loadAcquire();
        consumedSynthBEpoch_ = pendingSynthBEpoch_.loadAcquire();
        consumedDrumsEpoch_ = pendingDrumsEpoch_.loadAcquire();
    }

    std::size_t approximateSize() const {
        const uint32_t head = head_.loadAcquire();
        const uint32_t tail = tail_.loadAcquire();
        return head >= tail
            ? static_cast<std::size_t>(head - tail)
            : static_cast<std::size_t>(kStorageSize - tail + head);
    }

    uint32_t droppedNoteOnCount() const {
        return droppedNoteOn_.loadRelaxed();
    }

    uint32_t droppedCriticalCount() const {
        return droppedCritical_.loadRelaxed();
    }

    uint32_t suppressedNonRealtimeCount() const {
        return suppressedNonRealtime_.loadRelaxed();
    }

private:
    static uint32_t packedOutputDisableEpochs() {
        return static_cast<uint32_t>(GroovePuterOutput::midiDisableEpoch(
                   GroovePuterOutput::Track::SynthA)) |
               (static_cast<uint32_t>(GroovePuterOutput::midiDisableEpoch(
                    GroovePuterOutput::Track::SynthB)) << 8u) |
               (static_cast<uint32_t>(GroovePuterOutput::midiDisableEpoch(
                    GroovePuterOutput::Track::Drums)) << 16u);
    }

    static bool isRealtimeProducerContext() {
#if defined(ESP32) || defined(ESP_PLATFORM)
        const char* taskName = pcTaskGetName(nullptr);
        return taskName != nullptr && std::strcmp(taskName, "AudioTask") == 0;
#else
        return true;
#endif
    }

    ScheduledMusicalEvent events_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord synthAGeneration_;
    MidiRealtimeWord synthBGeneration_;
    MidiRealtimeWord drumsGeneration_;
    MidiRealtimeWord pendingSynthAEpoch_;
    MidiRealtimeWord pendingSynthBEpoch_;
    MidiRealtimeWord pendingDrumsEpoch_;
    MidiRealtimeWord publicationSequence_;
    MidiRealtimeWord droppedNoteOn_;
    MidiRealtimeWord droppedCritical_;
    MidiRealtimeWord suppressedNonRealtime_;
    uint32_t consumedSynthAEpoch_{0};
    uint32_t consumedSynthBEpoch_{0};
    uint32_t consumedDrumsEpoch_{0};
    uint32_t consumedOutputDisableEpochs_{0};
};

#endif  // GROOVEPUTER_SCHEDULED_MUSICAL_EVENT_QUEUE_H
