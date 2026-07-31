#pragma once

#include <cstdint>

class MusicalEventRouter;
class ScheduledMusicalEventQueue;

// Registers a bounded live-event sink and starts the sole Cardputer USB-MIDI
// owner task. The PatternPlayer queue remains owned by AudioTask (producer) and
// MidiDispatchTask (consumer).
bool registerCardputerUsbMidiSink(
    MusicalEventRouter& router,
    ScheduledMusicalEventQueue& patternQueue);

// Publishes the predicted playback start for one generated audio block. The
// dispatcher combines this anchor with ScheduledMusicalEvent::frameOffset.
void publishCardputerUsbMidiBlockAnchor(uint32_t blockSequence,
                                        uint32_t playbackStartMicros);
