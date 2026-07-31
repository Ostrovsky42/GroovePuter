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
MidiBlockAnchorClock g_anchorClock;
MidiDispatchDiagnostics g_diagnostics;
TaskHandle_t g_dispatchTaskHandle = nullptr;
bool g_registered = false;

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
    // System realtime transport owns an equal sample timestamp. Within the
    // transport queue, Start/Stop is published before Clock at that timestamp.
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
        "[MIDI-DISPATCH] sched=%u transport=%u live=%u sent=%u/%u "
        "clockSent=%u clockLate=%u clockDropped=%u start=%u stop=%u "
        "transportFail=%u late=%u maxLateUs=%u stale=%u/%u badFrame=%u "
        "drop=%u/%u transportDrop=%u overflow=%u recovery=%u "
        "liveDrop=%u/%u suppressed=%u panic=%u/%u\n",
        static_cast<unsigned>(scheduledDepth),
        static_cast<unsigned>(transportDepth),
        static_cast<unsigned>(g_controlQueue.approximateSize()),
        static_cast<unsigned>(g_diagnostics.dispatchedScheduled),
        static_cast<unsigned>(g_diagnostics.dispatchedControl),
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

void midiDispatchTask(void*) {
    if (!g_output.begin()) {
        Serial.println("[MIDI-DISPATCH] USB MIDI begin failed");
    }

    ScheduledMusicalEvent pendingMusical{};
    ScheduledMidiTransportEvent pendingTransport{};
    bool hasPendingMusical = false;
    bool hasPendingTransport = false;

    while (true) {
        g_output.pollConnection();

        // Existing PatternPlayer cleanup stays ahead of Stop. This preserves the
        // accepted NoteOff/AllNotesOff lifecycle: release owned notes first,
        // then deliver the transport Stop boundary. No queue drain depends on
        // Arduino loop scheduling.
        dispatchPatternPanics();
        dispatchControlPanics();

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

        if (!hasPendingTransport && !hasPendingMusical) {
            drainControlEvents();
            logDiagnosticsIfDue();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
            continue;
        }

        const bool dispatchTransport = hasPendingTransport &&
            (!hasPendingMusical ||
             transportBeforeMusical(pendingTransport, pendingMusical));
        const uint32_t blockSequence = dispatchTransport
            ? pendingTransport.blockSequence
            : pendingMusical.blockSequence;
        const uint16_t frameOffset = dispatchTransport
            ? pendingTransport.frameOffset
            : pendingMusical.frameOffset;

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
            // Service a bounded amount of non-scheduled live input while the
            // next sample deadline is safely in the future. Clock timing can no
            // longer be delayed by draining the entire control queue.
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
            if (dispatchTransport &&
                pendingTransport.type == MidiTransportEventType::Clock) {
                ++g_diagnostics.clockLate;
                if (lateness > kClockStaleThresholdUs) {
                    ++g_diagnostics.clockDropped;
                    hasPendingTransport = false;
                    continue;
                }
            }
        }

        if (dispatchTransport) {
            // Stop can invalidate a Clock while this task is waiting for its
            // sample deadline. Re-check the transport generation immediately
            // before the USB write so a stale pulse can never arrive post-Stop.
            if (pendingTransport.type == MidiTransportEventType::Clock &&
                !scheduledMidiTransportEventGenerationIsCurrent(
                    pendingTransport,
                    g_patternQueue->transportQueue().generation())) {
                ++g_diagnostics.clockStaleGenerationDrops;
            } else {
                dispatchTransportEvent(pendingTransport);
            }
            hasPendingTransport = false;
        } else {
            // Re-check after waiting: a lifecycle transition may have invalidated
            // the target while this event was pending.
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

    // USB MIDI 1.0 uses CIN 0xF for a one-byte System Real-Time message.
    // The pinned Arduino-ESP32 3.2.2 USBMIDI API accepts the complete 4-byte
    // midiEventPacket_t via writePacket(). Cable number remains zero.
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

void publishCardputerUsbMidiBlockAnchor(uint32_t blockSequence,
                                        uint32_t playbackStartMicros) {
    g_anchorClock.publish(blockSequence,
                          playbackStartMicros + kOutputLatencyUs);
    g_anchorClock.markPublished();
    notifyDispatcher();
}
