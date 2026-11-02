// CALL/RESPONSE phase 0: the PERFORM input tap sees what the person pressed
// (built-in keys and external MIDI) and never changes what PERFORM outputs.
#include <cassert>
#include <cstdio>
#include <vector>

#include "src/input/performance_keyboard.h"

namespace {

class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override { events.push_back(event); }
    std::vector<MusicalEvent> events;
};

struct TapCall {
    bool on;
    uint16_t tag;
    uint8_t note;
    uint8_t velocity;
};

class RecordingTap final : public PerformanceInputObserver {
public:
    void onPerformanceInputNote(bool on, uint16_t tag, uint8_t note, uint8_t velocity) override {
        calls.push_back(TapCall{on, tag, note, velocity});
    }
    std::vector<TapCall> calls;
};

void script(PerformanceKeyboard& kb) {
    kb.service(0);
    kb.keyDown('a', 100);
    kb.keyDown('d', 90);
    kb.keyUp('a');
    kb.keyUp('d');
    const PerformanceInputIdentity input{7, 1, 2, 64, 0};
    kb.externalNoteOn(64, 80, input);
    kb.externalNoteOff(64, input);
    kb.setChordMode(PerformanceChordMode::Major);
    kb.keyDown('f', 110);
    kb.keyUp('f');
    kb.setChordMode(PerformanceChordMode::Off);
    kb.setPerformanceRole(GroovePuterPerformance::PerformanceRole::Lead);
    kb.setLeadMode(GroovePuterPerformance::LeadMode::Scale);
    kb.externalNoteOn(61, 70);
    kb.externalNoteOff(61);
    kb.setPerformanceRole(GroovePuterPerformance::PerformanceRole::Raw);
    kb.setTarget(MusicalEventTarget::Drums);
    kb.keyDown('a', 100);
    kb.keyUp('a');
    kb.externalNoteOn(36, 100);
    kb.externalNoteOff(36);
}

bool sameEvents(const std::vector<MusicalEvent>& a, const std::vector<MusicalEvent>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].type != b[i].type || a[i].target != b[i].target ||
            a[i].channel != b[i].channel || a[i].note != b[i].note ||
            a[i].velocity != b[i].velocity || a[i].flags != b[i].flags) return false;
    }
    return true;
}

}  // namespace

int main() {
    MusicalEventRouter plainRouter;
    RecordingSink plainSink;
    assert(plainRouter.addSink(plainSink));
    PerformanceKeyboard plain(plainRouter);
    script(plain);

    MusicalEventRouter tappedRouter;
    RecordingSink tappedSink;
    assert(tappedRouter.addSink(tappedSink));
    PerformanceKeyboard tapped(tappedRouter);
    RecordingTap tap;
    tapped.setInputObserver(&tap);
    script(tapped);

    // Output is identical with and without the tap.
    assert(!plainSink.events.empty());
    assert(sameEvents(plainSink.events, tappedSink.events));

    // Built-in keys: tag = key, note = the key's input pitch (before Smart Play).
    uint8_t aNote = 0, dNote = 0, fNote = 0;
    {
        PerformanceKeyboard probe(plainRouter);
        assert(probe.noteForKey('a', aNote) && probe.noteForKey('d', dNote) &&
               probe.noteForKey('f', fNote));
    }
    const uint16_t extTag = performanceExternalInputTag(1, 2, 64);
    const uint16_t legacyTag = performanceExternalInputTag(0, 0, 61);
    const TapCall expected[] = {
        {true, 'a', aNote, 100}, {true, 'd', dNote, 90},
        {false, 'a', aNote, 0}, {false, 'd', dNote, 0},
        {true, extTag, 64, 80}, {false, extTag, 64, 0},
        {true, 'f', fNote, 110}, {false, 'f', fNote, 0},
        // Lead/Scale may move 61 to a scale tone; the tap still reports 61.
        {true, legacyTag, 61, 70}, {false, legacyTag, 61, 0},
        // Drums target: no melodic input is reported.
    };
    const std::size_t expectedCount = sizeof(expected) / sizeof(expected[0]);
    if (tap.calls.size() != expectedCount) {
        std::fprintf(stderr, "tap calls %zu != %zu\n", tap.calls.size(), expectedCount);
        return 1;
    }
    for (std::size_t i = 0; i < expectedCount; ++i) {
        assert(tap.calls[i].on == expected[i].on);
        assert(tap.calls[i].tag == expected[i].tag);
        if (expected[i].on) {
            assert(tap.calls[i].note == expected[i].note);
            assert(tap.calls[i].velocity == expected[i].velocity);
        }
    }

    const int8_t* intervals = nullptr;
    uint8_t count = 0;
    assert(tapped.scaleIntervals(intervals, count) && intervals && count == 7);

    std::puts("cr keyboard tap tests passed");
    return 0;
}
