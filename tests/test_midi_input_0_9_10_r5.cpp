#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "src/input/midi_input_router.h"

namespace {
class CaptureSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        assert(count < 128u);
        events[count++] = event;
    }
    MusicalEvent events[128]{};
    std::size_t count{0};
};

NormalizedMidiInputMessage note(uint8_t status, uint8_t pitch, uint8_t velocity,
                                MidiInputTransportId transport,
                                MidiInputSessionId session) {
    NormalizedMidiInputMessage m{};
    assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        status, pitch, velocity, transport, session, 42u, m));
    return m;
}
}

int main() {
    MusicalEventRouter fanout;
    CaptureSink sink;
    assert(fanout.addSink(sink));
    MidiInputRouter input(fanout);

    MidiInputRoutingConfig cfg{};
    cfg.enabled = true;
    cfg.target = MidiInputTarget::SynthA;
    assert(input.setConfig(cfg));

    constexpr MidiInputTransportId usb = 1u;
    constexpr MidiInputTransportId other = 2u;
    constexpr MidiInputSessionId session1 = 10u;
    constexpr MidiInputSessionId session2 = 11u;

    // Distinct sessions can own distinct pitches simultaneously in the bounded owner table.
    assert(input.handle(note(0x90u, 48u, 100u, usb, session1)));
    assert(input.handle(note(0x90u, 52u, 100u, usb, session2)));
    assert(input.handle(note(0x90u, 55u, 100u, other, session1)));
    assert(input.activeNoteCount() == 3u);

    const std::size_t beforeCleanup = sink.count;
    assert(input.releaseSession(usb, session1) == 1u);
    assert(input.activeNoteCount() == 2u);
    assert(sink.count == beforeCleanup + 1u);
    assert(sink.events[beforeCleanup].type == MusicalEventType::NoteOff);
    assert(sink.events[beforeCleanup].note == 48u);

    // A stale NoteOff from the retired session is orphaned and cannot kill session2.
    assert(!input.handle(note(0x80u, 48u, 0u, usb, session1)));
    assert(input.activeNoteCount() == 2u);
    assert(input.handle(note(0x80u, 52u, 0u, usb, session2)));
    assert(input.activeNoteCount() == 1u);

    // Invalid lifetime identities never trigger broad cleanup.
    assert(input.releaseSession(0u, session1) == 0u);
    assert(input.releaseSession(other, 0u) == 0u);
    assert(input.activeNoteCount() == 1u);

    assert(input.releaseSession(other, session1) == 1u);
    assert(input.activeNoteCount() == 0u);
    assert(input.diagnostics().sessionCleanups == 2u);
    assert(input.diagnostics().sessionNotesReleased == 2u);
    assert(input.diagnostics().orphanNoteOffs == 1u);

    // Disable/route/channel mutations remain cleanup-first.
    assert(input.handle(note(0x90u, 60u, 100u, usb, session2)));
    assert(input.setEnabled(false));
    assert(input.activeNoteCount() == 0u);
    assert(input.setEnabled(true));
    assert(input.handle(note(0x90u, 61u, 100u, usb, session2)));
    assert(input.setSingleChannel(3u));
    assert(input.activeNoteCount() == 0u);

    std::cout << "0.9.10 R5 MIDI input lifecycle: PASS router="
              << sizeof(MidiInputRouter) << " bytes\n";
    return 0;
}
