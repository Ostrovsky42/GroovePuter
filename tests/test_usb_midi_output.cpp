#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/usb_midi_output.h"

namespace {
enum class PacketType : uint8_t {
    NoteOn,
    NoteOff,
};

struct Packet {
    PacketType type;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

class FakeUsbMidiTransport final : public IUsbMidiTransport {
public:
    bool begin() override {
        ++beginCalls;
        return beginResult;
    }

    bool mounted() const override { return mountedState; }

    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mountedState || !sendResult) return false;
        packets.push_back({PacketType::NoteOn, channel, note, velocity});
        return true;
    }

    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mountedState || !sendResult) return false;
        packets.push_back({PacketType::NoteOff, channel, note, velocity});
        return true;
    }

    void flush() override { ++flushCalls; }

    void clear() {
        packets.clear();
        flushCalls = 0;
    }

    bool beginResult{true};
    bool mountedState{false};
    bool sendResult{true};
    int beginCalls{0};
    int flushCalls{0};
    std::vector<Packet> packets;
};

class CountingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent&) override { ++count; }
    int count{0};
};

MusicalEvent event(MusicalEventType type,
                   uint8_t note,
                   uint8_t velocity = 0,
                   MusicalEventSource source = MusicalEventSource::PerformanceKeyboard,
                   MusicalEventTarget target = MusicalEventTarget::SynthA) {
    return MusicalEvent{type, source, target, 0, note, velocity};
}

void expectPacket(const Packet& packet,
                  PacketType type,
                  uint8_t channel,
                  uint8_t note,
                  uint8_t velocity) {
    assert(packet.type == type);
    assert(packet.channel == channel);
    assert(packet.note == note);
    assert(packet.velocity == velocity);
}
}  // namespace

int main() {
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);

    // No USB lifecycle means no packet and no crash.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100));
    assert(transport.packets.empty());
    assert(output.status() == UsbMidiStatus::Off);

    assert(output.begin());
    assert(transport.beginCalls == 1);
    assert(output.status() == UsbMidiStatus::Wait);

    transport.mountedState = true;
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Ready);
    assert(output.synthAChannel() == 7);  // MIDI channel 8 on the wire/UI.

    // Basic NoteOn conversion.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100));
    assert(transport.packets.size() == 1);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 36, 100);
    assert(output.activeNote(MusicalEventTarget::SynthA) == 36);
    assert(output.activeNote(MusicalEventTarget::SynthB) == -1);

    // Monophonic replacement sends NoteOff before the next NoteOn.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 40, 110));
    assert(transport.packets.size() == 3);
    expectPacket(transport.packets[1], PacketType::NoteOff, 7, 36, 0);
    expectPacket(transport.packets[2], PacketType::NoteOn, 7, 40, 110);
    assert(output.activeNote(MusicalEventTarget::SynthA) == 40);

    // An inactive release must not disturb the active external note.
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 36));
    assert(transport.packets.size() == 3);
    assert(output.activeNote(MusicalEventTarget::SynthA) == 40);

    // Restoring the previous held note is another ordered replacement.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 90));
    assert(transport.packets.size() == 5);
    expectPacket(transport.packets[3], PacketType::NoteOff, 7, 40, 0);
    expectPacket(transport.packets[4], PacketType::NoteOn, 7, 36, 90);

    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 36, 12));
    assert(transport.packets.size() == 6);
    expectPacket(transport.packets[5], PacketType::NoteOff, 7, 36, 12);
    assert(output.activeNote(MusicalEventTarget::SynthA) == -1);

    // Target-scoped panic sends one explicit NoteOff and no CC 123 packet.
    transport.clear();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 48, 127));
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0));
    assert(transport.packets.size() == 2);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 48, 127);
    expectPacket(transport.packets[1], PacketType::NoteOff, 7, 48, 0);
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0));
    assert(transport.packets.size() == 2);

    // Only PerformanceKeyboard -> SynthA is in scope for this spike.
    transport.clear();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 50, 100,
                                    MusicalEventSource::PatternPlayer));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 51, 100,
                                    MusicalEventSource::Arpeggiator));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 52, 100,
                                    MusicalEventSource::MidiInput));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 53, 100,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthB));
    assert(transport.packets.empty());

    // Router fan-out remains independent: another sink receives the same event.
    MusicalEventRouter router;
    CountingSink internalLikeSink;
    assert(router.addSink(internalLikeSink));
    assert(router.addSink(output));
    router.route(event(MusicalEventType::NoteOn, 55, 88));
    assert(internalLikeSink.count == 1);
    assert(transport.packets.size() == 1);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 55, 88);

    // Disconnect clears local ownership and never queues stale note state.
    transport.mountedState = false;
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Wait);
    assert(output.activeNote(MusicalEventTarget::SynthA) == -1);
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 55));
    assert(transport.packets.size() == 1);

    transport.mountedState = true;
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 57, 77));
    assert(transport.packets.size() == 2);
    expectPacket(transport.packets[1], PacketType::NoteOn, 7, 57, 77);

    // Runtime disable releases a currently owned note, then becomes inert.
    output.setEnabled(false);
    assert(output.status() == UsbMidiStatus::Off);
    assert(transport.packets.size() == 3);
    expectPacket(transport.packets[2], PacketType::NoteOff, 7, 57, 0);
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 100));
    assert(transport.packets.size() == 3);

    output.setEnabled(true);
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Ready);

    // Invalid route values are bounded to MIDI's 0..15 channel range.
    FakeUsbMidiTransport clampedTransport;
    clampedTransport.mountedState = true;
    UsbMidiOutput clampedOutput(clampedTransport, UsbMidiRouteConfig{99, true});
    assert(clampedOutput.begin());
    clampedOutput.handleMusicalEvent(event(MusicalEventType::NoteOn, 62, 64));
    assert(clampedTransport.packets.size() == 1);
    expectPacket(clampedTransport.packets[0], PacketType::NoteOn, 15, 62, 64);

    // Failed USB initialization leaves the groovebox-side sink safely OFF.
    FakeUsbMidiTransport failedTransport;
    failedTransport.beginResult = false;
    failedTransport.mountedState = true;
    UsbMidiOutput failedOutput(failedTransport);
    assert(!failedOutput.begin());
    assert(failedOutput.status() == UsbMidiStatus::Off);
    failedOutput.handleMusicalEvent(event(MusicalEventType::NoteOn, 64, 100));
    assert(failedTransport.packets.empty());

    return 0;
}
