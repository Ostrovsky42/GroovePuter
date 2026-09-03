#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "src/input/performance_keyboard.h"
#include "src/midi/project_transport_timeline.h"

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
        // Re-rooting an active memory voicing first releases the old generated
        // notes, then starts the new memory voicing. Cleanup must precede attack.
        assert(f.sink.events.size() == 6);
        assert(count(f.sink.events, MusicalEventType::NoteOff) == 3);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 3);
        for (std::size_t i = 0; i < 3; ++i) {
            assert(f.sink.events[i].type == MusicalEventType::NoteOff);
            assert(f.sink.events[i].source == MusicalEventSource::Arpeggiator);
        }
        assert(f.sink.events[3].type == MusicalEventType::NoteOn);
        assert(f.sink.events[3].note == 38);
        assert(f.sink.events[4].type == MusicalEventType::NoteOn);
        assert(f.sink.events[4].note == 41);
        assert(f.sink.events[5].type == MusicalEventType::NoteOn);
        assert(f.sink.events[5].note == 45);
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
        assert(f.keyboard.euclideanPulses() == 0);
        f.keyboard.cycleEuclideanPulses(1);  // 1 of 16
        assert(f.keyboard.ratchetCount() == 2);
        assert(f.keyboard.euclideanPulses() == 1);
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

    {
        Fixture f;
        auto& timeline = GroovePuterMidi::projectTransportTimeline();
        timeline.resetPublisher();
        timeline.publishBlock(10u, 512u, 3.25f, 120.0f, 22050.0f, true);

        f.keyboard.setArpeggiatorEnabled(true);
        f.keyboard.setTransportPlaying(true);
        f.sink.clear();
        f.keyboard.service(4000000u);
        assert(f.keyboard.keyDown('a', 100));
        f.keyboard.service(4000000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 0);

        // Project phase, not wall-clock time, authorizes preparation. Move the
        // authoritative timeline into the half-pulse lead window for step 4.
        timeline.publishBlock(11u, 512u, 3.55f, 120.0f, 22050.0f, true);
        f.keyboard.service(4023219u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 0);
        assert(f.keyboard.scheduledDepth() > 0u);

        // Delaying service beyond the 12 ms NoteOn window drops the stale hit
        // instead of producing a wall-clock catch-up burst.
        f.keyboard.service(4100000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 0);
        assert(f.keyboard.staleGeneratedNoteOnDrops() >= 1u);

        assert(f.keyboard.keyUp('a'));
        f.keyboard.setTransportPlaying(false);
        timeline.publishBlock(12u, 512u, 3.75f, 120.0f, 22050.0f, false);
    }

    {
        // Maximum-density chord memory previously required all 64 available
        // scheduler slots for one x4-ratchet step. Any overlap then triggered
        // stopGeneratedOutput(). The expanded bounded queue must start all
        // eight notes at the predicted boundary without a panic/silence gap.
        Fixture f;
        f.keyboard.setScale(PerformanceScale::Chromatic);
        for (char key : std::string("asdfghjk")) {
            assert(f.keyboard.keyDown(key, 100));
        }
        assert(f.keyboard.captureChordMemory());
        assert(f.keyboard.chordMemorySize() == 8);
        f.keyboard.cycleRatchet(1);
        f.keyboard.cycleRatchet(1);
        f.keyboard.cycleRatchet(1);
        assert(f.keyboard.ratchetCount() == 4);

        auto& timeline = GroovePuterMidi::projectTransportTimeline();
        timeline.resetPublisher();
        timeline.publishBlock(30u, 512u, 3.25f, 120.0f, 22050.0f, true);
        f.keyboard.setTransportPlaying(true);
        f.sink.clear();
        f.keyboard.service(5000000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 0);

        timeline.publishBlock(31u, 512u, 3.55f, 120.0f, 22050.0f, true);
        f.keyboard.service(5023219u);
        assert(f.keyboard.scheduledDepth() > 0u);
        f.keyboard.service(5080000u);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 8);
        assert(count(f.sink.events, MusicalEventType::AllNotesOff) == 0);
    }

    return 0;
}