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
    assert(keyboard.velocity() == PerformanceKeyboard::kDefaultVelocity);
    static_assert(PerformanceKeyboard::kMaxHeldNotes == 19);
    static_assert(PerformanceKeyboard::kMaxPolyChordNotes == 16);
    static_assert(PerformanceKeyboard::kMinVelocity == 10);
    static_assert(PerformanceKeyboard::kMaxVelocity == 120);
    static_assert(PerformanceKeyboard::kVelocityStep == 10);
    static_assert(PerformanceKeyboard::kDefaultVelocity == 100);

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

    // MONO sends the same physical key lifecycle as a normal MIDI keyboard.
    // The receiver owns one-voice priority/legato; GroovePuter never suppresses
    // an inactive-key release or synthesizes a new NoteOn for a still-held key.
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
    assert(sink.events.size() == 3);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 36);

    assert(keyboard.keyUp('s'));
    assert(keyboard.heldCount() == 0);
    assert(sink.events.size() == 4);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 38);

    // POLY keeps the same exact per-key lifecycle but uses its dedicated source
    // so UsbMidiOutput can request POLY receiver mode on the external target.
    sink.clear();
    keyboard.setVoiceMode(PerformanceVoiceMode::Poly);
    assert(keyboard.voiceMode() == PerformanceVoiceMode::Poly);
    assert(std::strcmp(keyboard.voiceModeName(), "POLY") == 0);
    assert(keyboard.directPolyphonyEnabled());
    assert(sink.events.size() == 1);
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

    // POLY+CHORD is a sustained union, not last-root replacement.
    sink.clear();
    keyboard.setChordMode(PerformanceChordMode::Fifth);
    assert(sink.events.size() == 1);
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

    assert(keyboard.keyDown('q', 104));  // 48,55,60; 48 already sounds
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

    assert(keyboard.keyUp('q'));
    assert(keyboard.heldCount() == 0);
    assert(sink.events.size() == 10);
    expectEvent(sink.events[7], MusicalEventType::NoteOff, 48,
                MusicalEventSource::Arpeggiator);
    expectEvent(sink.events[8], MusicalEventType::NoteOff, 55,
                MusicalEventSource::Arpeggiator);
    expectEvent(sink.events[9], MusicalEventType::NoteOff, 60,
                MusicalEventSource::Arpeggiator);

    // Direct POLY+CHORD has an explicit 16-unique-note ceiling.
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

    // Releasing the newer key in MONO emits only its NoteOff. The older held
    // key stays logically down at the receiver and must not receive a new attack.
    sink.clear();
    assert(keyboard.keyDown('a'));
    assert(keyboard.keyDown('d'));
    assert(keyboard.keyUp('d'));
    assert(sink.events.size() == 3);
    expectEvent(sink.events[2], MusicalEventType::NoteOff, 39);
    assert(keyboard.activeNote() == 36);

    const std::size_t beforeRepeat = sink.events.size();
    assert(keyboard.keyDown('a'));
    assert(keyboard.heldCount() == 1);
    assert(sink.events.size() == beforeRepeat);

    keyboard.releaseMissingKeys(nullptr, 0);
    assert(keyboard.heldCount() == 0);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 36);

    // Matrix reconciliation removes only missing MONO keys and never reattacks
    // an older key that is still physically down.
    sink.clear();
    assert(keyboard.keyDown('a'));
    assert(keyboard.keyDown('s'));
    const char onlyA[] = {'a'};
    keyboard.releaseMissingKeys(onlyA, 1);
    assert(keyboard.heldCount() == 1);
    assert(keyboard.activeNote() == 36);
    assert(sink.events.size() == 3);
    expectEvent(sink.events.back(), MusicalEventType::NoteOff, 38);
    assert(keyboard.keyUp('a'));

    // Cardputer velocity is a fixed runtime control: 10..120, step 10, default
    // 100. Changing it affects future keyDown events only and never panics held
    // notes or rewrites their already-sent velocity.
    sink.clear();
    assert(keyboard.velocity() == 100);
    assert(keyboard.adjustVelocity(-1));
    assert(keyboard.velocity() == 90);
    assert(keyboard.keyDown('a'));
    assert(sink.events.size() == 1);
    assert(sink.events[0].velocity == 90);
    assert(keyboard.adjustVelocity(1));
    assert(keyboard.velocity() == 100);
    assert(sink.events.size() == 1);
    assert(keyboard.keyUp('a'));

    keyboard.setVelocity(1);
    assert(keyboard.velocity() == 10);
    assert(!keyboard.adjustVelocity(-1));
    for (int i = 0; i < 20; ++i) keyboard.adjustVelocity(1);
    assert(keyboard.velocity() == 120);
    assert(!keyboard.adjustVelocity(1));
    keyboard.setVelocity(95);
    assert(keyboard.velocity() == 100);

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

    sink.clear();
    keyboard.panic();
    assert(keyboard.heldCount() == 0);
    assert(keyboard.activeNote() == -1);
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::AllNotesOff, 0);

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
