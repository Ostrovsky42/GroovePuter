// Pulse notes must survive long UI frames. loop() can spend ~30 ms in drawUI
// between service() calls (drawUI every 40 ms plus delay(5)); a pulse that is
// served that late must still sound, and only a pulse later than
// kGeneratedNoteOnStaleMicros (60 ms) may be shed -- and then it is counted.
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "src/input/performance_keyboard.h"
#include "src/midi/project_transport_timeline.h"

namespace {
class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        events.push_back(event);
    }
    std::vector<MusicalEvent> events;
};

struct Fixture {
    MusicalEventRouter router;
    RecordingSink sink;
    PerformanceKeyboard keyboard;
    Fixture() : keyboard(router) { assert(router.addSink(sink)); }
};

int noteOns(const std::vector<MusicalEvent>& events) {
    int result = 0;
    for (const MusicalEvent& event : events) {
        if (event.type == MusicalEventType::NoteOn) ++result;
    }
    return result;
}

void setEuclid16of16(PerformanceKeyboard& keyboard) {
    keyboard.setEuclideanLength(16);
    while (keyboard.euclideanPulses() != 16) keyboard.cycleEuclideanPulses(1);
    assert(keyboard.euclideanLength() == 16 && keyboard.euclideanPulses() == 16);
}

// 120 BPM, Sixteenth rate: one pulse every 125 ms. service() runs every 40 ms
// (a long UI frame), so a pulse is served up to 39 ms after its boundary.
// Keys are pressed after the first service() so their events carry a current
// timestamp. Returns the number of NoteOn events for 32 pulses (4 s).
int runLongFrames(Fixture& f, const char* keys) {
    f.keyboard.setTempoBpm(120.0f);
    uint32_t now = 1000000u;
    f.keyboard.service(now);
    for (const char* key = keys; *key != '\0'; ++key) {
        assert(f.keyboard.keyDown(*key, 100));
    }
    f.sink.events.clear();
    f.keyboard.service(now);
    for (int frame = 0; frame < 100; ++frame) {  // 100 * 40 ms = 4 s = 32 pulses
        now += 40000u;
        f.keyboard.service(now);
    }
    return noteOns(f.sink.events);
}
}  // namespace

int main() {
    {
        // EUCLID 16/16, STOP, one key: every one of 32 pulses sounds.
        Fixture f;
        setEuclid16of16(f.keyboard);
        const int ons = runLongFrames(f, "a");
        assert(ons == 32);
        assert(f.keyboard.staleGeneratedNoteOnDrops() == 0u);
    }

    {
        // EUCLID 16/16 over a three-key ARP UP: one NoteOn per pulse.
        Fixture f;
        setEuclid16of16(f.keyboard);
        f.keyboard.setArpeggiatorEnabled(true);
        const int ons = runLongFrames(f, "adg");
        assert(ons == 32);
        assert(f.keyboard.staleGeneratedNoteOnDrops() == 0u);
    }

    // TRANSPORT_COUNTER_BEGIN
    {
        // A transport pulse served later than 60 ms is shed and counted.
        Fixture f;
        auto& timeline = GroovePuterMidi::projectTransportTimeline();
        timeline.resetPublisher();
        timeline.publishBlock(10u, 512u, 3.25f, 120.0f, 22050.0f, true);
        f.keyboard.setArpeggiatorEnabled(true);
        f.keyboard.setTransportPlaying(true);
        f.keyboard.service(4000000u);
        assert(f.keyboard.keyDown('a', 100));
        f.keyboard.service(4000000u);  // anchors the block clock
        assert(f.keyboard.staleTransportPulseDrops() == 0u);
        // Same block, now inside the lead window: boundary is 56.25 ms after
        // the anchor. Serve it 100 ms after the anchor -> 43.75 ms late: kept.
        timeline.publishBlock(10u, 512u, 3.55f, 120.0f, 22050.0f, true);
        f.keyboard.service(4100000u);
        assert(f.keyboard.staleTransportPulseDrops() == 0u);
    }

    {
        Fixture f;
        auto& timeline = GroovePuterMidi::projectTransportTimeline();
        timeline.resetPublisher();
        timeline.publishBlock(10u, 512u, 3.25f, 120.0f, 22050.0f, true);
        f.keyboard.setArpeggiatorEnabled(true);
        f.keyboard.setTransportPlaying(true);
        f.keyboard.service(4000000u);
        assert(f.keyboard.keyDown('a', 100));
        f.keyboard.service(4000000u);
        timeline.publishBlock(10u, 512u, 3.55f, 120.0f, 22050.0f, true);
        // 200 ms after the anchor -> 143.75 ms late (> 60 ms): shed + counted.
        f.sink.events.clear();
        f.keyboard.service(4200000u);
        assert(f.keyboard.staleTransportPulseDrops() == 1u);
        assert(noteOns(f.sink.events) == 0);
        f.keyboard.resetPerformanceDiagnostics();
        assert(f.keyboard.staleTransportPulseDrops() == 0u);
    }
    // TRANSPORT_COUNTER_END

    return 0;
}
