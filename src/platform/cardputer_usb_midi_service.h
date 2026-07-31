#pragma once

#include <cstdint>

class MusicalEventRouter;
class MusicalEventQueue;

// Registers a bounded live-event sink and starts the sole Cardputer USB-MIDI
// owner task. Pattern and transport queues remain owned by AudioTask (producer)
// and MidiDispatchTask (consumer).
bool registerCardputerUsbMidiSink(
    MusicalEventRouter& router,
    MusicalEventQueue& patternQueue);

// Publishes the predicted playback start for one generated audio block. The
// dispatcher combines this anchor with scheduled frame offsets for both Pattern
// MIDI and MIDI transport events.
void publishCardputerUsbMidiBlockAnchor(uint32_t blockSequence,
                                        uint32_t playbackStartMicros);
