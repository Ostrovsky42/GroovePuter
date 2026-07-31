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
                 uint8_t channel,
                 uint8_t note) {
    assert(event.type == type);
    assert(event.source == MusicalEventSource::PerformanceKeyboard);
    assert(event.target == target);
    assert(event.channel == channel);
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
                MusicalEventTarget::SynthA, 0, 36);

    keyboard.setTarget(MusicalEventTarget::SynthB);
    assert(sink.events.size() == 2);
    expectEvent(sink.events[1], MusicalEventType::AllNotesOff,
                MusicalEventTarget::SynthA, 0, 0);
    assert(keyboard.heldCount() == 0);
    assert(keyboard.target() == MusicalEventTarget::SynthB);
    assert(std::strcmp(keyboard.targetName(), "SYNTH B") == 0);
    assert(keyboard.targetMidiChannel() == 9);

    sink.events.clear();
    keyboard.cycleTarget(1);
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff,
                MusicalEventTarget::SynthB, 0, 0);
    assert(keyboard.target() == MusicalEventTarget::Dx);
    assert(std::strcmp(keyboard.targetName(), "DX") == 0);
    assert(keyboard.targetMidiChannel() == 10);

    sink.events.clear();
    assert(keyboard.keyDown('s', 101));
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::NoteOn,
                MusicalEventTarget::Dx, 0, 38);
    assert(keyboard.keyUp('s'));
    assert(sink.events.size() == 2);
    expectEvent(sink.events[1], MusicalEventType::NoteOff,
                MusicalEventTarget::Dx, 0, 38);

    sink.events.clear();
    keyboard.cycleTarget(1);
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff,
                MusicalEventTarget::Dx, 0, 0);
    assert(keyboard.target() == MusicalEventTarget::Drums);
    assert(std::strcmp(keyboard.targetName(), "DRUMS") == 0);
    assert(keyboard.targetMidiChannel() == 1);

    // Native SEQTRAK drum performance: A/S/D/F/G/H/J -> CH1..7, note 60.
    sink.events.clear();
    constexpr char drumKeys[] = "asdfghj";
    for (uint8_t i = 0; i < 7; ++i) {
        assert(keyboard.keyDown(drumKeys[i], static_cast<uint8_t>(90 + i)));
        assert(sink.events.size() == static_cast<std::size_t>(i + 1));
        expectEvent(sink.events.back(), MusicalEventType::NoteOn,
                    MusicalEventTarget::Drums, i,
                    PerformanceKeyboard::kSeqtrakDrumNote);
    }
    assert(keyboard.heldCount() == 7);

    // Drum lanes are independent: releasing one pad does not retrigger another.
    assert(keyboard.keyUp('a'));
    assert(sink.events.size() == 8);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff,
                MusicalEventTarget::Drums, 0,
                PerformanceKeyboard::kSeqtrakDrumNote);
    assert(keyboard.heldCount() == 6);

    // Keys outside the seven native pads remain consumed in NOTE mode but do
    // not emit an invalid drum lane.
    const std::size_t beforeUnused = sink.events.size();
    assert(keyboard.keyDown('q', 100));
    assert(sink.events.size() == beforeUnused);

    keyboard.panic();
    assert(sink.events.size() == beforeUnused + 1);
    expectEvent(sink.events.back(), MusicalEventType::AllNotesOff,
                MusicalEventTarget::Drums, 0, 0);
    assert(keyboard.heldCount() == 0);

    sink.events.clear();
    keyboard.cycleTarget(1);
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff,
                MusicalEventTarget::Drums, 0, 0);
    assert(keyboard.target() == MusicalEventTarget::SynthA);
    assert(keyboard.targetMidiChannel() == 8);

    keyboard.cycleTarget(-1);
    assert(keyboard.target() == MusicalEventTarget::Drums);

    return 0;
}
