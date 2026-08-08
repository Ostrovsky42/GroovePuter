#include <cassert>
#include <cstring>
#include <string>
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
                 uint8_t note,
                 MusicalEventSource source = MusicalEventSource::PerformanceKeyboard) {
    assert(event.type == type);
    assert(event.source == source);
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
    assert(keyboard.noteModeEnabled());
    assert(keyboard.liveInputAllowed());
    assert(keyboard.voiceMode() == PerformanceVoiceMode::Mono);
    assert(std::strcmp(keyboard.voiceModeName(), "MONO") == 0);
    assert(!keyboard.directPolyphonyEnabled());
    static_assert(PerformanceKeyboard::kMaxHeldNotes == 19);
    static_assert(PerformanceKeyboard::kMaxPolyChordNotes == 16);

    for (char key : std::string("iopkl")) {
        assert(PerformanceKeyboard::isPerformanceKey(key));
    }
    assert(!PerformanceKeyboard::isPerformanceKey('n'));
    assert(!PerformanceKeyboard::isPerformanceKey('z'));

    uint8_t note = 0;
    assert(keyboard.noteForKey('a', note) && note == 36);
    assert(keyboard.noteForKey('s', note) && note == 38);
    assert(keyboard.noteForKey('d', note) && note == 39);
    assert(keyboard.noteForKey('q', note) && note == 48);
    assert(keyboard.noteForKey('w', note) && note == 50);
    assert(!keyboard.noteForKey('z', note));

    // MONO remains last-note priority: C2, then D2, then release inactive C2.
    assert(keyboard.keyDown('a', 90));
    assert(keyboard.keyDown('s', 110));
    assert(keyboard.heldCount() == 2);
    assert(keyboard.activeNote() == 38);
    assert(sink.events.size() == 2);
    expectEvent(sink.events[0], MusicalEventType::NoteOn, 36);
    expectEvent(sink.events[1], MusicalEventType::NoteOn, 38);
    assert(sink.events[1].velocity == 110);

    assert(keyboard.keyUp('a'));
    assert(keyboard.heldCount() == 1);
    assert(keyboard.activeNote() == 38);
    assert(sink.events.size() == 2);  // inactive release does not touch voice

    assert(keyboard.keyUp('s'));
    assert(keyboard.heldCount() == 0);
    assert(sink.events.size() == 3);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 38);

    // POLY is direct external-MIDI ownership: every physical key owns its own
    // NoteOn/NoteOff, while the internal synth can ignore this dedicated source.
    sink.clear();
    keyboard.setVoiceMode(PerformanceVoiceMode::Poly);
    assert(keyboard.voiceMode() == PerformanceVoiceMode::Poly);
    assert(std::strcmp(keyboard.voiceModeName(), "POLY") == 0);
    assert(keyboard.directPolyphonyEnabled());
    assert(sink.events.size() == 1);  // mode transition panic
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff, 0);

    sink.clear();
    assert(keyboard.keyDown('a', 90));
    assert(keyboard.keyDown('s', 110));
    assert(keyboard.keyDown('d', 100));
    assert(keyboard.heldCount() == 3);
    assert(sink.events.size() == 3);
    expectEvent(sink.events[0], MusicalEventType::NoteOn, 36,
                MusicalEventSource::PerformanceKeyboardPoly);
    expectEvent(sink.events[1], MusicalEventType::NoteOn, 38,
                MusicalEventSource::PerformanceKeyboardPoly);
    expectEvent(sink.events[2], MusicalEventType::NoteOn, 39,
                MusicalEventSource::PerformanceKeyboardPoly);

    // Releasing the middle note must release only that exact note.
    assert(keyboard.keyUp('s'));
    assert(keyboard.heldCount() == 2);
    assert(keyboard.activeNote() == 39);
    assert(sink.events.size() == 4);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 38,
                MusicalEventSource::PerformanceKeyboardPoly);
    assert(keyboard.keyUp('a'));
    assert(keyboard.heldCount() == 1);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 36,
                MusicalEventSource::PerformanceKeyboardPoly);
    assert(keyboard.keyUp('d'));
    assert(keyboard.heldCount() == 0);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 39,
                MusicalEventSource::PerformanceKeyboardPoly);

    // Matrix reconciliation in POLY releases each missing key independently and
    // must not retrigger the remaining held note.
    sink.clear();
    assert(keyboard.keyDown('a'));
    assert(keyboard.keyDown('s'));
    const char polyOnlyA[] = {'a'};
    keyboard.releaseMissingKeys(polyOnlyA, 1);
    assert(keyboard.heldCount() == 1);
    assert(keyboard.activeNote() == 36);
    assert(sink.events.size() == 3);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 38,
                MusicalEventSource::PerformanceKeyboardPoly);
    assert(keyboard.keyUp('a'));
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 36,
                MusicalEventSource::PerformanceKeyboardPoly);

    // POLY+CHORD is a sustained union, not last-root replacement. C5th owns
    // C/G/C and the upper C5th shares MIDI 48. Adding the second root must emit
    // only the two new notes; releasing either root must not retrigger or cut
    // the shared note while the other root still owns it.
    sink.clear();
    keyboard.setChordMode(PerformanceChordMode::Fifth);
    assert(sink.events.size() == 1);  // configuration panic
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff, 0);
    sink.clear();

    assert(keyboard.keyDown('a', 96));  // 36,43,48
    assert(sink.events.size() == 3);
    expectEvent(sink.events[0], MusicalEventType::NoteOn, 36,
                MusicalEventSource::Arpeggiator);
    expectEvent(sink.events[1], MusicalEventType::NoteOn, 43,
                MusicalEventSource::Arpeggiator);
    expectEvent(sink.events[2], MusicalEventType::NoteOn, 48,
                MusicalEventSource::Arpeggiator);

    assert(keyboard.keyDown('q', 104));  // 48,55,60; 48 is already sounding
    assert(sink.events.size() == 5);
    expectEvent(sink.events[3], MusicalEventType::NoteOn, 55,
                MusicalEventSource::Arpeggiator);
    expectEvent(sink.events[4], MusicalEventType::NoteOn, 60,
                MusicalEventSource::Arpeggiator);

    assert(keyboard.keyUp('a'));
    assert(keyboard.heldCount() == 1);
    assert(sink.events.size() == 7);
    expectEvent(sink.events[5], MusicalEventType::NoteOff, 36,
                MusicalEventSource::Arpeggiator);
    expectEvent(sink.events[6], MusicalEventType::NoteOff, 43,
                MusicalEventSource::Arpeggiator);
    // No NoteOn 48 is emitted here: the upper root already owns/sustains it.

    assert(keyboard.keyUp('q'));
    assert(keyboard.heldCount() == 0);
    assert(sink.events.size() == 10);
    expectEvent(sink.events[7], MusicalEventType::NoteOff, 48,
                MusicalEventSource::Arpeggiator);
    expectEvent(sink.events[8], MusicalEventType::NoteOff, 55,
                MusicalEventSource::Arpeggiator);
    expectEvent(sink.events[9], MusicalEventType::NoteOff, 60,
                MusicalEventSource::Arpeggiator);

    // Direct POLY+CHORD has an explicit 16-unique-note ceiling. Six NAT MINOR
    // roots in MIN7 fill it; a seventh held root may be tracked physically but
    // cannot add a seventeenth simultaneous generated MIDI note.
    sink.clear();
    keyboard.setChordMode(PerformanceChordMode::Minor7);
    assert(sink.events.size() == 1);
    sink.clear();
    for (char key : std::string("asdfgh")) assert(keyboard.keyDown(key));
    assert(keyboard.heldCount() == 6);
    assert(sink.events.size() == PerformanceKeyboard::kMaxPolyChordNotes);
    for (const MusicalEvent& event : sink.events) {
        assert(event.type == MusicalEventType::NoteOn);
        assert(event.source == MusicalEventSource::Arpeggiator);
    }
    assert(keyboard.keyDown('j'));
    assert(keyboard.heldCount() == 7);
    assert(sink.events.size() == PerformanceKeyboard::kMaxPolyChordNotes);
    keyboard.panic();

    sink.clear();
    keyboard.setChordMode(PerformanceChordMode::Off);
    assert(sink.events.size() == 1);
    sink.clear();
    keyboard.setVoiceMode(PerformanceVoiceMode::Mono);
    assert(keyboard.voiceMode() == PerformanceVoiceMode::Mono);
    assert(!keyboard.directPolyphonyEnabled());
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff, 0);

    // Releasing the active key restores the previous held note in MONO.
    sink.clear();
    assert(keyboard.keyDown('a'));
    assert(keyboard.keyDown('d'));
    assert(keyboard.keyUp('d'));
    assert(sink.events.size() == 3);
    expectEvent(sink.events[2], MusicalEventType::NoteOn, 36);
    assert(keyboard.activeNote() == 36);

    // Matrix repeats do not duplicate held state or retrigger.
    const std::size_t beforeRepeat = sink.events.size();
    assert(keyboard.keyDown('a'));
    assert(keyboard.heldCount() == 1);
    assert(sink.events.size() == beforeRepeat);

    // Matrix reconciliation recovers a missing key-up.
    keyboard.releaseMissingKeys(nullptr, 0);
    assert(keyboard.heldCount() == 0);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 36);

    sink.clear();
    assert(keyboard.keyDown('a'));
    assert(keyboard.keyDown('s'));
    const char onlyA[] = {'a'};
    keyboard.releaseMissingKeys(onlyA, 1);
    assert(keyboard.heldCount() == 1);
    assert(keyboard.activeNote() == 36);
    expectEvent(sink.events.back(), MusicalEventType::NoteOn, 36);

    // Transport playback no longer revokes live keyboard ownership. Direct
    // notes remain immediate and the transition itself must not emit panic.
    keyboard.panic();
    sink.clear();
    keyboard.setTransportPlaying(true);
    assert(keyboard.transportPlaying());
    assert(keyboard.liveInputAllowed());
    assert(keyboard.heldCount() == 0);
    assert(sink.events.empty());

    assert(keyboard.keyDown('a', 96));
    assert(keyboard.heldCount() == 1);
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::NoteOn, 36);
    assert(sink.events[0].velocity == 96);
    assert(keyboard.keyUp('a'));
    assert(sink.events.size() == 2);
    expectEvent(sink.events[1], MusicalEventType::NoteOff, 36);
    assert(!keyboard.keyDown('z'));

    keyboard.setTransportPlaying(false);
    assert(keyboard.liveInputAllowed());

    // NOTE mode is explicit. Turning it off releases live ownership and allows
    // the same letters to reach legacy commands; turning it on reserves them.
    sink.clear();
    keyboard.setNoteModeEnabled(false);
    assert(!keyboard.noteModeEnabled());
    assert(!keyboard.liveInputAllowed());
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff, 0);
    sink.clear();
    assert(!keyboard.keyDown('i'));
    assert(sink.events.empty());

    keyboard.setNoteModeEnabled(true);
    assert(keyboard.noteModeEnabled());
    assert(keyboard.liveInputAllowed());
    assert(keyboard.keyDown('q'));
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
    const char lowerManual[] = "asdfghjkl";
    const char upperManual[] = "qwertyuio";
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
            for (std::size_t i = 0; i < sizeof(lowerManual) - 1; ++i) {
                uint8_t lower = 0;
                uint8_t upper = 0;
                assert(keyboard.noteForKey(lowerManual[i], lower));
                assert(keyboard.noteForKey(upperManual[i], upper));
                assert(upper == static_cast<uint8_t>(lower + 12));
            }
        }
    }

    assert(!keyboard.shiftOctave(1));
    assert(keyboard.octaveShift() == PerformanceKeyboard::kMaxOctaveShift);

    router.removeSink(sink);
    assert(router.sinkCount() == 0);
    return 0;
}
