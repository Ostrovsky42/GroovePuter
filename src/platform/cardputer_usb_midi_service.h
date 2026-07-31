#pragma once

#include <cstdint>

class MusicalEventRouter;
class MusicalEventQueue;
class ScheduledSmfMidiEventQueue;

// Registers a bounded live-event sink and starts the sole Cardputer USB-MIDI
// owner task. Pattern and transport queues remain owned by AudioTask (producer)
// and MidiDispatchTask (consumer).
bool registerCardputerUsbMidiSink(
    MusicalEventRouter& router,
    MusicalEventQueue& patternQueue);

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
