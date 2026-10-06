#pragma once

#include <cstddef>

#include <cstdint>

#include "src/midi/external_note_queue.h"
#include "src/midi/latency_histogram.h"
#include "src/midi/midi_input_dispatcher.h"

class MusicalEventRouter;
class MusicalEventQueue;
class ScheduledSmfMidiEventQueue;
class ExternalMidiTransportEventQueue;

// Read-only endpoint state for the Cardputer UI. The dispatcher remains the
// sole USB owner; this snapshot exists so MIDI-only firmware can be diagnosed
// without a CDC serial console.
struct CardputerUsbMidiStatusSnapshot {
    bool registered{false};
    bool started{false};
    bool cdcOnBoot{false};
    bool mounted{false};
    bool suspended{false};
    bool stalled{false};
    uint32_t txAccepted{0};
    uint32_t txRejected{0};
    uint32_t txRejectedEndpointBusy{0};
    uint32_t txRejectedEndpointStalled{0};
    uint16_t queuedSmfEvents{0};
};

// Registers a bounded live-event sink and starts the sole Cardputer USB-MIDI
// owner task. Pattern and transport queues remain owned by AudioTask (producer)
// and MidiDispatchTask (consumer).
bool registerCardputerUsbMidiSink(
    MusicalEventRouter& router,
    MusicalEventQueue& patternQueue,
    ExternalMidiTransportEventQueue& externalTransportQueue);

// Registers the separate SPSC queue produced by SmfPlayerTask. The queue does
// not write USB itself; MidiDispatchTask remains the only consumer/USB owner.
void registerCardputerSmfMidiQueue(ScheduledSmfMidiEventQueue* queue);

GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRuntimeRoutingConfig();
// External keyboard notes for the PERFORM target. Producer: MidiDispatchTask. Consumer: the loop
// task, which owns the PERFORM keyboard.
GroovePuterMidi::ExternalNoteQueue& cardputerExternalNoteQueue();

// Acceptance diagnostics: USB-callback -> dispatch-task latency (needs GROOVEPUTER_USB_ACCEPT_DIAG)
// and the dispatch task's minimum free stack since start.
const GroovePuterMidi::LatencyHistogram& cardputerUsbRingLatency();
uint32_t cardputerUsbDispatchStackFreeBytes();
// Last non-note Host packets as "CTL n=<count> <status.d1.d2> ..." (needs GROOVEPUTER_USB_ACCEPT_DIAG).
void cardputerUsbLastRawText(char* out, size_t size);
// Inbound clock path counters as "RX pk=<usb packets> f8=<queued clocks> ign=<realtime
// dropped because CLOCK is INTERNAL> fa=<starts> fc=<stops>" (needs GROOVEPUTER_USB_ACCEPT_DIAG).
void cardputerUsbClockRxText(char* out, size_t size);
// Shape of the last controller / pitch-bend burst: count, min..max, last value.
void cardputerUsbRampText(char* ccOut, size_t ccSize, char* pbOut, size_t pbSize);

bool applyCardputerMidiInputRuntimeRoutingConfig(
    const GroovePuterMidi::MidiInputRoutingConfig& config);

// Publishes the predicted playback start for one generated audio block. The
// dispatcher combines this anchor with scheduled frame offsets for Pattern,
// transport and SMF events.
void publishCardputerUsbMidiBlockAnchor(uint32_t blockSequence,
                                        uint32_t playbackStartMicros);

// Safe snapshot used by the SMF producer to schedule events several audio
// blocks ahead. This is an anchor, not an independent wall-clock scheduler.
bool snapshotCardputerUsbMidiBlockAnchor(uint32_t& blockSequence,
                                         uint32_t& playbackStartMicros);

#if defined(ARDUINO)
CardputerUsbMidiStatusSnapshot snapshotCardputerUsbMidiStatus();
#else
inline CardputerUsbMidiStatusSnapshot snapshotCardputerUsbMidiStatus() {
    return {};
}
#endif
