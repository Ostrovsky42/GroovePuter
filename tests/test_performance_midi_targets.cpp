#include <cassert>
#include <cstring>
#include <vector>

#include "src/input/performance_keyboard.h"

namespace {
class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        events.push_back(event);
    }

    std::vector<MusicalEvent> events;
};

void expectEvent(const MusicalEvent& event,
                 MusicalEventType type,
                 MusicalEventTarget target,
                 uint8_t note) {
    assert(event.type == type);
    assert(event.source == MusicalEventSource::PerformanceKeyboard);
    assert(event.target == target);
    assert(event.note == note);
}
}  // namespace

int main() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));

    PerformanceKeyboard keyboard(router);
    assert(keyboard.target() == MusicalEventTarget::SynthA);
    assert(std::strcmp(keyboard.targetName(), "SYNTH A") == 0);
    assert(keyboard.targetMidiChannel() == 8);

    assert(keyboard.keyDown('a', 100));
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 36);

    keyboard.setTarget(MusicalEventTarget::SynthB);
    assert(sink.events.size() == 2);
    expectEvent(sink.events[1], MusicalEventType::AllNotesOff,
                MusicalEventTarget::SynthA, 0);
    assert(keyboard.heldCount() == 0);
    assert(keyboard.target() == MusicalEventTarget::SynthB);
    assert(std::strcmp(keyboard.targetName(), "SYNTH B") == 0);
    assert(keyboard.targetMidiChannel() == 9);

    assert(keyboard.keyDown('s', 101));
    assert(sink.events.size() == 3);
    expectEvent(sink.events[2], MusicalEventType::NoteOn,
                MusicalEventTarget::SynthB, 38);

    keyboard.cycleTarget(1);
    assert(sink.events.size() == 4);
    expectEvent(sink.events[3], MusicalEventType::AllNotesOff,
                MusicalEventTarget::SynthB, 0);
    assert(keyboard.target() == MusicalEventTarget::Drums);
    assert(std::strcmp(keyboard.targetName(), "DRUMS") == 0);
    assert(keyboard.targetMidiChannel() == 10);

    assert(keyboard.keyDown('d', 102));
    assert(sink.events.size() == 5);
    expectEvent(sink.events[4], MusicalEventType::NoteOn,
                MusicalEventTarget::Drums, 39);

    keyboard.cycleTarget(1);
    assert(sink.events.size() == 6);
    expectEvent(sink.events[5], MusicalEventType::AllNotesOff,
                MusicalEventTarget::Drums, 0);
    assert(keyboard.target() == MusicalEventTarget::SynthA);

    keyboard.cycleTarget(-1);
    assert(sink.events.size() == 7);
    expectEvent(sink.events[6], MusicalEventType::AllNotesOff,
                MusicalEventTarget::SynthA, 0);
    assert(keyboard.target() == MusicalEventTarget::Drums);

    return 0;
}
