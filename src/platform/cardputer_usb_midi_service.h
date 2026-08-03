#pragma once

#include <cstdint>

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
