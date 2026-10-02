// External keyboard (Host MIDI IN) through the PERFORM keyboard: absolute pitch, shared held-note
// list, chord / latch / drum behaviour, scoped release.
#include <cassert>
#include <cstdio>
#include <vector>

#include "src/input/performance_keyboard.h"

namespace {
class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override { events.push_back(event); }
    void clear() { events.clear(); }
    int count(MusicalEventType type) const {
        int n = 0;
        for (const auto& e : events) n += e.type == type ? 1 : 0;
        return n;
    }
    bool has(MusicalEventType type, uint8_t note) const {
        for (const auto& e : events) if (e.type == type && e.note == note) return true;
        return false;
    }
    std::vector<MusicalEvent> events;
};
}  // namespace

int main() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));

    // 1. absolute pitch, velocity kept, source is the PERFORM keyboard (same owner as the built-in keys)
    {
        PerformanceKeyboard kb(router);
        sink.clear();
        assert(kb.externalNoteOn(60, 97));
        assert(sink.events.size() == 1);
        assert(sink.events[0].type == MusicalEventType::NoteOn);
        assert(sink.events[0].source == MusicalEventSource::PerformanceKeyboard);
        assert(sink.events[0].note == 60);          // no QWERTY octave / scale offset
        assert(sink.events[0].velocity == 97);
        sink.clear();
        assert(kb.externalNoteOff(60));
        assert(sink.events.size() == 1 && sink.events[0].type == MusicalEventType::NoteOff &&
               sink.events[0].note == 60);
        assert(!kb.externalNoteOff(60));            // already released
    }
    // 2. velocity 0 is a NoteOff
    {
        PerformanceKeyboard kb(router);
        assert(kb.externalNoteOn(64, 90));
        sink.clear();
        assert(kb.externalNoteOn(64, 0));
        assert(sink.count(MusicalEventType::NoteOff) == 1 && sink.has(MusicalEventType::NoteOff, 64));
    }
    // 3. out of range folds by octaves, and the NoteOff finds the same hold
    {
        PerformanceKeyboard kb(router);
        sink.clear();
        assert(kb.externalNoteOn(110, 80));
        assert(sink.events.size() == 1);
        const uint8_t folded = sink.events[0].note;
        assert(folded >= PerformanceKeyboard::kMinNote && folded <= PerformanceKeyboard::kMaxNote);
        assert(folded % 12 == 110 % 12);
        sink.clear();
        assert(kb.externalNoteOff(110));
        assert(sink.has(MusicalEventType::NoteOff, folded));
    }
    // 4. built-in keys and external notes coexist; the built-in key sweep never releases external notes
    {
        PerformanceKeyboard kb(router);
        assert(kb.externalNoteOn(60, 100));
        assert(kb.keyDown('k', 100));
        sink.clear();
        kb.releaseMissingKeys(nullptr, 0);          // no built-in key is pressed any more
        assert(sink.count(MusicalEventType::NoteOff) >= 1);
        assert(!sink.has(MusicalEventType::NoteOff, 60));   // external note 60 still held
        assert(kb.externalNoteOff(60));
        assert(sink.has(MusicalEventType::NoteOff, 60));
    }
    // 5. chord mode transforms an external note exactly like a built-in one
    {
        PerformanceKeyboard kb(router);
        kb.setChordMode(PerformanceChordMode::Fifth);
        sink.clear();
        assert(kb.externalNoteOn(60, 100));
        assert(sink.count(MusicalEventType::NoteOn) >= 2);
        assert(sink.has(MusicalEventType::NoteOn, 60) || sink.has(MusicalEventType::NoteOn, 67));
        kb.panic();
    }
    // 6. scoped release: all external notes off, nothing else disturbed
    {
        PerformanceKeyboard kb(router);
        assert(kb.externalNoteOn(60, 100));
        assert(kb.externalNoteOn(64, 100));
        sink.clear();
        kb.releaseAllExternalNotes();
        assert(sink.has(MusicalEventType::NoteOff, 60) && sink.has(MusicalEventType::NoteOff, 64));
        sink.clear();
        kb.releaseAllExternalNotes();               // idempotent
        assert(sink.events.empty());
    }
    // 7. disabled keyboard ignores a new note but a NoteOff for an unknown note is harmless
    {
        PerformanceKeyboard kb(router);
        kb.setEnabled(false);
        sink.clear();
        assert(kb.externalNoteOn(60, 100));
        assert(sink.events.empty());
        assert(!kb.externalNoteOff(60));
    }
    // 8. drum target: external pitch class selects a lane, release by lane
    {
        PerformanceKeyboard kb(router);
        kb.setTarget(MusicalEventTarget::Drums);
        sink.clear();
        assert(kb.externalNoteOn(38, 100));         // pitch class 2 = snare lane
        assert(sink.count(MusicalEventType::NoteOn) == 1);
        assert(sink.events[0].channel == 1);
        sink.clear();
        kb.releaseAllExternalNotes();
        assert(sink.count(MusicalEventType::NoteOff) == 1 && sink.events[0].channel == 1);
    }
    // 9. MONO / POLY apply to an external keyboard exactly like to the built-in keys: MONO emits the
    //    mono PERFORM source, POLY emits the poly source and keeps both pitches sounding.
    {
        PerformanceKeyboard kb(router);
        kb.setVoiceMode(PerformanceVoiceMode::Mono);
        sink.clear();
        assert(kb.externalNoteOn(60, 100));
        assert(kb.externalNoteOn(64, 100));
        assert(sink.events.size() == 2);
        for (const auto& e : sink.events) assert(e.source == MusicalEventSource::PerformanceKeyboard);
        kb.panic();

        kb.setVoiceMode(PerformanceVoiceMode::Poly);
        assert(kb.directPolyphonyEnabled());
        sink.clear();
        assert(kb.externalNoteOn(60, 100));
        assert(kb.externalNoteOn(64, 100));
        assert(sink.count(MusicalEventType::NoteOn) == 2);
        for (const auto& e : sink.events) {
            assert(e.source == MusicalEventSource::PerformanceKeyboardPoly);
        }
        sink.clear();
        assert(kb.externalNoteOff(60));
        assert(sink.has(MusicalEventType::NoteOff, 60) && !sink.has(MusicalEventType::NoteOff, 64));
        kb.panic();
    }
    // 10. The PERFORM velocity setting scales an external keyboard's dynamics (x setting/100) and
    //     keeps their shape: ordering is preserved, 100 is neutral, extremes are clamped to 1..127.
    {
        PerformanceKeyboard kb(router);
        auto velocityOf = [&](uint8_t keyVelocity) {
            sink.clear();
            assert(kb.externalNoteOn(60, keyVelocity));
            assert(sink.events.size() == 1);
            const uint8_t out = sink.events[0].velocity;
            assert(kb.externalNoteOff(60));
            return out;
        };
        assert(kb.velocity() == PerformanceKeyboard::kDefaultVelocity);
        assert(velocityOf(97) == 97);                    // default setting is neutral
        kb.setVelocity(50);
        assert(velocityOf(100) == 50);                   // half
        assert(velocityOf(40) == 20);
        assert(velocityOf(1) == 1);                      // never rounds down to a NoteOff
        assert(velocityOf(20) < velocityOf(60) && velocityOf(60) < velocityOf(120));  // dynamics kept
        kb.setVelocity(PerformanceKeyboard::kMaxVelocity);   // 120 = x1.2
        assert(velocityOf(100) == 120);
        assert(velocityOf(127) == 127);                  // clamped
        kb.setVelocity(PerformanceKeyboard::kMinVelocity);   // 10 = x0.1
        assert(velocityOf(100) == 10);
        assert(velocityOf(127) == 13);
    }
    // 11. Sustain button: LATCH while held (LATCH exists only with ARP on), previous LATCH state
    //     restored on release. Without ARP the button is a harmless no-op.
    {
        PerformanceKeyboard kb(router);
        kb.setArpeggiatorEnabled(true);
        assert(!kb.latchEnabled());
        kb.externalSustain(true);
        assert(kb.externalSustainDown() && kb.latchEnabled());
        kb.externalSustain(true);                          // repeated press: no new edge
        kb.externalSustain(false);
        assert(!kb.externalSustainDown() && !kb.latchEnabled());

        PerformanceKeyboard manual(router);                // user had LATCH on already
        manual.setArpeggiatorEnabled(true);
        manual.setLatchEnabled(true);
        assert(manual.latchEnabled());
        manual.externalSustain(true);
        manual.externalSustain(false);
        assert(manual.latchEnabled());                     // restored to the user's choice

        PerformanceKeyboard noArp(router);                 // no ARP: nothing to latch
        noArp.externalSustain(true);
        assert(!noArp.latchEnabled());
        noArp.externalSustain(false);
        assert(!noArp.latchEnabled());
    }
    std::puts("PERFORM keyboard external notes: PASS");
    return 0;
}
