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
    keyboard.setTarget(MusicalEventTarget::SynthB);
    sink.clear();
    const PerformanceInputIdentity bendUsb{4, 1, 2, 0};
    const PerformanceInputIdentity bendUart{9, 2, 2, 0};
    assert(keyboard.externalPitchBend(4096, bendUsb));
    assert(sink.events.size() == 1u);
    assert(sink.events.back().type == MusicalEventType::PitchBend);
    assert(sink.events.back().target == MusicalEventTarget::SynthB);
    assert(sink.events.back().pitchBend14 == 4096u);
    assert(keyboard.externalPitchBend(12288, bendUart));
    assert(sink.events.size() == 2u && sink.events.back().pitchBend14 == 12288u);
    assert(keyboard.externalPitchBend(8192, bendUsb));
    assert(sink.events.size() == 2u); // USB release does not clear the UART-owned bend.
    assert(keyboard.externalPitchBend(8192, bendUart));
    assert(sink.events.size() == 3u && sink.events.back().pitchBend14 == 8192u);
    const PerformanceInputIdentity bendUsbReconnect{5, 1, 2, 0};
    assert(keyboard.externalPitchBend(14000, bendUsbReconnect));
    assert(sink.events.size() == 4u && sink.events.back().pitchBend14 == 14000u);
    assert(keyboard.externalPitchBend(8192, bendUsb)); // stale prior-session center
    assert(sink.events.size() == 4u);
    assert(keyboard.externalPitchBend(8192, bendUsbReconnect));
    assert(sink.events.size() == 5u && sink.events.back().pitchBend14 == 8192u);
    assert(keyboard.externalPitchBend(4096, bendUsbReconnect));
    assert(sink.events.back().target == MusicalEventTarget::SynthB &&
           sink.events.back().pitchBend14 == 4096u);
    keyboard.setTarget(MusicalEventTarget::SynthA);
    assert(sink.events.back().type == MusicalEventType::PitchBend &&
           sink.events.back().target == MusicalEventTarget::SynthB &&
           sink.events.back().pitchBend14 == 8192u);
    sink.clear();

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

    // POLY+CHORD is a sustained union, not last-root replacement. Changing an
    // idle chord mode does not emit destructive cleanup when nothing is sounding.
    sink.clear();
    keyboard.setChordMode(PerformanceChordMode::Fifth);
    assert(sink.events.empty());

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
    assert(sink.events.empty());
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
    assert(sink.events.empty());
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

    // Distinct MIDI sessions can hold the same pitch. The shared musical note is released only
    // after the final source owner releases it.
    keyboard.panic();
    sink.clear();
    const PerformanceInputIdentity usb{17, 1, 0, 60};
    const PerformanceInputIdentity uart{4, 2, 0, 60};
    assert(keyboard.externalNoteOn(60, 90, usb));
    assert(keyboard.externalNoteOn(60, 100, uart));
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::NoteOn, 60);
    assert(keyboard.externalNoteOff(60, usb));
    assert(sink.events.size() == 1);
    assert(keyboard.externalNoteOff(60, uart));
    assert(sink.events.size() == 2);
    expectEvent(sink.events[1], MusicalEventType::NoteOff, 60);

    keyboard.panic();
    sink.clear();
    const PerformanceInputIdentity low{21, 1, 2, 0};
    const PerformanceInputIdentity high{21, 1, 2, 127};
    assert(keyboard.externalRawNoteOn(0, 71, low));
    assert(keyboard.externalRawNoteOn(127, 113, high));
    assert(sink.events.size() == 2);
    assert(sink.events[0].type == MusicalEventType::NoteOn && sink.events[0].note == 0);
    assert(sink.events[0].velocity == 71);
    assert(sink.events[1].type == MusicalEventType::NoteOn && sink.events[1].note == 127);
    assert(sink.events[1].velocity == 113);
    assert(keyboard.externalRawNoteOff(0, low));
    assert(keyboard.externalRawNoteOff(127, high));
    assert(sink.events.size() == 4);
    assert(sink.events[2].type == MusicalEventType::NoteOff && sink.events[2].note == 0);
    assert(sink.events[3].type == MusicalEventType::NoteOff && sink.events[3].note == 127);

    // External RAW and local POLY are two physical owners of one receiver pitch.
    // Releasing either owner first must not cut the pitch while the other is held.
    keyboard.panic();
    keyboard.setVoiceMode(PerformanceVoiceMode::Poly);
    sink.clear();
    uint8_t localPolyNote = 0;
    assert(keyboard.noteForKey('a', localPolyNote));
    const PerformanceInputIdentity polyExternal{31, 1, 0, localPolyNote};
    assert(keyboard.keyDown('a', 90));
    assert(keyboard.externalRawNoteOn(localPolyNote, 100, polyExternal));
    assert(keyboard.externalRawNoteOff(localPolyNote, polyExternal));
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::NoteOn, localPolyNote,
                MusicalEventSource::PerformanceKeyboardPoly);
    assert(keyboard.keyUp('a'));
    assert(sink.events.size() == 2);
    expectEvent(sink.events[1], MusicalEventType::NoteOff, localPolyNote,
                MusicalEventSource::PerformanceKeyboardPoly);

    sink.clear();
    assert(keyboard.externalRawNoteOn(localPolyNote, 100, polyExternal));
    assert(keyboard.keyDown('a', 90));
    assert(keyboard.keyUp('a'));
    assert(sink.events.size() == 1);
    expectEvent(sink.events[0], MusicalEventType::NoteOn, localPolyNote,
                MusicalEventSource::PerformanceKeyboardPoly);
    assert(keyboard.externalRawNoteOff(localPolyNote, polyExternal));
    assert(sink.events.size() == 2);
    expectEvent(sink.events[1], MusicalEventType::NoteOff, localPolyNote,
                MusicalEventSource::PerformanceKeyboardPoly);

    // CC120 force-silences a scoped MIDI channel even while that channel owns sustain.
    keyboard.panic();
    sink.clear();
    const PerformanceInputIdentity sustainedPedal{32, 1, 4, 0xff};
    const PerformanceInputIdentity sustainedKey{32, 1, 4, 60};
    keyboard.externalSustain(true, sustainedPedal);
    assert(keyboard.externalNoteOn(60, 100, sustainedKey));
    assert(keyboard.externalNoteOff(60, sustainedKey));
    assert(sink.events.size() == 1u);
    assert(sink.events[0].type == MusicalEventType::NoteOn);
    keyboard.externalAllSoundOff(sustainedPedal);
    assert(sink.events.size() == 2u);
    expectEvent(sink.events[1], MusicalEventType::NoteOff, 60,
                MusicalEventSource::PerformanceKeyboardPoly);

    keyboard.panic();
    sink.clear();
    assert(keyboard.performanceRole() == GroovePuterPerformance::PerformanceRole::Raw);
    const PerformanceInputIdentity rawZero{22, 1, 0, 0};
    const PerformanceInputIdentity raw127{22, 1, 0, 127};
    assert(keyboard.externalNoteOn(0, 83, rawZero));
    assert(keyboard.externalNoteOn(127, 107, raw127));
    assert(sink.events.size() == 2);
    assert(sink.events[0].note == 0 && sink.events[1].note == 127);
    assert(sink.events[0].velocity == 83 && sink.events[1].velocity == 107);
    assert(keyboard.externalNoteOff(0, rawZero));
    assert(keyboard.externalNoteOff(127, raw127));

    // A direct Cardputer key and an external RAW note share one sounding pitch owner.
    PerformanceKeyboard sharedOwner(router);
    sink.clear();
    assert(sharedOwner.keyDown('s', 100));
    assert(sink.events.size() == 1 && sink.events[0].type == MusicalEventType::NoteOn);
    const uint8_t cardputerPitch = sink.events[0].note;
    const PerformanceInputIdentity sharedCardputerPitch{25, 1, 0, cardputerPitch};
    assert(sharedOwner.externalNoteOn(cardputerPitch, 100, sharedCardputerPitch));
    assert(sink.events.size() == 1);
    assert(sharedOwner.externalNoteOff(cardputerPitch, sharedCardputerPitch));
    assert(sink.events.size() == 1); // Cardputer still owns the pitch.
    assert(sharedOwner.keyUp('s'));
    assert(sink.events.size() == 2 && sink.events.back().type == MusicalEventType::NoteOff &&
           sink.events.back().note == cardputerPitch);

    sharedOwner.panic();
    sink.clear();
    assert(sharedOwner.externalNoteOn(cardputerPitch, 100, sharedCardputerPitch));
    assert(sharedOwner.keyDown('s', 100));
    assert(sink.events.size() == 1);
    assert(sharedOwner.keyUp('s'));
    assert(sink.events.size() == 1); // External still owns the pitch.
    assert(sharedOwner.externalNoteOff(cardputerPitch, sharedCardputerPitch));
    assert(sink.events.size() == 2 && sink.events.back().type == MusicalEventType::NoteOff);

    // Two BASS gestures can resolve to one pitch. Releasing either key must
    // leave the shared output sounding until the last local owner releases.
    sharedOwner.panic();
    sink.clear();
    GroovePuterPerformance::PerformanceContext bassContext{};
    bassContext.hasHarmony = true;
    bassContext.rootMidi = 60;
    bassContext.harmony.quality = GroovePuterRhythm::ChordQuality::Triad;
    bassContext.minNote = 36;
    bassContext.maxNote = 60;
    sharedOwner.setPerformanceContext(bassContext);
    assert(sharedOwner.setPerformanceRole(GroovePuterPerformance::PerformanceRole::Bass));
    assert(sharedOwner.keyDown('s', 100)); // degree 2 -> THIRD
    const uint8_t bassThird = sink.events.back().note;
    assert(sharedOwner.keyDown('j', 100)); // degree 7 -> THIRD
    assert(sink.events.size() == 1 && sink.events[0].note == bassThird);
    assert(sharedOwner.keyUp('s'));
    assert(sink.events.size() == 1);
    assert(sharedOwner.keyUp('j'));
    assert(sink.events.size() == 2 && sink.events.back().type == MusicalEventType::NoteOff &&
           sink.events.back().note == bassThird);

    sink.clear();
    const PerformanceInputIdentity bassRootInput{31, 1, 0, 60};
    const PerformanceInputIdentity bassThirdInput{31, 1, 0, 64};
    const PerformanceInputIdentity bassFifthInput{31, 1, 0, 67};
    const PerformanceInputIdentity bassApproachInput{31, 1, 0, 59};
    const PerformanceInputIdentity unsupportedBassInput{31, 1, 0, 62};
    assert(sharedOwner.externalNoteOn(60, 90, bassRootInput));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 60);
    assert(sharedOwner.externalNoteOff(60, bassRootInput));
    assert(sharedOwner.externalNoteOn(64, 90, bassThirdInput));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 52);
    assert(sharedOwner.externalNoteOff(64, bassThirdInput));
    assert(sharedOwner.externalNoteOn(67, 90, bassFifthInput));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 55);
    assert(sharedOwner.externalNoteOff(67, bassFifthInput));
    bassContext.harmony.quality = GroovePuterRhythm::ChordQuality::Major7;
    sharedOwner.setPerformanceContext(bassContext);
    const PerformanceInputIdentity bassSeventhInput{31, 1, 0, 71};
    assert(sharedOwner.externalNoteOn(71, 90, bassSeventhInput));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 59);
    assert(sharedOwner.externalNoteOff(71, bassSeventhInput));
    bassContext.harmony.quality = GroovePuterRhythm::ChordQuality::Triad;
    sharedOwner.setPerformanceContext(bassContext);
    assert(sharedOwner.externalNoteOn(59, 90, bassApproachInput));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 59);
    assert(sharedOwner.externalNoteOff(59, bassApproachInput));
    const std::size_t bassEventCount = sink.events.size();
    assert(!sharedOwner.externalNoteOn(62, 90, unsupportedBassInput));
    assert(sink.events.size() == bassEventCount); // Unknown pitch is not silently recast as ROOT.

    // Identity-scoped sustain holds RAW NoteOff until that source/channel releases the pedal.
    assert(sharedOwner.setPerformanceRole(GroovePuterPerformance::PerformanceRole::Raw));
    sharedOwner.clearPerformanceHarmonyContext();
    sharedOwner.panic();
    sink.clear();
    const PerformanceInputIdentity usbSustain{26, 1, 0, 0xff};
    const PerformanceInputIdentity uartSustain{27, 2, 0, 0xff};
    const PerformanceInputIdentity sustainedUsbNote{26, 1, 0, 64};
    const PerformanceInputIdentity sustainedUartNote{27, 2, 0, 64};
    assert(sharedOwner.dispatchIntent({PerformanceIntentKind::Hold, usbSustain, 0, 0}));
    assert(sharedOwner.dispatchIntent({PerformanceIntentKind::Hold, uartSustain, 0, 0}));
    assert(sharedOwner.externalNoteOn(64, 90, sustainedUsbNote));
    assert(sharedOwner.externalNoteOn(64, 90, sustainedUartNote));
    assert(sharedOwner.externalNoteOff(64, sustainedUsbNote));
    assert(sharedOwner.externalNoteOff(64, sustainedUartNote));
    assert(sink.events.size() == 1);
    assert(sharedOwner.dispatchIntent({PerformanceIntentKind::ReleaseHold, usbSustain, 0, 0}));
    assert(sink.events.size() == 1); // UART sustain still has the shared note.
    assert(sharedOwner.dispatchIntent({PerformanceIntentKind::ReleaseHold, uartSustain, 0, 0}));
    assert(sink.events.size() == 2 && sink.events.back().type == MusicalEventType::NoteOff);

    // A same-owner re-press clears the stale deferred release while sustain is down.
    sharedOwner.panic();
    sink.clear();
    const PerformanceInputIdentity retriggerNote{28, 1, 0, 65};
    const PerformanceInputIdentity retriggerPedal{28, 1, 0, 0xff};
    assert(sharedOwner.dispatchIntent({PerformanceIntentKind::Hold, retriggerPedal, 0, 0}));
    assert(sharedOwner.externalNoteOn(65, 90, retriggerNote));
    assert(sharedOwner.externalNoteOff(65, retriggerNote));
    assert(sharedOwner.externalNoteOn(65, 110, retriggerNote));
    assert(sink.events.size() == 2u);
    assert(sink.events.back().type == MusicalEventType::NoteOn &&
           sink.events.back().note == 65u && sink.events.back().velocity == 110u);
    assert(sharedOwner.dispatchIntent({PerformanceIntentKind::ReleaseHold, retriggerPedal, 0, 0}));
    assert(sink.events.size() == 2); // stale deferred release was canceled by the new attack.
    assert(sharedOwner.externalNoteOff(65, retriggerNote));
    assert(sink.events.size() == 3 && sink.events.back().type == MusicalEventType::NoteOff);

    assert(keyboard.setPerformanceRole(GroovePuterPerformance::PerformanceRole::Lead));
    assert(keyboard.setLeadMode(GroovePuterPerformance::LeadMode::Scale));
    GroovePuterPerformance::PerformanceContext performanceContext{};
    performanceContext.scalePitchClasses = 0x0ab5u; // C major
    performanceContext.minNote = 48;
    performanceContext.maxNote = 84;
    keyboard.setPerformanceContext(performanceContext);
    const PerformanceInputIdentity scaledInput{22, 1, 0, 66};
    assert(keyboard.externalNoteOn(66, 90, scaledInput));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 65);
    assert(keyboard.externalNoteOff(66, scaledInput));

    assert(keyboard.setLeadMode(GroovePuterPerformance::LeadMode::VoiceLead));
    performanceContext.hasHarmony = true;
    performanceContext.rootMidi = 60;
    performanceContext.harmony.quality = GroovePuterRhythm::ChordQuality::Major7;
    performanceContext.minNote = 48;
    performanceContext.maxNote = 84;
    keyboard.setPerformanceContext(performanceContext);
    const PerformanceInputIdentity voiceOne{23, 1, 0, 64};
    assert(keyboard.externalNoteOn(64, 90, voiceOne));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 64);
    assert(keyboard.externalNoteOff(64, voiceOne));
    performanceContext.rootMidi = 65;
    performanceContext.harmony.quality = GroovePuterRhythm::ChordQuality::Major7;
    performanceContext.sourceOrdinal = 2;
    keyboard.setPerformanceContext(performanceContext);
    const PerformanceInputIdentity voiceTwo{23, 1, 0, 77};
    assert(keyboard.externalNoteOn(77, 90, voiceTwo));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 64);
    assert(keyboard.externalNoteOff(77, voiceTwo));

    PerformanceKeyboard standalone(router);
    assert(standalone.setPerformanceRole(GroovePuterPerformance::PerformanceRole::Lead));
    assert(standalone.setLeadMode(GroovePuterPerformance::LeadMode::Scale));
    assert(standalone.keyDown('s', 92));
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 38);
    assert(standalone.keyUp('s'));
    assert(standalone.setPerformanceRole(GroovePuterPerformance::PerformanceRole::Bass));
    assert(standalone.keyDown('s', 93));  // without a harmony provider, Bass remains playable.
    assert(sink.events.back().type == MusicalEventType::NoteOn && sink.events.back().note == 38);
    assert(standalone.keyUp('s'));

    const PerformanceKeyboardConfig savedConfig = keyboard.exportConfiguration();
    PerformanceKeyboard restored(router);
    assert(restored.applyConfiguration(savedConfig));
    const PerformanceKeyboardConfig loadedConfig = restored.exportConfiguration();
    assert(loadedConfig.role == savedConfig.role);
    assert(loadedConfig.leadMode == savedConfig.leadMode);
    assert(loadedConfig.arpPitchSource == savedConfig.arpPitchSource);
    assert(loadedConfig.harmonyChangePolicy == savedConfig.harmonyChangePolicy);
    assert(loadedConfig.arpDirection == savedConfig.arpDirection);
    assert(loadedConfig.chordMode == savedConfig.chordMode);
    assert(loadedConfig.rootPitchClass == savedConfig.rootPitchClass);
    PerformanceKeyboardConfig invalidConfig = savedConfig;
    invalidConfig.arpRate = 0xffu;
    assert(!restored.applyConfiguration(invalidConfig));

    PerformanceKeyboard intentConfigured(router);
    PerformanceIntent selectLead{};
    selectLead.kind = PerformanceIntentKind::SelectRole;
    selectLead.value = static_cast<uint8_t>(GroovePuterPerformance::PerformanceRole::Lead);
    assert(intentConfigured.dispatchIntent(selectLead));
    assert(intentConfigured.performanceRole() == GroovePuterPerformance::PerformanceRole::Lead);
    PerformanceIntent selectVoiceLead{};
    selectVoiceLead.kind = PerformanceIntentKind::SelectLeadMode;
    selectVoiceLead.value = static_cast<uint8_t>(GroovePuterPerformance::LeadMode::VoiceLead);
    assert(intentConfigured.dispatchIntent(selectVoiceLead));
    assert(intentConfigured.leadMode() == GroovePuterPerformance::LeadMode::VoiceLead);
    PerformanceIntent selectChordMode{};
    selectChordMode.kind = PerformanceIntentKind::SelectChordMode;
    selectChordMode.value = static_cast<uint8_t>(PerformanceChordMode::Major7);
    assert(intentConfigured.dispatchIntent(selectChordMode));
    assert(intentConfigured.chordMode() == PerformanceChordMode::Major7);
    PerformanceIntent selectHarmonySource{};
    selectHarmonySource.kind = PerformanceIntentKind::SelectArpPitchSource;
    selectHarmonySource.value = static_cast<uint8_t>(GroovePuterPerformance::ArpPitchSource::Harmony);
    assert(intentConfigured.dispatchIntent(selectHarmonySource));
    assert(intentConfigured.arpPitchSource() == GroovePuterPerformance::ArpPitchSource::Harmony);
    PerformanceIntent selectRestart{};
    selectRestart.kind = PerformanceIntentKind::SelectHarmonyChangePolicy;
    selectRestart.value = static_cast<uint8_t>(GroovePuterPerformance::HarmonyChangePolicy::Restart);
    assert(intentConfigured.dispatchIntent(selectRestart));
    assert(intentConfigured.harmonyChangePolicy() == GroovePuterPerformance::HarmonyChangePolicy::Restart);
    PerformanceIntent enableArp{};
    enableArp.kind = PerformanceIntentKind::ToggleArp;
    enableArp.value = 1;
    assert(intentConfigured.dispatchIntent(enableArp));
    assert(intentConfigured.arpeggiatorEnabled());
    PerformanceIntent enableLatch{};
    enableLatch.kind = PerformanceIntentKind::ToggleLatch;
    enableLatch.value = 1;
    assert(intentConfigured.dispatchIntent(enableLatch));
    assert(intentConfigured.latchEnabled());
    PerformanceIntent invalidRole = selectLead;
    invalidRole.value = static_cast<uint8_t>(GroovePuterPerformance::PerformanceRole::Count);
    assert(!intentConfigured.dispatchIntent(invalidRole));

    PerformanceKeyboard qwertyIntentOwner(router);
    sink.clear();
    uint8_t qwertyNote = 0;
    assert(qwertyIntentOwner.noteForKey('s', qwertyNote));
    const PerformanceInputIdentity qwertyKey{0, 0, 0, static_cast<uint8_t>('s')};
    const PerformanceIntent qwertyPlay{
        PerformanceIntentKind::PlayNote, qwertyKey, qwertyNote, 93};
    assert(qwertyIntentOwner.dispatchIntent(qwertyPlay));
    assert(sink.events.size() == 1 && sink.events[0].type == MusicalEventType::NoteOn &&
           sink.events[0].note == qwertyNote);
    const PerformanceIntent qwertyRelease{
        PerformanceIntentKind::ReleaseNote, qwertyKey, qwertyNote, 0};
    assert(qwertyIntentOwner.dispatchIntent(qwertyRelease));
    assert(sink.events.size() == 2 && sink.events[1].type == MusicalEventType::NoteOff &&
           sink.events[1].note == qwertyNote);

    router.removeSink(sink);
    assert(router.sinkCount() == 0);
    return 0;
}
