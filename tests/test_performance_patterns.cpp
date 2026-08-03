#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "src/input/performance_keyboard.h"

namespace {
class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        events.push_back(event);
    }
    void clear() { events.clear(); }
    std::vector<MusicalEvent> events;
};

struct Fixture {
    MusicalEventRouter router;
    RecordingSink sink;
    PerformanceKeyboard keyboard;
    Fixture() : keyboard(router) { assert(router.addSink(sink)); }
};

int count(const std::vector<MusicalEvent>& events, MusicalEventType type) {
    int result = 0;
    for (const MusicalEvent& event : events) {
        if (event.type == type) ++result;
    }
    return result;
}
}  // namespace

int main() {
    {
        Fixture f;
        f.keyboard.setChordMode(PerformanceChordMode::Major);
        f.sink.clear();
        assert(f.keyboard.keyDown('a', 100));
        assert(f.sink.events.size() == 3);
        assert(f.sink.events[0].note == 36);
        assert(f.sink.events[1].note == 40);
        assert(f.sink.events[2].note == 43);
        for (const auto& event : f.sink.events) {
            assert(event.source == MusicalEventSource::Arpeggiator);
        }
        assert(f.keyboard.keyUp('a'));
        assert(count(f.sink.events, MusicalEventType::NoteOff) == 3);
    }

    {
        Fixture f;
        assert(f.keyboard.keyDown('a'));
        assert(f.keyboard.keyDown('d'));
        assert(f.keyboard.keyDown('g'));
        assert(f.keyboard.captureChordMemory());
        assert(f.keyboard.chordMemorySize() == 3);
        assert(f.keyboard.chordMode() == PerformanceChordMode::Memory);
        f.sink.clear();
        assert(f.keyboard.keyDown('s', 90));
        assert(f.sink.events.size() == 3);
        assert(f.sink.events[0].note == 38);
        assert(f.sink.events[1].note == 41);
        assert(f.sink.events[2].note == 45);
    }

    {
        Fixture f;
        f.keyboard.setArpeggiatorEnabled(true);
        f.keyboard.setTempoBpm(120.0f); // 125 ms sixteenth
        f.sink.clear();
        f.keyboard.service(1000000u);
        assert(f.keyboard.keyDown('a', 100));
        f.keyboard.service(1000000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 1);
        f.keyboard.service(1075000u);
        assert(count(f.sink.events, MusicalEventType::NoteOff) == 1);
        f.keyboard.service(1125000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 2);
    }

    {
        Fixture f;
        f.keyboard.cycleRatchet(1);          // 2 hits per sixteenth
        f.keyboard.cycleEuclideanPulses(1);  // 3 of 16
        assert(f.keyboard.ratchetCount() == 2);
        assert(f.keyboard.euclideanPulses() == 3);
        f.sink.clear();
        f.keyboard.service(2000000u);
        assert(f.keyboard.keyDown('a', 100));
        f.keyboard.service(2000000u);
        f.keyboard.service(2070000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 2);
        assert(count(f.sink.events, MusicalEventType::NoteOff) >= 1);
        f.keyboard.service(2130000u);
        assert(count(f.sink.events, MusicalEventType::NoteOff) == 2);
    }

    {
        Fixture f;
        f.keyboard.setChordMode(PerformanceChordMode::Major);
        f.keyboard.cycleStrum(1);  // 8 ms
        f.sink.clear();
        f.keyboard.service(3000000u);
        assert(f.keyboard.keyDown('a', 100));
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 1);
        f.keyboard.service(3009000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 2);
        // Releasing before the third delayed note cancels the queue and emits
        // scoped NoteOff for the two notes that actually started.
        assert(f.keyboard.keyUp('a'));
        f.keyboard.service(3020000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 2);
        assert(count(f.sink.events, MusicalEventType::NoteOff) == 2);
    }

    {
        Fixture f;
        f.keyboard.setTarget(MusicalEventTarget::Drums);
        f.keyboard.setChordMode(PerformanceChordMode::Major);
        f.keyboard.setArpeggiatorEnabled(true);
        f.sink.clear();
        assert(f.keyboard.keyDown('a', 99));
        assert(f.sink.events.size() == 1);
        assert(f.sink.events[0].source == MusicalEventSource::PerformanceKeyboard);
        assert(f.sink.events[0].target == MusicalEventTarget::Drums);
        assert(f.sink.events[0].channel == 0);
        assert(f.sink.events[0].note == PerformanceKeyboard::kSeqtrakDrumNote);
    }

    return 0;
}
