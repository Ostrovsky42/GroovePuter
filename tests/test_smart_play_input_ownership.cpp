#include <cassert>
#include <cstdint>
#include <vector>

#include "src/input/musical_event_router.h"
#include "src/input/performance_keyboard.h"

#if __has_include("src/input/performance_intent.h")
#include "src/input/performance_intent.h"

namespace {
class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override { events.push_back(event); }
    int count(MusicalEventType type, uint8_t note) const {
        int result = 0;
        for (const MusicalEvent& event : events)
            result += event.type == type && event.note == note ? 1 : 0;
        return result;
    }
    std::vector<MusicalEvent> events;
};

PerformanceInputIdentity input(uint32_t generation, uint8_t source, uint8_t note = 60,
                               uint8_t cable = 0) {
    return PerformanceInputIdentity{generation, source, 0, note, cable};
}
}

int main() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    PerformanceKeyboard keyboard(router);
    keyboard.setVoiceMode(PerformanceVoiceMode::Poly);

    // Cardputer and USB share the same semantic pitch owner. Releasing either
    // source must leave the other source sounding, regardless of release order.
    {
        MusicalEventRouter sharedRouter;
        RecordingSink sharedSink;
        assert(sharedRouter.addSink(sharedSink));
        PerformanceKeyboard sharedKeyboard(sharedRouter);
        sharedKeyboard.setVoiceMode(PerformanceVoiceMode::Poly);
        const PerformanceInputIdentity usbC = input(1, 1, 36);

        assert(sharedKeyboard.keyDown('a', 90)); // Cardputer key maps to MIDI 36.
        assert(sharedKeyboard.externalNoteOn(36, 100, usbC));
        assert(sharedSink.count(MusicalEventType::NoteOn, 36) == 1);
        assert(sharedKeyboard.releaseExternalInput(1, 1) == 1u);
        assert(sharedSink.count(MusicalEventType::NoteOff, 36) == 0);
        assert(sharedKeyboard.keyUp('a'));
        assert(sharedSink.count(MusicalEventType::NoteOff, 36) == 1);

        sharedSink.events.clear();
        assert(sharedKeyboard.externalNoteOn(36, 100, usbC));
        assert(sharedKeyboard.keyDown('a', 90));
        assert(sharedSink.count(MusicalEventType::NoteOn, 36) == 1);
        assert(sharedKeyboard.keyUp('a'));
        assert(sharedSink.count(MusicalEventType::NoteOff, 36) == 0);
        assert(sharedKeyboard.externalNoteOff(36, usbC));
        assert(sharedSink.count(MusicalEventType::NoteOff, 36) == 1);
    }

    const PerformanceInputIdentity usb = input(4, 1);
    const PerformanceInputIdentity uart = input(9, 2);
    assert(keyboard.externalNoteOn(60, 90, usb));
    const std::size_t ownersAfterFirstAttack = keyboard.externalHeldCount();
    assert(keyboard.externalNoteOn(60, 90, usb));
    assert(keyboard.externalHeldCount() == ownersAfterFirstAttack);
    assert(keyboard.externalNoteOn(60, 110, uart));
    assert(sink.count(MusicalEventType::NoteOn, 60) == 1);

    const PerformanceInputIdentity wrongCable = input(4, 1, 60, 1);
    assert(!keyboard.externalNoteOff(60, wrongCable));
    assert(!keyboard.externalNoteOff(60, input(3, 1, 0)));
    assert(keyboard.externalNoteOff(60, usb));
    assert(sink.count(MusicalEventType::NoteOff, 60) == 0);

    // A new session can hold the same physical key; an old session's late NoteOff is inert.
    const PerformanceInputIdentity reconnectedUsb = input(5, 1, 60);
    assert(keyboard.externalNoteOn(60, 100, reconnectedUsb));
    assert(!keyboard.externalNoteOff(60, usb));

    keyboard.releaseExternalInput(1, 5);
    assert(sink.count(MusicalEventType::NoteOff, 60) == 0); // UART still owns this pitch.
    assert(keyboard.externalNoteOff(60, uart));
    assert(sink.count(MusicalEventType::NoteOff, 60) == 1);
    assert(keyboard.externalHeldCount() == 0);

    const PerformanceInputIdentity staleAfterCleanup = input(12, 2, 64);
    assert(keyboard.externalNoteOn(64, 90, staleAfterCleanup));
    keyboard.releaseAllExternalNotes();
    assert(!keyboard.externalNoteOff(64, staleAfterCleanup));

    keyboard.setArpeggiatorEnabled(true);
    keyboard.externalSustain(true, usb);
    keyboard.externalSustain(true, uart);
    assert(keyboard.latchEnabled());
    assert(keyboard.releaseExternalInput(1, 4) == 0);
    assert(keyboard.latchEnabled()); // USB release cannot cancel UART sustain.
    assert(keyboard.releaseExternalInput(2, 9) == 0);
    assert(!keyboard.latchEnabled());

    const PerformanceInputIdentity sustainCh0{31, 1, 0, 0, 0};
    const PerformanceInputIdentity sustainCh1{31, 1, 1, 0, 0};
    keyboard.externalSustain(true, sustainCh0);
    keyboard.externalSustain(true, sustainCh1);
    assert(keyboard.latchEnabled());
    assert(keyboard.releaseExternalChannel(1, 31, 0, 0) == 0);
    assert(keyboard.latchEnabled()); // Channel 1 still owns sustain.
    assert(keyboard.releaseExternalChannel(1, 31, 1, 0) == 0);
    assert(!keyboard.latchEnabled());

    // CC120 scoped to one channel cannot release an identical pitch owned on another channel.
    PerformanceInputIdentity ch0{21, 1, 0, 67, 0};
    PerformanceInputIdentity ch1{21, 1, 1, 67, 0};
    assert(keyboard.externalNoteOn(67, 90, ch0));
    assert(keyboard.externalNoteOn(67, 100, ch1));
    const int noteOffBefore = sink.count(MusicalEventType::NoteOff, 67);
    assert(keyboard.releaseExternalChannel(1, 21, 0, 0) == 1u);
    assert(sink.count(MusicalEventType::NoteOff, 67) == noteOffBefore);
    assert(keyboard.externalNoteOff(67, ch1));
    assert(sink.count(MusicalEventType::NoteOff, 67) == noteOffBefore + 1);

    // Held-note capacity rejects only the incoming owner. Existing physical
    // and external owners remain live; an overflowed external NoteOn cannot
    // leave a phantom identity that later appears releasable.
    {
        MusicalEventRouter capacityRouter;
        RecordingSink capacitySink;
        assert(capacityRouter.addSink(capacitySink));
        PerformanceKeyboard capacity(capacityRouter);
        capacity.setVoiceMode(PerformanceVoiceMode::Poly);
        PerformanceInputIdentity owners[PerformanceKeyboard::kMaxHeldNotes]{};
        for (std::size_t i = 0; i < PerformanceKeyboard::kMaxHeldNotes - 1; ++i) {
            owners[i] = input(100, 1, static_cast<uint8_t>(40 + i));
            assert(capacity.externalNoteOn(owners[i].inputNote, 90, owners[i]));
        }
        assert(capacity.keyDown('a', 100)); // 19th owner
        assert(capacity.heldCount() == PerformanceKeyboard::kMaxHeldNotes);
        const std::size_t eventsBeforeOverflow = capacitySink.events.size();
        assert(capacity.keyDown('s', 100)); // consumed, but cannot steal existing owners
        assert(capacity.heldCount() == PerformanceKeyboard::kMaxHeldNotes);
        assert(capacity.isPhysicalKeyHeld('a'));
        assert(capacitySink.events.size() == eventsBeforeOverflow);

        const auto rejected = input(100, 1, 100);
        assert(!capacity.externalNoteOn(100, 90, rejected));
        assert(capacity.heldCount() == PerformanceKeyboard::kMaxHeldNotes);
        assert(capacity.releaseExternalInput(1, 100) ==
               PerformanceKeyboard::kMaxHeldNotes - 1);
        assert(!capacity.externalNoteOff(100, rejected));
        assert(capacity.isPhysicalKeyHeld('a'));
        assert(capacity.keyUp('a'));
        assert(capacity.heldCount() == 0);
    }

    // Sustain owner-table exhaustion is explicit and bounded. A rejected
    // owner cannot cancel existing sustain; once one slot is freed it can retry.
    {
        MusicalEventRouter sustainRouter;
        RecordingSink sustainSink;
        assert(sustainRouter.addSink(sustainSink));
        PerformanceKeyboard sustain(sustainRouter);
        sustain.setArpeggiatorEnabled(true);
        PerformanceInputIdentity inputs[33]{};
        for (uint32_t i = 0; i < 32; ++i) {
            inputs[i] = PerformanceInputIdentity{
                400u + i, 1, static_cast<uint8_t>(i % 16u), 0,
                static_cast<uint8_t>(i / 16u)};
            assert(sustain.externalSustain(true, inputs[i]));
        }
        inputs[32] = PerformanceInputIdentity{999u, 2, 0, 0, 0};
        assert(sustain.externalSustainDown());
        assert(!sustain.externalSustain(true, inputs[32]));
        assert(sustain.externalSustainOverflowCount() == 1u);
        assert(sustain.externalSustainDown());
        assert(sustain.externalSustain(false, inputs[0]));
        assert(sustain.externalSustain(true, inputs[32]));
        for (const PerformanceInputIdentity& owner : inputs)
            assert(sustain.externalSustain(false, owner));
        assert(!sustain.externalSustainDown());
    }

    PerformanceIntent unsupported{};
    unsupported.kind = static_cast<PerformanceIntentKind>(255);
    assert(!keyboard.dispatchIntent(unsupported));
    assert(keyboard.externalHeldCount() == 0);
    return 0;
}

#else
int main() {
    assert(false && "source-scoped PerformanceKeyboard input ownership is not implemented");
    return 1;
}
#endif
