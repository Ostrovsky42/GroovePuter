#include <cassert>
#include <cstdint>

#include "src/midi/smf_midi_visual.h"

using namespace GroovePuterMidi;

int main() {
    SmfMidiVisualTimeline timeline;
    timeline.reset();
    const uint32_t epoch = timeline.snapshot().epoch;

    timeline.queue(10, 48, 40, 0);
    timeline.queue(12, 72, 110, 8);
    assert(timeline.pending() == 2);

    SmfMidiVisualSnapshot snapshot = timeline.advanceTo(9);
    assert(snapshot.pulseCounter == 0);
    assert(timeline.pending() == 2);

    snapshot = timeline.advanceTo(12);
    assert(snapshot.pulseCounter == 2);
    assert(snapshot.note == 72);
    assert(snapshot.velocity == 110);
    assert(snapshot.channel == 8);
    assert(timeline.pending() == 0);

    for (std::size_t i = 0; i < SmfMidiVisualTimeline::kCapacity + 3; ++i) {
        timeline.queue(static_cast<uint32_t>(100 + i),
                       static_cast<uint8_t>(36 + (i % 48)),
                       static_cast<uint8_t>(60 + (i % 60)),
                       static_cast<uint8_t>(i % 16));
    }
    assert(timeline.pending() == SmfMidiVisualTimeline::kCapacity);
    assert(timeline.snapshot().droppedEvents == 3);

    timeline.clearPending();
    assert(timeline.pending() == 0);
    assert(timeline.snapshot().velocity == 0);

    timeline.reset();
    assert(timeline.snapshot().epoch == epoch + 1);
    assert(timeline.snapshot().pulseCounter == 0);
    assert(timeline.snapshot().droppedEvents == 0);
    return 0;
}
