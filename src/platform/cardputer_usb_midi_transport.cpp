#include "cardputer_usb_midi_transport.h"
#include "cardputer_usb_midi_service.h"

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "src/audio/audio_config.h"
#include "src/input/musical_event_queue.h"
#include "src/input/musical_event_router.h"
#include "src/midi/midi_control_event_queue.h"
#include "src/midi/scheduled_midi_transport_event.h"
#include "src/midi/scheduled_musical_event_queue.h"
#include "src/midi/smf_dispatch_policy.h"
#include "src/midi/scheduled_smf_midi_event_queue.h"
#include "src/midi/usb_midi_output.h"

#if ARDUINO_USB_MODE
#error "USB MIDI requires Cardputer USBMode=default (USB-OTG/TinyUSB)"
#endif

namespace {
constexpr uint8_t kCinNoteOff = 0x08;
constexpr uint8_t kCinNoteOn = 0x09;
constexpr uint8_t kCinSingleByte = 0x0F;
constexpr uint8_t kStatusNoteOff = 0x80;
constexpr uint8_t kStatusNoteOn = 0x90;
constexpr uint8_t kStatusTimingClock = 0xF8;
constexpr uint8_t kStatusStart = 0xFA;
constexpr uint8_t kStatusStop = 0xFC;
constexpr uint32_t kBlockDurationUs = static_cast<uint32_t>(
    (1000000ULL * static_cast<uint64_t>(kBlockFrames)) /
    static_cast<uint64_t>(kSampleRate));
// One audio block keeps MIDI deadlines in the future while DSP renders and the
// current DMA buffer is queued. This constant affects latency, not spacing.
constexpr uint32_t kOutputLatencyUs = kBlockDurationUs;
constexpr uint32_t kDiagnosticsPeriodMs = 5000;
// GroovePuter tops out at 250 BPM, where MIDI Clock is 10 ms apart. A clock
// pulse more than 5 ms late is stale: dropping it avoids catch-up bursts while
// retaining at most the currently useful pulse.
constexpr uint32_t kClockStaleThresholdUs = 5000;
// A stale NoteOn has no cleanup responsibility and must not create a catch-up
// burst after USB endpoint backpressure. NoteOff remains cleanup-critical.
constexpr uint32_t kSmfStaleNoteOnThresholdUs = 100000;
constexpr TickType_t kSmfRetryDelay = pdMS_TO_TICKS(1);
constexpr uint32_t kSmfCleanupRetryDelayMs = 10;
constexpr uint8_t kSmfCleanupAttemptLimit = 8;
constexpr std::size_t kControlDrainBudget = 8;

class MidiBlockAnchorClock {
public:
    void publish(uint32_t blockSequence, uint32_t playbackStartMicros) {
        const uint32_t version = version_;
        version_ = version + 1u;
        asm volatile("memw" ::: "memory");
        blockSequence_ = blockSequence;
        playbackStartMicros_ = playbackStartMicros;
        asm volatile("memw" ::: "memory");
        version_ = version + 2u;
    }

    bool snapshot(uint32_t& blockSequence,
                  uint32_t& playbackStartMicros) const {
        for (int attempt = 0; attempt < 4; ++attempt) {
            const uint32_t first = version_;
            asm volatile("memw" ::: "memory");
            if ((first & 1u) != 0u) continue;

            const uint32_t sequence = blockSequence_;
            const uint32_t start = playbackStartMicros_;
            asm volatile("memw" ::: "memory");
            const uint32_t second = version_;
            if (first == second && (second & 1u) == 0u) {
                blockSequence = sequence;
                playbackStartMicros = start;
                return published_;
            }
        }
        return false;
    }

    void markPublished() {
        published_ = true;
        asm volatile("memw" ::: "memory");
    }

private:
    alignas(4) volatile uint32_t version_{0};
    alignas(4) volatile uint32_t blockSequence_{0};
    alignas(4) volatile uint32_t playbackStartMicros_{0};
    alignas(4) volatile bool published_{false};
};

struct MidiDispatchDiagnostics {
    uint32_t dispatchedScheduled{0};
    uint32_t dispatchedControl{0};
    uint32_t staleGenerationDrops{0};
    uint32_t invalidFrameDrops{0};
    uint32_t lateEvents{0};
    uint32_t maximumLatenessUs{0};
    uint32_t patternPanics{0};
    uint32_t controlPanics{0};
    uint32_t clockSent{0};
    uint32_t clockLate{0};
    uint32_t clockDropped{0};
    uint32_t clockStaleGenerationDrops{0};
    uint32_t startSent{0};
    uint32_t stopSent{0};
    uint32_t transportSendFailures{0};
    uint32_t smfSent{0};
    uint32_t smfStaleGenerationDrops{0};
    uint32_t smfPanics{0};
    uint32_t smfSendRetries{0};
    uint32_t smfSendDrops{0};
    uint32_t smfLateNoteOnDrops{0};
    uint32_t smfCleanupRetries{0};
    uint32_t smfTransportAborts{0};
};

// Construction of USBMIDI registers its interface descriptor before Arduino
// starts the TinyUSB composite. No USB methods or application state are touched
// during global initialization.
CardputerUsbMidiTransport g_transport;
UsbMidiOutput g_output(
    g_transport,
    UsbMidiRouteConfig{
        7,     // live Synth A -> MIDI channel 8
        7,     // PatternPlayer Synth A -> MIDI channel 8
        8,     // PatternPlayer Synth B -> MIDI channel 9
        true,
        true,
    });
MidiControlEventQueue g_controlQueue;
MusicalEventQueue* g_patternQueue = nullptr;
ScheduledSmfMidiEventQueue* g_smfQueue = nullptr;
MidiBlockAnchorClock g_anchorClock;
MidiDispatchDiagnostics g_diagnostics;
TaskHandle_t g_dispatchTaskHandle = nullptr;
bool g_registered = false;
bool g_smfCleanupPending = false;
bool g_smfCleanupMustAbort = false;
uint32_t g_nextSmfCleanupAttemptMs = 0;
uint8_t g_smfCleanupAttempts = 0;

void notifyDispatcher() {
    if (g_dispatchTaskHandle != nullptr) {
        xTaskNotifyGive(g_dispatchTaskHandle);
    }
}

class QueuedUsbMidiSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        g_controlQueue.tryPush(event);
        notifyDispatcher();
    }
};

QueuedUsbMidiSink g_queueSink;

void beginSmfCleanup(bool transportFailure = false) {
    if (!g_smfCleanupPending) {
        g_nextSmfCleanupAttemptMs = 0;
        g_smfCleanupAttempts = 0;
        g_smfCleanupMustAbort = transportFailure;
    } else if (transportFailure) {
        g_smfCleanupMustAbort = true;
    }
    g_smfCleanupPending = true;
}

void reportSmfTransportFailure() {
    g_output.abandonAllSmfNotes();
    g_smfQueue->reportTransportFailure();
    g_smfCleanupPending = false;
    g_smfCleanupMustAbort = false;
    g_nextSmfCleanupAttemptMs = 0;
    g_smfCleanupAttempts = 0;
    ++g_diagnostics.smfTransportAborts;
    Serial.println("[SMF-ERROR] USB MIDI endpoint blocked; playback stopped");
}

MusicalEvent panicEvent(MusicalEventSource source,
                        MusicalEventTarget target) {
    return MusicalEvent{
        MusicalEventType::AllNotesOff,
        source,
        target,
        0,
        0,
        0,
    };
}

void dispatchPatternPanics() {
    if (g_patternQueue == nullptr) return;
    const uint8_t mask = g_patternQueue->takePendingAllNotesOffMask();
    if (mask & ScheduledMusicalEventQueue::kSynthAMask) {
        g_output.handleMusicalEvent(
            panicEvent(MusicalEventSource::PatternPlayer,
                       MusicalEventTarget::SynthA));
        ++g_diagnostics.patternPanics;
    }
    if (mask & ScheduledMusicalEventQueue::kSynthBMask) {
        g_output.handleMusicalEvent(
            panicEvent(MusicalEventSource::PatternPlayer,
                       MusicalEventTarget::SynthB));
        ++g_diagnostics.patternPanics;
    }
}

void dispatchControlPanics() {
    const uint8_t mask = g_controlQueue.takePendingAllNotesOffMask();
    if (mask & MidiControlEventQueue::kSynthAMask) {
        g_output.handleMusicalEvent(
            panicEvent(MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthA));
        ++g_diagnostics.controlPanics;
    }
    if (mask & MidiControlEventQueue::kSynthBMask) {
        g_output.handleMusicalEvent(
            panicEvent(MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthB));
        ++g_diagnostics.controlPanics;
    }
    if (mask & MidiControlEventQueue::kDrumsMask) {
        g_output.handleMusicalEvent(
            panicEvent(MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::Drums));
        ++g_diagnostics.controlPanics;
    }
    if (mask & MidiControlEventQueue::kDxMask) {
        g_output.handleMusicalEvent(
            panicEvent(MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::Dx));
        ++g_diagnostics.controlPanics;
    }
}

void dispatchSmfPanic() {
    if (g_smfQueue == nullptr) return;
    uint32_t generation = 0;
    if (g_smfQueue->takePendingPanic(generation)) {
        (void)generation;
        beginSmfCleanup();
        ++g_diagnostics.smfPanics;
    }
    if (!g_smfCleanupPending) return;

    const uint32_t nowMs = millis();
    if (g_nextSmfCleanupAttemptMs != 0 &&
        static_cast<int32_t>(nowMs - g_nextSmfCleanupAttemptMs) < 0) {
        return;
    }
    if (g_output.releaseAllSmfNotes()) {
        if (g_smfCleanupMustAbort) {
            reportSmfTransportFailure();
            return;
        }
        g_smfCleanupPending = false;
        g_smfCleanupMustAbort = false;
        g_nextSmfCleanupAttemptMs = 0;
        g_smfCleanupAttempts = 0;
    } else {
        if (g_smfCleanupAttempts < UINT8_MAX) ++g_smfCleanupAttempts;
        g_nextSmfCleanupAttemptMs = nowMs + kSmfCleanupRetryDelayMs;
        ++g_diagnostics.smfCleanupRetries;
        if (g_smfCleanupAttempts >= kSmfCleanupAttemptLimit) {
            // A host that does not consume the USB MIDI IN endpoint can keep
            // every NoteOff rejected indefinitely. Give up only SMF ownership,
            // invalidate queued events, and let the player surface the fault.
            // Other live/Pattern owners remain accounted for locally.
            reportSmfTransportFailure();
        }
    }
}

void drainControlEvents(std::size_t budget = kControlDrainBudget) {
    MusicalEvent event{};
    std::size_t drained = 0;
    while (drained < budget && g_controlQueue.tryPop(event)) {
        g_output.handleMusicalEvent(event);
        ++g_diagnostics.dispatchedControl;
        ++drained;
    }
}

bool deadlineFor(uint32_t blockSequence,
                 uint16_t frameOffset,
                 uint32_t& deadlineMicros) {
    uint32_t anchorSequence = 0;
    uint32_t anchorStartMicros = 0;
    if (!g_anchorClock.snapshot(anchorSequence, anchorStartMicros)) return false;

    const int32_t blockDelta = static_cast<int32_t>(
        blockSequence - anchorSequence);
    if (blockDelta > 0) return false;

    const int64_t blockOffsetUs =
        static_cast<int64_t>(blockDelta) *
        static_cast<int64_t>(kBlockDurationUs);
    const uint32_t frameOffsetUs = static_cast<uint32_t>(
        (1000000ULL * static_cast<uint64_t>(frameOffset)) /
        static_cast<uint64_t>(kSampleRate));
    deadlineMicros = static_cast<uint32_t>(
        static_cast<int64_t>(anchorStartMicros) + blockOffsetUs +
        static_cast<int64_t>(frameOffsetUs));
    return true;
}

bool transportBeforeMusical(const ScheduledMidiTransportEvent& transport,
                            const ScheduledMusicalEvent& musical) {
    if (transport.blockSequence != musical.blockSequence) {
        return midiSequenceBefore(transport.blockSequence,
                                  musical.blockSequence);
    }
    if (transport.frameOffset != musical.frameOffset) {
        return transport.frameOffset < musical.frameOffset;
    }
    return true;
}

bool transportBeforeSmf(const ScheduledMidiTransportEvent& transport,
                        const ScheduledSmfMidiEvent& smf) {
    if (transport.blockSequence != smf.blockSequence) {
        return midiSequenceBefore(transport.blockSequence, smf.blockSequence);
    }
    if (transport.frameOffset != smf.frameOffset) {
        return transport.frameOffset < smf.frameOffset;
    }
    return true;
}

bool musicalBeforeSmf(const ScheduledMusicalEvent& musical,
                      const ScheduledSmfMidiEvent& smf) {
    if (musical.blockSequence != smf.blockSequence) {
        return midiSequenceBefore(musical.blockSequence, smf.blockSequence);
    }
    if (musical.frameOffset != smf.frameOffset) {
        return musical.frameOffset < smf.frameOffset;
    }
    // Pattern playback retains priority at an identical sample timestamp. The
    // ordering within the SMF queue itself remains deterministic and stable.
    return true;
}

bool dispatchTransportEvent(const ScheduledMidiTransportEvent& event) {
    bool sent = false;
    switch (event.type) {
        case MidiTransportEventType::Clock:
            sent = g_transport.sendTimingClock();
            if (sent) {
                ++g_diagnostics.clockSent;
            } else {
                ++g_diagnostics.clockDropped;
            }
            break;
        case MidiTransportEventType::Start:
            sent = g_transport.sendStart();
            if (sent) {
                ++g_diagnostics.startSent;
            } else {
                ++g_diagnostics.transportSendFailures;
            }
            break;
        case MidiTransportEventType::Stop:
            sent = g_transport.sendStop();
            if (sent) {
                ++g_diagnostics.stopSent;
            } else {
                ++g_diagnostics.transportSendFailures;
            }
            break;
    }
    return sent;
}

bool dispatchSmfEvent(const ScheduledSmfMidiEvent& event) {
    if (event.type == ScheduledSmfMidiEventType::NoteOn) {
        return g_output.handleSmfNoteOn(
            event.channel, event.note, event.velocity);
    }
    return g_output.handleSmfNoteOff(
        event.channel, event.note, event.velocity);
}

void logDiagnosticsIfDue() {
    static uint32_t lastLogMs = 0;
    const uint32_t nowMs = millis();
    if (nowMs - lastLogMs < kDiagnosticsPeriodMs) return;
    lastLogMs = nowMs;

    const std::size_t scheduledDepth = g_patternQueue
        ? g_patternQueue->approximateSize()
        : 0;
    const std::size_t transportDepth = g_patternQueue
        ? g_patternQueue->transportQueue().approximateSize()
        : 0;
    const std::size_t smfDepth = g_smfQueue
        ? g_smfQueue->approximateSize()
        : 0;
    const uint32_t scheduledNoteDrops = g_patternQueue
        ? g_patternQueue->droppedNoteOnCount()
        : 0;
    const uint32_t scheduledCriticalDrops = g_patternQueue
        ? g_patternQueue->droppedCriticalCount()
        : 0;
    const uint32_t suppressed = g_patternQueue
        ? g_patternQueue->suppressedNonRealtimeCount()
        : 0;
    const uint32_t transportQueueDrops = g_patternQueue
        ? g_patternQueue->transportQueue().droppedClockCount()
        : 0;
    const uint32_t transportCriticalOverflow = g_patternQueue
        ? g_patternQueue->transportQueue().criticalOverflowCount()
        : 0;
    const uint32_t transportRecoveries = g_patternQueue
        ? g_patternQueue->transportQueue().criticalRecoveryCount()
        : 0;

    Serial.printf(
        "[MIDI-DISPATCH] sched=%u transport=%u smf=%u live=%u sent=%u/%u "
        "smfSent=%u smfStale=%u smfPanic=%u smfRetry=%u smfDrop=%u "
        "smfLateDrop=%u smfCleanRetry=%u smfAbort=%u "
        "clockSent=%u clockLate=%u clockDropped=%u start=%u stop=%u "
        "transportFail=%u late=%u maxLateUs=%u stale=%u/%u badFrame=%u "
        "drop=%u/%u transportDrop=%u overflow=%u recovery=%u "
        "liveDrop=%u/%u suppressed=%u panic=%u/%u\n",
        static_cast<unsigned>(scheduledDepth),
        static_cast<unsigned>(transportDepth),
        static_cast<unsigned>(smfDepth),
        static_cast<unsigned>(g_controlQueue.approximateSize()),
        static_cast<unsigned>(g_diagnostics.dispatchedScheduled),
        static_cast<unsigned>(g_diagnostics.dispatchedControl),
        static_cast<unsigned>(g_diagnostics.smfSent),
        static_cast<unsigned>(g_diagnostics.smfStaleGenerationDrops),
        static_cast<unsigned>(g_diagnostics.smfPanics),
        static_cast<unsigned>(g_diagnostics.smfSendRetries),
        static_cast<unsigned>(g_diagnostics.smfSendDrops),
        static_cast<unsigned>(g_diagnostics.smfLateNoteOnDrops),
        static_cast<unsigned>(g_diagnostics.smfCleanupRetries),
        static_cast<unsigned>(g_diagnostics.smfTransportAborts),
        static_cast<unsigned>(g_diagnostics.clockSent),
        static_cast<unsigned>(g_diagnostics.clockLate),
        static_cast<unsigned>(g_diagnostics.clockDropped),
        static_cast<unsigned>(g_diagnostics.startSent),
        static_cast<unsigned>(g_diagnostics.stopSent),
        static_cast<unsigned>(g_diagnostics.transportSendFailures),
        static_cast<unsigned>(g_diagnostics.lateEvents),
        static_cast<unsigned>(g_diagnostics.maximumLatenessUs),
        static_cast<unsigned>(g_diagnostics.staleGenerationDrops),
        static_cast<unsigned>(g_diagnostics.clockStaleGenerationDrops),
        static_cast<unsigned>(g_diagnostics.invalidFrameDrops),
        static_cast<unsigned>(scheduledNoteDrops),
        static_cast<unsigned>(scheduledCriticalDrops),
        static_cast<unsigned>(transportQueueDrops),
        static_cast<unsigned>(transportCriticalOverflow),
        static_cast<unsigned>(transportRecoveries),
        static_cast<unsigned>(g_controlQueue.droppedNoteOnCount()),
        static_cast<unsigned>(g_controlQueue.droppedCriticalCount()),
        static_cast<unsigned>(suppressed),
        static_cast<unsigned>(g_diagnostics.patternPanics),
        static_cast<unsigned>(g_diagnostics.controlPanics));
}

enum class PendingKind : uint8_t {
    None = 0,
    Transport,
    Pattern,
    Smf,
};

void midiDispatchTask(void*) {
    if (!g_output.begin()) {
        Serial.println("[MIDI-DISPATCH] USB MIDI begin failed");
    }

    ScheduledMusicalEvent pendingMusical{};
    ScheduledMidiTransportEvent pendingTransport{};
    ScheduledSmfMidiEvent pendingSmf{};
    bool hasPendingMusical = false;
    bool hasPendingTransport = false;
    bool hasPendingSmf = false;
    uint8_t smfFailedAttempts = 0;

    auto clearPendingSmf = [&]() {
        hasPendingSmf = false;
        smfFailedAttempts = 0;
    };

    while (true) {
        g_output.pollConnection();

        // Existing PatternPlayer and live cleanup remain ahead of scheduled
        // lifecycle traffic. SMF cleanup is independent and cannot silence a
        // Pattern/PERFORM owner of the same physical channel+note.
        dispatchPatternPanics();
        dispatchControlPanics();
        dispatchSmfPanic();

        if (g_patternQueue != nullptr) {
            if (!hasPendingTransport) {
                hasPendingTransport =
                    g_patternQueue->transportQueue().tryPop(pendingTransport);
                if (!hasPendingTransport) {
                    hasPendingTransport = g_patternQueue->transportQueue()
                        .takePendingCriticalRecovery(pendingTransport);
                }
            }
            if (!hasPendingMusical) {
                hasPendingMusical = g_patternQueue->tryPop(pendingMusical);
            }
        }
        if (g_smfQueue != nullptr && !hasPendingSmf && !g_smfCleanupPending) {
            hasPendingSmf = g_smfQueue->tryPop(pendingSmf);
        }

        if (hasPendingTransport &&
            !scheduledMidiTransportEventFrameIsValid(
                pendingTransport, kBlockFrames)) {
            ++g_diagnostics.invalidFrameDrops;
            hasPendingTransport = false;
            continue;
        }

        if (hasPendingTransport &&
            pendingTransport.type == MidiTransportEventType::Clock &&
            !scheduledMidiTransportEventGenerationIsCurrent(
                pendingTransport,
                g_patternQueue->transportQueue().generation())) {
            ++g_diagnostics.clockStaleGenerationDrops;
            hasPendingTransport = false;
            continue;
        }

        if (hasPendingMusical &&
            !scheduledMusicalEventFrameIsValid(
                pendingMusical, kBlockFrames)) {
            ++g_diagnostics.invalidFrameDrops;
            g_output.handleMusicalEvent(
                panicEvent(MusicalEventSource::PatternPlayer,
                           pendingMusical.event.target));
            hasPendingMusical = false;
            continue;
        }

        if (hasPendingMusical) {
            const uint32_t currentGeneration =
                g_patternQueue->generationFor(pendingMusical.event.target);
            if (!scheduledMusicalEventGenerationIsCurrent(
                    pendingMusical, currentGeneration)) {
                ++g_diagnostics.staleGenerationDrops;
                hasPendingMusical = false;
                continue;
            }
        }

        if (hasPendingSmf &&
            !scheduledSmfMidiEventFrameIsValid(pendingSmf, kBlockFrames)) {
            ++g_diagnostics.invalidFrameDrops;
            beginSmfCleanup();
            clearPendingSmf();
            continue;
        }

        if (hasPendingSmf && g_smfQueue != nullptr &&
            g_smfQueue->transportFailed()) {
            ++g_diagnostics.smfStaleGenerationDrops;
            clearPendingSmf();
            continue;
        }

        if (hasPendingSmf && g_smfQueue != nullptr &&
            !scheduledSmfMidiEventGenerationIsCurrent(
                pendingSmf, g_smfQueue->generation())) {
            ++g_diagnostics.smfStaleGenerationDrops;
            clearPendingSmf();
            continue;
        }

        if (!hasPendingTransport && !hasPendingMusical && !hasPendingSmf) {
            drainControlEvents();
            logDiagnosticsIfDue();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
            continue;
        }

        PendingKind kind = PendingKind::None;
        if (hasPendingTransport) kind = PendingKind::Transport;
        if (hasPendingMusical) {
            if (kind == PendingKind::None ||
                (kind == PendingKind::Transport &&
                 !transportBeforeMusical(pendingTransport, pendingMusical))) {
                kind = PendingKind::Pattern;
            }
        }
        if (hasPendingSmf && !g_smfCleanupPending) {
            bool smfWins = kind == PendingKind::None;
            if (kind == PendingKind::Transport) {
                smfWins = !transportBeforeSmf(pendingTransport, pendingSmf);
            } else if (kind == PendingKind::Pattern) {
                smfWins = !musicalBeforeSmf(pendingMusical, pendingSmf);
            }
            if (smfWins) kind = PendingKind::Smf;
        }

        if (kind == PendingKind::None) {
            drainControlEvents();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
            continue;
        }

        uint32_t blockSequence = 0;
        uint16_t frameOffset = 0;
        switch (kind) {
            case PendingKind::Transport:
                blockSequence = pendingTransport.blockSequence;
                frameOffset = pendingTransport.frameOffset;
                break;
            case PendingKind::Pattern:
                blockSequence = pendingMusical.blockSequence;
                frameOffset = pendingMusical.frameOffset;
                break;
            case PendingKind::Smf:
                blockSequence = pendingSmf.blockSequence;
                frameOffset = pendingSmf.frameOffset;
                break;
            case PendingKind::None:
                break;
        }

        uint32_t deadlineMicros = 0;
        if (!deadlineFor(blockSequence, frameOffset, deadlineMicros)) {
            drainControlEvents();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
            continue;
        }

        const uint32_t nowMicros = micros();
        const int32_t untilDeadline = static_cast<int32_t>(
            deadlineMicros - nowMicros);
        if (untilDeadline > 1500) {
            drainControlEvents();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
            continue;
        }
        if (untilDeadline > 0) {
            esp_rom_delay_us(static_cast<uint32_t>(untilDeadline));
        } else if (untilDeadline < 0) {
            const uint32_t lateness = static_cast<uint32_t>(-untilDeadline);
            ++g_diagnostics.lateEvents;
            if (lateness > g_diagnostics.maximumLatenessUs) {
                g_diagnostics.maximumLatenessUs = lateness;
            }
            if (kind == PendingKind::Transport &&
                pendingTransport.type == MidiTransportEventType::Clock) {
                ++g_diagnostics.clockLate;
                if (lateness > kClockStaleThresholdUs) {
                    ++g_diagnostics.clockDropped;
                    hasPendingTransport = false;
                    continue;
                }
            }
            if (kind == PendingKind::Smf &&
                pendingSmf.type == ScheduledSmfMidiEventType::NoteOn &&
                lateness > kSmfStaleNoteOnThresholdUs) {
                ++g_diagnostics.smfLateNoteOnDrops;
                clearPendingSmf();
                continue;
            }
        }

        if (kind == PendingKind::Transport) {
            if (pendingTransport.type == MidiTransportEventType::Clock &&
                !scheduledMidiTransportEventGenerationIsCurrent(
                    pendingTransport,
                    g_patternQueue->transportQueue().generation())) {
                ++g_diagnostics.clockStaleGenerationDrops;
            } else {
                dispatchTransportEvent(pendingTransport);
            }
            hasPendingTransport = false;
        } else if (kind == PendingKind::Pattern) {
            if (scheduledMusicalEventGenerationIsCurrent(
                    pendingMusical,
                    g_patternQueue->generationFor(
                        pendingMusical.event.target))) {
                g_output.handleMusicalEvent(pendingMusical.event);
                ++g_diagnostics.dispatchedScheduled;
            } else {
                ++g_diagnostics.staleGenerationDrops;
            }
            hasPendingMusical = false;
        } else {
            if (g_smfQueue == nullptr ||
                !scheduledSmfMidiEventGenerationIsCurrent(
                    pendingSmf, g_smfQueue->generation())) {
                ++g_diagnostics.smfStaleGenerationDrops;
                clearPendingSmf();
            } else if (dispatchSmfEvent(pendingSmf)) {
                ++g_diagnostics.smfSent;
                clearPendingSmf();
            } else {
                ++g_diagnostics.smfSendRetries;
                if (smfFailedAttempts < UINT8_MAX) ++smfFailedAttempts;
                const SmfSendFailureAction action = smfSendFailureAction(
                    pendingSmf, smfFailedAttempts);
                if (action == SmfSendFailureAction::DropNoteOn) {
                    ++g_diagnostics.smfSendDrops;
                    beginSmfCleanup(true);
                    clearPendingSmf();
                } else if (action == SmfSendFailureAction::BeginCleanup) {
                    // UsbMidiOutput retains ownership after a failed NoteOff.
                    // Move recovery to the paced all-notes-off path.
                    beginSmfCleanup(true);
                    clearPendingSmf();
                }
                // A pending task notification must not turn USB backpressure
                // into a zero-delay retry loop that starves UI and TinyUSB.
                vTaskDelay(kSmfRetryDelay);
            }
        }

        drainControlEvents(2);
        logDiagnosticsIfDue();
    }
}

}  // namespace

uint8_t CardputerUsbMidiTransport::clamp7Bit(uint8_t value) {
    return value > 127 ? 127 : value;
}

uint8_t CardputerUsbMidiTransport::clampChannel(uint8_t channel) {
    return channel > 15 ? 15 : channel;
}

bool CardputerUsbMidiTransport::begin() {
    // The USBMIDI member constructor has already registered the MIDI interface.
    // With CDCOnBoot enabled, Arduino app_main() starts the complete TinyUSB
    // composite before setup(). USBMIDI::begin() is a no-op in the pinned core.
    midi_.begin();
    begun_ = true;
    return true;
}

bool CardputerUsbMidiTransport::mounted() const {
    return begun_ && static_cast<bool>(USB);
}

bool CardputerUsbMidiTransport::writeChannelPacket(uint8_t codeIndex,
                                                    uint8_t statusBase,
                                                    uint8_t zeroBasedChannel,
                                                    uint8_t note,
                                                    uint8_t velocity) {
    if (!mounted()) return false;

    midiEventPacket_t packet{
        codeIndex,
        static_cast<uint8_t>(statusBase | clampChannel(zeroBasedChannel)),
        clamp7Bit(note),
        clamp7Bit(velocity),
    };
    return midi_.writePacket(&packet);
}

bool CardputerUsbMidiTransport::writeRealtimePacket(uint8_t status) {
    if (!mounted()) return false;
    if (status != kStatusTimingClock &&
        status != kStatusStart &&
        status != kStatusStop) {
        return false;
    }

    midiEventPacket_t packet{
        kCinSingleByte,
        status,
        0,
        0,
    };
    return midi_.writePacket(&packet);
}

bool CardputerUsbMidiTransport::sendNoteOn(uint8_t zeroBasedChannel,
                                           uint8_t note,
                                           uint8_t velocity) {
    return writeChannelPacket(kCinNoteOn,
                              kStatusNoteOn,
                              zeroBasedChannel,
                              note,
                              velocity);
}

bool CardputerUsbMidiTransport::sendNoteOff(uint8_t zeroBasedChannel,
                                            uint8_t note,
                                            uint8_t velocity) {
    return writeChannelPacket(kCinNoteOff,
                              kStatusNoteOff,
                              zeroBasedChannel,
                              note,
                              velocity);
}

bool CardputerUsbMidiTransport::sendTimingClock() {
    return writeRealtimePacket(kStatusTimingClock);
}

bool CardputerUsbMidiTransport::sendStart() {
    return writeRealtimePacket(kStatusStart);
}

bool CardputerUsbMidiTransport::sendStop() {
    return writeRealtimePacket(kStatusStop);
}

void CardputerUsbMidiTransport::flush() {
    // USBMIDI::writePacket() queues a complete four-byte USB-MIDI event packet.
    // The pinned TinyUSB API exposes no additional flush operation.
}

bool registerCardputerUsbMidiSink(
    MusicalEventRouter& router,
    MusicalEventQueue& patternQueue) {
    if (g_registered) return true;

    g_patternQueue = &patternQueue;
    if (!router.addSink(g_queueSink)) {
        g_patternQueue = nullptr;
        return false;
    }

    const BaseType_t taskResult = xTaskCreatePinnedToCore(
        midiDispatchTask,
        "MidiDispatchTask",
        4096,
        nullptr,
        2,
        &g_dispatchTaskHandle,
        0);
    if (taskResult != pdPASS) {
        router.removeSink(g_queueSink);
        g_patternQueue = nullptr;
        g_dispatchTaskHandle = nullptr;
        return false;
    }

    g_registered = true;
    return true;
}

void registerCardputerSmfMidiQueue(ScheduledSmfMidiEventQueue* queue) {
    g_smfQueue = queue;
    notifyDispatcher();
}

void publishCardputerUsbMidiBlockAnchor(uint32_t blockSequence,
                                        uint32_t playbackStartMicros) {
    g_anchorClock.publish(blockSequence,
                          playbackStartMicros + kOutputLatencyUs);
    g_anchorClock.markPublished();
    notifyDispatcher();
}

bool snapshotCardputerUsbMidiBlockAnchor(uint32_t& blockSequence,
                                         uint32_t& playbackStartMicros) {
    return g_anchorClock.snapshot(blockSequence, playbackStartMicros);
}
