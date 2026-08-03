#include "cardputer_usb_midi_transport.h"
#include "cardputer_usb_midi_service.h"
#include "cardputer_usb_midi_tx_stress.h"

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "src/audio/audio_config.h"
#include "src/input/musical_event_queue.h"
#include "src/input/musical_event_router.h"
#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_control_event_queue.h"
#include "src/midi/external_midi_transport_event_queue.h"
#include "src/midi/pattern_drum_gate_scheduler.h"
#include "src/midi/project_smf_dispatch_policy.h"
#include "src/midi/project_transport_timeline.h"
#include "src/midi/scheduled_midi_transport_event.h"
#include "src/midi/scheduled_musical_event_queue.h"
#include "src/midi/smf_dispatch_policy.h"
#include "src/midi/smf_late_policy.h"
#include "src/midi/scheduled_smf_midi_event_queue.h"
#include "src/midi/usb_midi_output.h"
#include "src/midi/usb_endpoint_health.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/midi/usb_midi_realtime_parser.h"

#if ARDUINO_USB_MODE
#error "USB MIDI requires Cardputer USBMode=default (USB-OTG/TinyUSB)"
#endif

namespace {
constexpr uint8_t kCinNoteOff = 0x08;
constexpr uint8_t kCinNoteOn = 0x09;
constexpr uint8_t kCinControlChange = 0x0B;
constexpr uint8_t kCinSingleByte = 0x0F;
constexpr uint8_t kStatusNoteOff = 0x80;
constexpr uint8_t kStatusNoteOn = 0x90;
constexpr uint8_t kStatusControlChange = 0xB0;
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
constexpr uint32_t kSmfStaleNoteOnThresholdUs = kBlockDurationUs;
constexpr TickType_t kSmfRetryDelay = pdMS_TO_TICKS(1);
constexpr uint32_t kSmfCleanupRetryDelayMs = 10;
// The TinyUSB MIDI TX FIFO holds only CFG_TUD_MIDI_TX_BUFSIZE bytes (16 event
// packets), so a chord or a catch-up burst can fill it until the host polls the
// bulk IN endpoint again. Declaring the endpoint dead after 80 ms turned every
// such burst into a stopped transport; a receiver that never drains still gets
// surfaced, just after a window that real backpressure can survive.
constexpr uint8_t kSmfCleanupAttemptLimit = 32;
// Once stalled the retry is no longer a race against a busy FIFO but a probe for
// a host that may stay silent for minutes. One second keeps it negligible: the
// fast 10 ms phase stays bounded by kSmfCleanupAttemptLimit.
constexpr uint32_t kSmfStallProbeDelayMs = 1000;
// A 16-packet TinyUSB FIFO can reject a short chord while the receiver catches
// up. This threshold classifies only sustained backpressure for diagnostics.
constexpr uint32_t kUsbEndpointStallThresholdMs = 50;
constexpr std::size_t kControlDrainBudget = 8;
// Upper bound on how long the dispatcher may stay runnable without blocking.
// The task watchdog window is seconds; 10 ms keeps sample-accurate dispatch
// intact while guaranteeing the CPU0 idle task runs.
constexpr uint32_t kDispatchFairnessYieldMs = 10;
constexpr std::size_t kMidiRxDrainBudget = 32;

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
    uint32_t patternDrumGates{0};
    uint32_t patternDrumGateReleases{0};
    uint32_t patternDrumGateFailures{0};
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
    uint32_t smfLateNoteOnSent{0};
    uint32_t smfMaxNoteOnLatenessUs{0};
    uint32_t smfLateNoteOffSent{0};
    uint32_t smfTransportEpochDrops{0};
    uint32_t smfCleanupRetries{0};
    uint32_t smfTransportAborts{0};
    uint32_t smfTransportRecoveries{0};
    uint32_t dispatchFairnessYields{0};
    // Longest continuous stretch where the USB MIDI TX FIFO refused writes and
    // then recovered. Separates a short burst from a host that stopped reading.
    uint32_t smfMaxSendBlockUs{0};
    uint32_t externalRxClock{0};
    uint32_t externalRxStart{0};
    uint32_t externalRxContinue{0};
    uint32_t externalRxStop{0};
    uint32_t externalRxIgnored{0};
    uint32_t externalRxMasterIgnored{0};
    uint32_t outboundTransportSuppressed{0};
};

// Construction of USBMIDI registers its interface descriptor before Arduino
// starts the TinyUSB composite. No USB methods or application state are touched
// during global initialization.
CardputerUsbMidiTransport g_transport;
UsbEndpointHealth g_endpointHealth(kUsbEndpointStallThresholdMs);
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
ExternalMidiTransportEventQueue* g_externalTransportQueue = nullptr;
PatternDrumGateScheduler g_patternDrumGates;
MidiBlockAnchorClock g_anchorClock;
MidiDispatchDiagnostics g_diagnostics;
TaskHandle_t g_dispatchTaskHandle = nullptr;
bool g_registered = false;
bool g_smfCleanupPending = false;
// Set once cleanup has exceeded its attempt budget. Playback stays paused and
// the cleanup keeps probing until the endpoint accepts a write again.
bool g_smfTransportStalled = false;
uint32_t g_nextSmfCleanupAttemptMs = 0;
uint8_t g_smfCleanupAttempts = 0;
// Timestamp of the first failed SMF write of the current stall, used to report
// how long the endpoint actually refused data.
uint32_t g_smfSendBlockStartedUs = 0;
uint32_t g_externalRxPulseOrdinal = 0;

// Logged at the exact moment the endpoint enters or leaves a stall, so a phase
// change can be aligned with mount and suspend edges. A momentary
// Ready<->Backpressured flap during a chord is normal and stays silent.
void observeEndpointWrite(bool mounted, bool accepted) {
    const uint32_t nowMs = millis();
    const UsbEndpointHealthState before = g_endpointHealth.snapshot(nowMs).state;
    g_endpointHealth.observeWrite(mounted, accepted, nowMs);
    const UsbEndpointHealthSnapshot after = g_endpointHealth.snapshot(nowMs);
    const bool wasStalled = before == UsbEndpointHealthState::Stalled;
    const bool isStalled = after.state == UsbEndpointHealthState::Stalled;
    if (wasStalled == isStalled) return;

    const CardputerUsbMidiTransportDiagnostics usb = g_transport.diagnostics();
    Serial.printf(
        "[USB-EDGE] %s mounted=%u suspended=%u mountUp/Down=%u/%u "
        "susp/res=%u/%u ok=%u reject=%u block=%ums up=%ums\n",
        isStalled ? "STALL" : "CLEAR",
        static_cast<unsigned>(g_transport.mounted() ? 1 : 0),
        static_cast<unsigned>(g_transport.suspended() ? 1 : 0),
        static_cast<unsigned>(usb.mountUpEvents),
        static_cast<unsigned>(usb.mountDownEvents),
        static_cast<unsigned>(usb.suspendEvents),
        static_cast<unsigned>(usb.resumeEvents),
        static_cast<unsigned>(usb.txAccepted),
        static_cast<unsigned>(usb.txRejected),
        static_cast<unsigned>(after.maximumBlockedMs),
        static_cast<unsigned>(nowMs));
}

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

// Cleanup is a recovery step, not a verdict: a completed all-notes-off means the
// wire is consistent again and playback may continue. Only cleanup that cannot
// complete within kSmfCleanupAttemptLimit reports a transport failure.
void beginSmfCleanup() {
    if (!g_smfCleanupPending) {
        g_nextSmfCleanupAttemptMs = 0;
        g_smfCleanupAttempts = 0;
    }
    g_smfCleanupPending = true;
}

void recordRecoveredSmfSendBlock() {
    if (g_smfSendBlockStartedUs == 0) return;
    const uint32_t blockedUs = micros() - g_smfSendBlockStartedUs;
    if (blockedUs > g_diagnostics.smfMaxSendBlockUs) {
        g_diagnostics.smfMaxSendBlockUs = blockedUs;
    }
    g_smfSendBlockStartedUs = 0;
}

// A host that has the interface configured but never reads the bulk IN endpoint
// (no application holding the port open, or a suspended device) blocks cleanup
// indefinitely. That is recoverable: hold SMF note ownership, publish a stall so
// the player pauses, and keep retrying the all-notes-off. Each retry doubles as
// the write probe that detects the host reading again.
void enterSmfTransportStall() {
    const uint32_t blockedUs = g_smfSendBlockStartedUs != 0
        ? micros() - g_smfSendBlockStartedUs
        : 0;
    if (g_smfTransportStalled) return;
    g_smfTransportStalled = true;
    g_smfQueue->reportTransportFailure();
    ++g_diagnostics.smfTransportAborts;
    // mounted=1 means the host has the interface configured but is not draining
    // the bulk IN endpoint (no application reading the port, or a suspended
    // device). mounted=0 is a real disconnect.
    Serial.printf(
        "[SMF-WAIT] USB MIDI endpoint not draining; playback paused "
        "mounted=%u blockedUs=%u attempts=%u\n",
        static_cast<unsigned>(g_transport.mounted() ? 1 : 0),
        static_cast<unsigned>(blockedUs),
        static_cast<unsigned>(g_smfCleanupAttempts));
}

// Recovery requires a write that actually succeeded while the interface is
// mounted. Cleanup can also complete because pollConnection() abandoned every
// SMF note on an unplug, which proves nothing about the endpoint.
void leaveSmfTransportStall() {
    if (!g_smfTransportStalled) return;
    if (!g_transport.mounted()) return;
    g_smfTransportStalled = false;
    g_smfQueue->reportTransportRecovery();
    ++g_diagnostics.smfTransportRecoveries;
    Serial.println("[SMF-WAIT] USB MIDI endpoint draining again; resuming");
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
    if (mask & ScheduledMusicalEventQueue::kDrumsMask) {
        g_patternDrumGates.clear();
        g_output.handleMusicalEvent(
            panicEvent(MusicalEventSource::PatternPlayer,
                       MusicalEventTarget::Drums));
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
        // Every SMF-owned note is released and the wire is consistent again.
        // Backpressure that recovers is not a transport failure.
        recordRecoveredSmfSendBlock();
        leaveSmfTransportStall();
        g_smfCleanupPending = false;
        g_nextSmfCleanupAttemptMs = 0;
        g_smfCleanupAttempts = 0;
    } else {
        if (g_smfCleanupAttempts < UINT8_MAX) ++g_smfCleanupAttempts;
        ++g_diagnostics.smfCleanupRetries;
        if (g_smfCleanupAttempts >= kSmfCleanupAttemptLimit) {
            // A host that does not consume the USB MIDI IN endpoint keeps every
            // NoteOff rejected for as long as it stays silent. Pause playback,
            // keep note ownership, and let the paced retry below act as the
            // probe that notices the host reading again.
            enterSmfTransportStall();
        }
        g_nextSmfCleanupAttemptMs = nowMs + (g_smfTransportStalled
            ? kSmfStallProbeDelayMs
            : kSmfCleanupRetryDelayMs);
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

void drainIncomingMidiPackets() {
    if (g_externalTransportQueue == nullptr) return;

    midiEventPacket_t packet{};
    for (std::size_t drained = 0;
         drained < kMidiRxDrainBudget && g_transport.readPacket(packet);
         ++drained) {
        ExternalMidiTransportEventType type{};
        if (!GroovePuterMidi::parseUsbMidiRealtimeTransport(
                packet.header, packet.byte1, type)) {
            ++g_diagnostics.externalRxIgnored;
            continue;
        }
        if (GroovePuterMidi::transportClockRuntime().source() !=
            GroovePuterMidi::TransportClockSource::SeqtrakExternal) {
            ++g_diagnostics.externalRxMasterIgnored;
            continue;
        }

        const uint32_t receivedAtMicros = micros();
        switch (type) {
            case ExternalMidiTransportEventType::Clock:
                ++g_externalRxPulseOrdinal;
                if (g_externalTransportQueue->tryPushClock(
                        receivedAtMicros, g_externalRxPulseOrdinal)) {
                    ++g_diagnostics.externalRxClock;
                }
                break;
            case ExternalMidiTransportEventType::Start:
                if (g_externalTransportQueue->tryPushCritical(
                        type, receivedAtMicros, g_externalRxPulseOrdinal)) {
                    ++g_diagnostics.externalRxStart;
                }
                break;
            case ExternalMidiTransportEventType::Continue:
                if (g_externalTransportQueue->tryPushCritical(
                        type, receivedAtMicros, g_externalRxPulseOrdinal)) {
                    ++g_diagnostics.externalRxContinue;
                }
                break;
            case ExternalMidiTransportEventType::Stop:
                if (g_externalTransportQueue->tryPushCritical(
                        type, receivedAtMicros, g_externalRxPulseOrdinal)) {
                    ++g_diagnostics.externalRxStop;
                }
                break;
        }
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

bool projectSmfNoteOnStillCurrent(const ScheduledSmfMidiEvent& event) {
    if (event.projectTransportEpoch == 0) return true;
    GroovePuterMidi::ProjectTransportBlockSnapshot transport{};
    return GroovePuterMidi::projectTransportTimeline().trySnapshot(transport) &&
           GroovePuterMidi::projectSmfNoteOnStillCurrent(event, transport);
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
        "drumGate=%u/%u/%u active=%u "
        "smfSent=%u smfStale=%u smfPanic=%u smfRetry=%u smfDrop=%u "
        "smfLateDrop=%u smfLateSent=%u smfMaxLateOnUs=%u smfLateOff=%u "
        "smfEpochDrop=%u smfCleanRetry=%u smfStall=%u/%u smfMaxBlockUs=%u "
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
        static_cast<unsigned>(g_diagnostics.patternDrumGates),
        static_cast<unsigned>(g_diagnostics.patternDrumGateReleases),
        static_cast<unsigned>(g_diagnostics.patternDrumGateFailures),
        static_cast<unsigned>(g_patternDrumGates.activeCount()),
        static_cast<unsigned>(g_diagnostics.smfSent),
        static_cast<unsigned>(g_diagnostics.smfStaleGenerationDrops),
        static_cast<unsigned>(g_diagnostics.smfPanics),
        static_cast<unsigned>(g_diagnostics.smfSendRetries),
        static_cast<unsigned>(g_diagnostics.smfSendDrops),
        static_cast<unsigned>(g_diagnostics.smfLateNoteOnDrops),
        static_cast<unsigned>(g_diagnostics.smfLateNoteOnSent),
        static_cast<unsigned>(g_diagnostics.smfMaxNoteOnLatenessUs),
        static_cast<unsigned>(g_diagnostics.smfLateNoteOffSent),
        static_cast<unsigned>(g_diagnostics.smfTransportEpochDrops),
        static_cast<unsigned>(g_diagnostics.smfCleanupRetries),
        static_cast<unsigned>(g_diagnostics.smfTransportAborts),
        static_cast<unsigned>(g_diagnostics.smfTransportRecoveries),
        static_cast<unsigned>(g_diagnostics.smfMaxSendBlockUs),
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

    const auto clock = GroovePuterMidi::transportClockRuntime().snapshot();
    Serial.printf(
        "[MIDI-RX] source=%s state=%s running=%u bpm=%.2f "
        "rx=%u/%u/%u/%u ignored=%u masterIgnored=%u queue=%u "
        "clockDrop=%u criticalOverflow=%u failures=%u txSuppressed=%u\n",
        GroovePuterMidi::transportClockSourceName(clock.source),
        GroovePuterMidi::externalClockLockStateName(clock.externalState),
        static_cast<unsigned>(clock.externalRunning ? 1 : 0),
        clock.externalBpm(),
        static_cast<unsigned>(g_diagnostics.externalRxClock),
        static_cast<unsigned>(g_diagnostics.externalRxStart),
        static_cast<unsigned>(g_diagnostics.externalRxContinue),
        static_cast<unsigned>(g_diagnostics.externalRxStop),
        static_cast<unsigned>(g_diagnostics.externalRxIgnored),
        static_cast<unsigned>(g_diagnostics.externalRxMasterIgnored),
        static_cast<unsigned>(g_externalTransportQueue
            ? g_externalTransportQueue->approximateSize() : 0),
        static_cast<unsigned>(g_externalTransportQueue
            ? g_externalTransportQueue->droppedClockCount() : 0),
        static_cast<unsigned>(g_externalTransportQueue
            ? g_externalTransportQueue->criticalOverflowCount() : 0),
        static_cast<unsigned>(clock.externalFailureCount),
        static_cast<unsigned>(g_diagnostics.outboundTransportSuppressed));

    const auto usb = g_transport.diagnostics();
    const auto endpoint = g_endpointHealth.snapshot(millis());
    Serial.printf(
        "[USB-DIAG] midiMounted=%u edge=up/down=%u/%u "
        "tx=attempt/ok/reject/noMount=%u/%u/%u/%u pace=%u/%ums rx=%u "
        "live=q/dispatched/drop=%u/%u/%u/%u smf=cleanup/stalled=%u/%u "
        "health=%u reject=%u stall=%u/%u block=%ums max=%ums "
        "susp=%u/%u/%u up=%ums\n",
        static_cast<unsigned>(g_transport.mounted() ? 1 : 0),
        static_cast<unsigned>(usb.mountUpEvents),
        static_cast<unsigned>(usb.mountDownEvents),
        static_cast<unsigned>(usb.txAttempts),
        static_cast<unsigned>(usb.txAccepted),
        static_cast<unsigned>(usb.txRejected),
        static_cast<unsigned>(usb.txNotMounted),
        static_cast<unsigned>(usb.txPacingWaits),
        static_cast<unsigned>(usb.txPacingWaitMicros / 1000u),
        static_cast<unsigned>(usb.rxPackets),
        static_cast<unsigned>(g_controlQueue.approximateSize()),
        static_cast<unsigned>(g_diagnostics.dispatchedControl),
        static_cast<unsigned>(g_controlQueue.droppedNoteOnCount()),
        static_cast<unsigned>(g_controlQueue.droppedCriticalCount()),
        static_cast<unsigned>(g_diagnostics.smfCleanupRetries),
        static_cast<unsigned>(g_smfTransportStalled ? 1 : 0),
        static_cast<unsigned>(endpoint.state),
        static_cast<unsigned>(endpoint.writeRejected),
        static_cast<unsigned>(endpoint.stalledTransitions),
        static_cast<unsigned>(endpoint.recoveredTransitions),
        static_cast<unsigned>(endpoint.currentBlockedMs),
        static_cast<unsigned>(endpoint.maximumBlockedMs),
        static_cast<unsigned>(g_transport.suspended() ? 1 : 0),
        static_cast<unsigned>(usb.suspendEvents),
        static_cast<unsigned>(usb.resumeEvents),
        static_cast<unsigned>(millis()));
}

enum class PendingKind : uint8_t {
    None = 0,
    Transport,
    Pattern,
    PatternDrumGate,
    Smf,
};

void midiDispatchTask(void*) {
    if (!g_output.begin()) {
        Serial.println("[MIDI-DISPATCH] USB MIDI begin failed");
    }

    ScheduledMusicalEvent pendingMusical{};
    ScheduledMusicalEvent pendingDrumGate{};
    ScheduledMidiTransportEvent pendingTransport{};
    ScheduledSmfMidiEvent pendingSmf{};
    bool hasPendingMusical = false;
    bool hasPendingTransport = false;
    bool hasPendingSmf = false;
    uint8_t smfFailedAttempts = 0;
    uint32_t lastFairnessYieldMs = millis();
    uint32_t pendingSmfLatenessUs = 0;

    auto clearPendingSmf = [&]() {
        hasPendingSmf = false;
        smfFailedAttempts = 0;
        pendingSmfLatenessUs = 0;
    };

    while (true) {
        g_output.pollConnection();
        g_transport.pollSuspendState();
        drainIncomingMidiPackets();

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

        // Drain queued GP-master realtime traffic immediately after a source
        // switch. The second check at the physical write closes the <=1.5 ms
        // deadline-wait race; this early check prevents stale lifecycle packets
        // from surviving a quick SEQ MASTER -> GP MASTER round trip.
        if (hasPendingTransport &&
            !GroovePuterMidi::transportClockSourcePublishesOutboundClock(
                GroovePuterMidi::transportClockRuntime().source())) {
            ++g_diagnostics.outboundTransportSuppressed;
            hasPendingTransport = false;
            continue;
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
            g_patternQueue->invalidateTarget(pendingMusical.event.target);
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

        bool hasPendingDrumGate =
            g_patternDrumGates.peekEarliest(pendingDrumGate);
        if (hasPendingDrumGate &&
            !scheduledMusicalEventFrameIsValid(
                pendingDrumGate, kBlockFrames)) {
            ++g_diagnostics.invalidFrameDrops;
            g_patternDrumGates.consume(pendingDrumGate.event.channel);
            if (g_patternQueue != nullptr) {
                g_patternQueue->invalidateTarget(MusicalEventTarget::Drums);
            }
            continue;
        }
        if (hasPendingDrumGate && g_patternQueue != nullptr &&
            !scheduledMusicalEventGenerationIsCurrent(
                pendingDrumGate,
                g_patternQueue->generationFor(MusicalEventTarget::Drums))) {
            ++g_diagnostics.staleGenerationDrops;
            g_patternDrumGates.consume(pendingDrumGate.event.channel);
            continue;
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

        if (hasPendingSmf && pendingSmf.projectTransportEpoch != 0) {
            GroovePuterMidi::ProjectTransportBlockSnapshot transport{};
            if (!GroovePuterMidi::projectTransportTimeline().trySnapshot(transport)) {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
                continue;
            }
            if (!transport.valid || !transport.playing ||
                !scheduledSmfMidiEventTransportEpochIsCurrent(
                    pendingSmf, transport.transportEpoch)) {
              ++g_diagnostics.smfTransportEpochDrops;
              clearPendingSmf();
              continue;
            }
        }

        if (!hasPendingTransport && !hasPendingMusical && !hasPendingDrumGate &&
            !hasPendingSmf) {
          drainControlEvents();
          drainCardputerUsbMidiTxStress(g_transport);
          logDiagnosticsIfDue();
          ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
          continue;
        }

        PendingKind kind = PendingKind::None;
        if (hasPendingTransport)
          kind = PendingKind::Transport;
        if (hasPendingMusical) {
          if (kind == PendingKind::None ||
              (kind == PendingKind::Transport &&
               !transportBeforeMusical(pendingTransport, pendingMusical))) {
            kind = PendingKind::Pattern;
          }
        }
        if (hasPendingDrumGate) {
          bool gateWins = kind == PendingKind::None;
          if (kind == PendingKind::Transport) {
            gateWins =
                !transportBeforeMusical(pendingTransport, pendingDrumGate);
          } else if (kind == PendingKind::Pattern) {
            gateWins =
                scheduledMusicalEventBefore(pendingDrumGate, pendingMusical);
          }
          if (gateWins)
            kind = PendingKind::PatternDrumGate;
        }
        if (hasPendingSmf && !g_smfCleanupPending) {
          bool smfWins = kind == PendingKind::None;
          if (kind == PendingKind::Transport) {
            smfWins = !transportBeforeSmf(pendingTransport, pendingSmf);
          } else if (kind == PendingKind::Pattern) {
            smfWins = !musicalBeforeSmf(pendingMusical, pendingSmf);
          } else if (kind == PendingKind::PatternDrumGate) {
            smfWins = !musicalBeforeSmf(pendingDrumGate, pendingSmf);
          }
          if (smfWins)
            kind = PendingKind::Smf;
        }

        if (kind == PendingKind::None) {
          drainControlEvents();
          drainCardputerUsbMidiTxStress(g_transport);
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
        case PendingKind::PatternDrumGate:
          blockSequence = pendingDrumGate.blockSequence;
          frameOffset = pendingDrumGate.frameOffset;
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
            if (kind == PendingKind::Smf) {
                const SmfLateDispatchAction action = smfLateDispatchAction(
                    pendingSmf.type,
                    lateness,
                    kSmfStaleNoteOnThresholdUs);
                if (action == SmfLateDispatchAction::DropLateNoteOn) {
                    ++g_diagnostics.smfLateNoteOnDrops;
                    clearPendingSmf();
                    continue;
                }
                pendingSmfLatenessUs = lateness;
            }
        }

        if (kind == PendingKind::Transport) {
            if (!GroovePuterMidi::transportClockSourcePublishesOutboundClock(
                    GroovePuterMidi::transportClockRuntime().source())) {
                ++g_diagnostics.outboundTransportSuppressed;
            } else if (pendingTransport.type == MidiTransportEventType::Clock &&
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
                bool dispatch = true;
                if (pendingMusical.event.source ==
                        MusicalEventSource::PatternPlayer &&
                    pendingMusical.event.target == MusicalEventTarget::Drums &&
                    pendingMusical.event.type == MusicalEventType::NoteOn) {
                    dispatch = g_patternDrumGates.scheduleOrExtend(
                        pendingMusical,
                        GroovePuterMidi::kDefaultDrumGateMs,
                        kSampleRate,
                        static_cast<uint16_t>(kBlockFrames));
                    if (dispatch) {
                        ++g_diagnostics.patternDrumGates;
                    } else {
                        ++g_diagnostics.patternDrumGateFailures;
                        g_patternQueue->invalidateTarget(
                            MusicalEventTarget::Drums);
                    }
                }
                if (dispatch) {
                    g_output.handleMusicalEvent(pendingMusical.event);
                    ++g_diagnostics.dispatchedScheduled;
                }
            } else {
                ++g_diagnostics.staleGenerationDrops;
            }
            hasPendingMusical = false;
        } else if (kind == PendingKind::PatternDrumGate) {
            if (g_patternQueue == nullptr ||
                !scheduledMusicalEventGenerationIsCurrent(
                    pendingDrumGate,
                    g_patternQueue->generationFor(
                        MusicalEventTarget::Drums))) {
                ++g_diagnostics.staleGenerationDrops;
                g_patternDrumGates.consume(pendingDrumGate.event.channel);
            } else {
                const uint8_t releases = g_patternDrumGates.releaseCount(
                    pendingDrumGate.event.channel);
                for (uint8_t i = 0; i < releases; ++i) {
                    g_output.handleMusicalEvent(pendingDrumGate.event);
                    ++g_diagnostics.patternDrumGateReleases;
                }
                g_patternDrumGates.consume(pendingDrumGate.event.channel);
            }
        } else {
            if (g_smfQueue == nullptr ||
                !scheduledSmfMidiEventGenerationIsCurrent(
                    pendingSmf, g_smfQueue->generation())) {
                ++g_diagnostics.smfStaleGenerationDrops;
                clearPendingSmf();
            } else if (pendingSmf.type == ScheduledSmfMidiEventType::NoteOn &&
                       !projectSmfNoteOnStillCurrent(pendingSmf)) {
                // Stop/Restart can happen during the final <=1.5 ms busy-wait.
                // Recheck the live transport immediately before the USB write.
                ++g_diagnostics.smfTransportEpochDrops;
                clearPendingSmf();
            } else if (dispatchSmfEvent(pendingSmf)) {
                ++g_diagnostics.smfSent;
                recordRecoveredSmfSendBlock();
                if (pendingSmfLatenessUs > 0) {
                    if (pendingSmf.type == ScheduledSmfMidiEventType::NoteOn) {
                        ++g_diagnostics.smfLateNoteOnSent;
                        if (pendingSmfLatenessUs >
                            g_diagnostics.smfMaxNoteOnLatenessUs) {
                            g_diagnostics.smfMaxNoteOnLatenessUs =
                                pendingSmfLatenessUs;
                        }
                    } else {
                        ++g_diagnostics.smfLateNoteOffSent;
                    }
                }
                clearPendingSmf();
            } else {
                ++g_diagnostics.smfSendRetries;
                if (smfFailedAttempts < UINT8_MAX) ++smfFailedAttempts;
                if (g_smfSendBlockStartedUs == 0) {
                    g_smfSendBlockStartedUs = micros();
                }
                const SmfSendFailureAction action = smfSendFailureAction(
                    pendingSmf, smfFailedAttempts);
                if (action == SmfSendFailureAction::DropNoteOn) {
                    // The write never reached the wire, so nothing is owned and
                    // there is nothing to clean up. Shedding the NoteOn is the
                    // whole point of this action; escalating to a transport
                    // failure here stopped playback on any burst that briefly
                    // filled the TX FIFO.
                    ++g_diagnostics.smfSendDrops;
                    clearPendingSmf();
                } else if (action == SmfSendFailureAction::BeginCleanup) {
                    // UsbMidiOutput retains ownership after a failed NoteOff.
                    // Move recovery to the paced all-notes-off path.
                    beginSmfCleanup();
                    clearPendingSmf();
                }
                // A pending task notification must not turn USB backpressure
                // into a zero-delay retry loop that starves UI and TinyUSB.
                vTaskDelay(kSmfRetryDelay);
            }
        }

        drainControlEvents(2);
        logDiagnosticsIfDue();

        // Every branch that waits for a deadline blocks, but the branch that
        // dispatches a due event does not. A backlog of already-due events -
        // a saturated queue, or a resume anchored in the past - therefore keeps
        // this loop runnable indefinitely and starves the CPU0 idle task until
        // the task watchdog fires. Yield on a wall-clock budget so the fast
        // path stays fast while fairness is guaranteed.
        const uint32_t nowMs = millis();
        if (nowMs - lastFairnessYieldMs >= kDispatchFairnessYieldMs) {
            lastFairnessYieldMs = nowMs;
            ++g_diagnostics.dispatchFairnessYields;
            vTaskDelay(1);
        }
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
    diagnostics_ = {};
    mountStateKnown_ = false;
    lastMounted_ = false;
    suspendStateKnown_ = false;
    lastSuspended_ = false;
    txPacer_.reset();

    // Arduino app_main() starts TinyUSB only when an on-boot USB interface such
    // as CDC is enabled. The MIDI-only SEQTRAK profile has no such interface,
    // and USBMIDI::begin() is a no-op in the pinned core, so start the already
    // registered MIDI descriptor explicitly in that configuration.
#if !ARDUINO_USB_CDC_ON_BOOT
    if (!USB.begin()) {
        begun_ = false;
        return false;
    }
#endif

    midi_.begin();
    begun_ = true;
    return true;
}

bool CardputerUsbMidiTransport::mounted() const {
    // ESPUSB reports the whole composite as mounted once CDC is configured.
    // MIDI can still be unavailable while its interface is being claimed, so
    // use the class-specific TinyUSB state before accepting output packets.
    const bool nextMounted = begun_ && tud_midi_mounted();
    observeMountState(nextMounted);
    return nextMounted;
}

bool CardputerUsbMidiTransport::suspended() const {
    return begun_ && tud_suspended();
}

void CardputerUsbMidiTransport::pollSuspendState() const {
    if (!begun_) return;
    const bool nowSuspended = tud_suspended();
    if (!suspendStateKnown_) {
        suspendStateKnown_ = true;
        lastSuspended_ = nowSuspended;
        return;
    }
    if (nowSuspended == lastSuspended_) return;
    lastSuspended_ = nowSuspended;
    if (nowSuspended) {
        ++diagnostics_.suspendEvents;
    } else {
        ++diagnostics_.resumeEvents;
    }
}

bool CardputerUsbMidiTransport::readPacket(midiEventPacket_t& packet) {
    if (!begun_) return false;
    const bool received = midi_.readPacket(&packet);
    if (received) ++diagnostics_.rxPackets;
    return received;
}

void CardputerUsbMidiTransport::observeMountState(bool mounted) const {
    if (!mountStateKnown_) {
        mountStateKnown_ = true;
        lastMounted_ = mounted;
        if (mounted) ++diagnostics_.mountUpEvents;
        return;
    }
    if (mounted == lastMounted_) return;
    lastMounted_ = mounted;
    if (mounted) {
        ++diagnostics_.mountUpEvents;
    } else {
        ++diagnostics_.mountDownEvents;
    }
}

bool CardputerUsbMidiTransport::writePacket(midiEventPacket_t& packet) {
    ++diagnostics_.txAttempts;
    if (!mounted()) {
        ++diagnostics_.txNotMounted;
        txPacer_.reset();
        observeEndpointWrite(false, false);
        return false;
    }

    const uint32_t pacingWaitUs = txPacer_.waitMicros(micros());
    if (pacingWaitUs > 0) {
        ++diagnostics_.txPacingWaits;
        diagnostics_.txPacingWaitMicros += pacingWaitUs;
        esp_rom_delay_us(pacingWaitUs);
    }
    txPacer_.recordAttempt(micros());

    if (!midi_.writePacket(&packet)) {
        ++diagnostics_.txRejected;
        observeEndpointWrite(true, false);
        return false;
    }
    ++diagnostics_.txAccepted;
    observeEndpointWrite(true, true);
    return true;
}

bool CardputerUsbMidiTransport::writeChannelPacket(uint8_t codeIndex,
                                                    uint8_t statusBase,
                                                    uint8_t zeroBasedChannel,
                                                    uint8_t note,
                                                    uint8_t velocity) {
    midiEventPacket_t packet{
        codeIndex,
        static_cast<uint8_t>(statusBase | clampChannel(zeroBasedChannel)),
        clamp7Bit(note),
        clamp7Bit(velocity),
    };
    return writePacket(packet);
}

bool CardputerUsbMidiTransport::writeRealtimePacket(uint8_t status) {
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
    return writePacket(packet);
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

bool CardputerUsbMidiTransport::sendControlChange(uint8_t zeroBasedChannel,
                                                  uint8_t controller,
                                                  uint8_t value) {
    return writeChannelPacket(kCinControlChange,
                              kStatusControlChange,
                              zeroBasedChannel,
                              controller,
                              value);
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
    MusicalEventRouter &router, MusicalEventQueue &patternQueue,
    ExternalMidiTransportEventQueue &externalTransportQueue) {
  if (g_registered)
    return true;

  g_patternQueue = &patternQueue;
  g_externalTransportQueue = &externalTransportQueue;
  g_patternDrumGates.clear();
  if (!beginCardputerUsbMidiTxStress(notifyDispatcher)) {
    Serial.println("[USB-MIDI] TX stress diagnostics unavailable");
  }
  if (!router.addSink(g_queueSink)) {
    g_patternQueue = nullptr;
    g_externalTransportQueue = nullptr;
    return false;
  }

  const BaseType_t taskResult =
      xTaskCreatePinnedToCore(midiDispatchTask, "MidiDispatchTask", 4096,
                              nullptr, 2, &g_dispatchTaskHandle, 0);
  if (taskResult != pdPASS) {
    router.removeSink(g_queueSink);
    g_patternQueue = nullptr;
    g_externalTransportQueue = nullptr;
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
