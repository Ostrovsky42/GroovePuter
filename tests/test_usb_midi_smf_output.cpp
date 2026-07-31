#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/usb_midi_output.h"

namespace {
enum class Kind : uint8_t { On, Off };
struct Packet { Kind kind; uint8_t channel; uint8_t note; uint8_t velocity; };

class FakeTransport final : public IUsbMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return mountedState; }
    bool sendNoteOn(uint8_t ch, uint8_t note, uint8_t vel) override {
        if (!mountedState || !sendOk) return false;
        packets.push_back({Kind::On, ch, note, vel});
        return true;
    }
    bool sendNoteOff(uint8_t ch, uint8_t note, uint8_t vel) override {
        if (!mountedState || !sendOk) return false;
        packets.push_back({Kind::Off, ch, note, vel});
        return true;
    }
    void flush() override {}

    bool mountedState{true};
    bool sendOk{true};
    std::vector<Packet> packets;
};

MusicalEvent liveEvent(MusicalEventType type, uint8_t note) {
    return MusicalEvent{
        type,
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA,
        0,
        note,
        100,
    };
}
}

int main() {
    FakeTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    // SMF is polyphonic and keeps the source channel unchanged.
    assert(output.handleSmfNoteOn(2, 60, 90));
    assert(output.handleSmfNoteOn(2, 64, 80));
    assert(transport.packets.size() == 2);
    assert(transport.packets[0].kind == Kind::On && transport.packets[0].channel == 2);
    assert(transport.packets[1].kind == Kind::On && transport.packets[1].note == 64);
    assert(output.smfOwnerCount(2, 60) == 1);
    assert(output.smfOwnerCount(2, 64) == 1);

    // Repeated same-note NoteOn is physically emitted to preserve SMF retrigger.
    assert(output.handleSmfNoteOn(2, 60, 100));
    assert(transport.packets.size() == 3);
    assert(output.smfOwnerCount(2, 60) == 2);
    assert(output.wireOwnerCount(2, 60) == 2);

    // First NoteOff removes one overlapping SMF owner but must not cut the note.
    assert(output.handleSmfNoteOff(2, 60));
    assert(transport.packets.size() == 3);
    assert(output.smfOwnerCount(2, 60) == 1);
    assert(output.wireOwnerCount(2, 60) == 1);
    assert(output.handleSmfNoteOff(2, 60));
    assert(transport.packets.size() == 4);
    assert(transport.packets.back().kind == Kind::Off);

    // Close the independent E4 owner before the shared-owner panic scenario.
    assert(output.handleSmfNoteOff(2, 64));
    assert(output.smfOwnerCount(2, 64) == 0);
    assert(output.wireOwnerCount(2, 64) == 0);

    // Player panic must not silence a live/performance owner sharing CH8 note 67.
    output.handleMusicalEvent(liveEvent(MusicalEventType::NoteOn, 67));
    const std::size_t afterLiveOn = transport.packets.size();
    assert(output.handleSmfNoteOn(7, 67, 110));
    assert(output.wireOwnerCount(7, 67) == 2);
    assert(output.releaseAllSmfNotes());
    assert(output.smfOwnerCount(7, 67) == 0);
    assert(output.wireOwnerCount(7, 67) == 1);
    // SMF retrigger produced an On, but panic produced no Off while live owns it.
    assert(transport.packets.size() == afterLiveOn + 1);

    output.handleMusicalEvent(liveEvent(MusicalEventType::NoteOff, 67));
    assert(output.wireOwnerCount(7, 67) == 0);
    assert(transport.packets.back().kind == Kind::Off);

    // Failed final SMF NoteOff keeps ownership for a later retry.
    assert(output.handleSmfNoteOn(5, 72, 100));
    transport.sendOk = false;
    assert(!output.handleSmfNoteOff(5, 72));
    assert(output.smfOwnerCount(5, 72) == 1);
    assert(output.wireOwnerCount(5, 72) == 1);
    transport.sendOk = true;
    assert(output.handleSmfNoteOff(5, 72));
    assert(output.smfOwnerCount(5, 72) == 0);

    // Disconnect clears local ownership and stale notes are never replayed.
    assert(output.handleSmfNoteOn(4, 55, 90));
    transport.mountedState = false;
    output.pollConnection();
    assert(output.smfOwnerCount(4, 55) == 0);
    assert(output.wireOwnerCount(4, 55) == 0);
    transport.mountedState = true;
    output.pollConnection();

    return 0;
}
