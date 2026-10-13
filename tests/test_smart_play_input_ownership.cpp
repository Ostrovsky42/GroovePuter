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
