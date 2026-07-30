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

    void clear() { events.clear(); }

    std::vector<MusicalEvent> events;
};

void expectEvent(const MusicalEvent& event,
                 MusicalEventType type,
                 uint8_t note) {
    assert(event.type == type);
    assert(event.source == MusicalEventSource::PerformanceKeyboard);
    assert(event.target == MusicalEventTarget::SynthA);
    assert(event.channel == 0);
    assert(event.note == note);
}
}  // namespace

int main() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    assert(router.addSink(sink));
    assert(router.sinkCount() == 1);

    PerformanceKeyboard keyboard(router);
    assert(std::strcmp(keyboard.scaleName(), "NAT MINOR") == 0);
    assert(keyboard.octaveShift() == 0);
    assert(keyboard.liveInputAllowed());

    uint8_t note = 0;
    assert(keyboard.noteForKey('a', note) && note == 48);
    assert(keyboard.noteForKey('s', note) && note == 50);
    assert(keyboard.noteForKey('d', note) && note == 51);
    assert(keyboard.noteForKey('q', note) && note == 48);
    assert(!keyboard.noteForKey('z', note));

    // Last-note priority: C, then D, then release inactive C.
    assert(keyboard.keyDown('a', 90));
    assert(keyboard.keyDown('s', 110));
    assert(keyboard.heldCount() == 2);
    assert(keyboard.activeNote() == 50);
    assert(sink.events.size() == 2);
    expectEvent(sink.events[0], MusicalEventType::NoteOn, 48);
    expectEvent(sink.events[1], MusicalEventType::NoteOn, 50);
    assert(sink.events[1].velocity == 110);

    assert(keyboard.keyUp('a'));
    assert(keyboard.heldCount() == 1);
    assert(keyboard.activeNote() == 50);
    assert(sink.events.size() == 2);  // inactive release does not touch voice

    assert(keyboard.keyUp('s'));
    assert(keyboard.heldCount() == 0);
    assert(sink.events.size() == 3);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 50);

    // Releasing the active key restores the previous held note.
    sink.clear();
    assert(keyboard.keyDown('a'));
    assert(keyboard.keyDown('d'));
    assert(keyboard.keyUp('d'));
    assert(sink.events.size() == 3);
    expectEvent(sink.events[2], MusicalEventType::NoteOn, 48);
    assert(keyboard.activeNote() == 48);

    // Matrix repeats do not duplicate held state or retrigger.
    const std::size_t beforeRepeat = sink.events.size();
    assert(keyboard.keyDown('a'));
    assert(keyboard.heldCount() == 1);
    assert(sink.events.size() == beforeRepeat);

    // Matrix reconciliation recovers a missing key-up.
    keyboard.releaseMissingKeys(nullptr, 0);
    assert(keyboard.heldCount() == 0);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 48);

    sink.clear();
    assert(keyboard.keyDown('a'));
    assert(keyboard.keyDown('s'));
    const char onlyA[] = {'a'};
    keyboard.releaseMissingKeys(onlyA, 1);
    assert(keyboard.heldCount() == 1);
    assert(keyboard.activeNote() == 48);
    expectEvent(sink.events.back(), MusicalEventType::NoteOn, 48);

    // Starting transport returns Synth A to PatternPlayer and blocks live input.
    sink.clear();
    keyboard.setTransportPlaying(true);
    assert(keyboard.transportPlaying());
    assert(!keyboard.liveInputAllowed());
    assert(keyboard.heldCount() == 0);
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff, 0);
    assert(!keyboard.keyDown('a'));
    assert(sink.events.size() == 1);

    keyboard.setTransportPlaying(false);
    assert(keyboard.liveInputAllowed());
    assert(keyboard.keyDown('a'));
    assert(keyboard.activeNote() == 48);

    // Panic is deterministic even if the source has already lost key-up state.
    sink.clear();
    keyboard.panic();
    assert(keyboard.heldCount() == 0);
    assert(keyboard.activeNote() == -1);
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff, 0);

    // Runtime-only scale/octave state always remains inside the synth range.
    const char playableKeys[] = "asdfghjklqwertyuiop";
    for (int scaleIndex = 0;
         scaleIndex < static_cast<int>(PerformanceScale::Count);
         ++scaleIndex) {
        keyboard.setScale(static_cast<PerformanceScale>(scaleIndex));
        for (int octave = PerformanceKeyboard::kMinOctaveShift;
             octave <= PerformanceKeyboard::kMaxOctaveShift;
             ++octave) {
            while (keyboard.octaveShift() < octave) assert(keyboard.shiftOctave(1));
            while (keyboard.octaveShift() > octave) assert(keyboard.shiftOctave(-1));
            for (char key : playableKeys) {
                if (key == '\0') break;
                assert(keyboard.noteForKey(key, note));
                assert(note >= PerformanceKeyboard::kMinNote);
                assert(note <= PerformanceKeyboard::kMaxNote);
            }
        }
    }

    assert(!keyboard.shiftOctave(1));
    assert(keyboard.octaveShift() == PerformanceKeyboard::kMaxOctaveShift);

    router.removeSink(sink);
    assert(router.sinkCount() == 0);
    return 0;
}
