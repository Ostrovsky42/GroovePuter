#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/usb_midi_output.h"

namespace {

enum class PacketKind : uint8_t {
    NoteOn,
    NoteOff,
    ControlChange,
};

struct Packet {
    PacketKind kind;
    uint8_t channel;
    uint8_t data1;
    uint8_t data2;
};

class FakeTransport final : public IUsbMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return true; }

    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({PacketKind::NoteOn, channel, note, velocity});
        return true;
    }

    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({PacketKind::NoteOff, channel, note, velocity});
        return true;
    }

    bool sendControlChange(uint8_t channel,
                           uint8_t controller,
                           uint8_t value) override {
        packets.push_back({PacketKind::ControlChange, channel, controller, value});
        return true;
    }

    void flush() override {}

    void clear() { packets.clear(); }

    std::vector<Packet> packets;
};

MusicalEvent event(MusicalEventType type,
                   MusicalEventSource source,
                   MusicalEventTarget target,
                   uint8_t note = 0,
                   uint8_t velocity = 0) {
    return MusicalEvent{type, source, target, 0, note, velocity};
}

void expectCc123(const Packet& packet, uint8_t channel) {
    assert(packet.kind == PacketKind::ControlChange);
    assert(packet.channel == channel);
    assert(packet.data1 == 123);
    assert(packet.data2 == 0);
}

void panicRecoversAChannelEvenWhenLocalOwnershipIsAlreadyEmpty() {
    FakeTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    // This models the failure we need panic to recover from: the remote synth
    // may still sound a note while UsbMidiOutput no longer has a lane/table
    // owner for it. Per-note cleanup has nothing to iterate, so the panic must
    // still provide a channel-wide terminal recovery command.
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));

    assert(transport.packets.size() == 1);
    expectCc123(transport.packets[0], 7);
}

void channelWideRecoveryWaitsForAnUnrelatedKnownOwner() {
    FakeTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    // Pattern and live Performance intentionally share CH8. A Pattern panic is
    // target/source scoped and must not use CC123 while the live owner remains.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA,
                                    60,
                                    90));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthA,
                                    60,
                                    110));
    assert(output.wireOwnerCount(7, 60) == 2);

    transport.clear();
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(output.wireOwnerCount(7, 60) == 1);
    assert(transport.packets.empty());

    // Once the unrelated owner leaves, the deferred channel recovery becomes
    // safe. The exact NoteOff happens first, then CC123 clears any forgotten
    // remote state that the sparse owner table cannot name.
    output.handleMusicalEvent(event(MusicalEventType::NoteOff,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthA,
                                    60));
    assert(output.wireOwnerCount(7, 60) == 0);
    assert(transport.packets.size() == 2);
    assert(transport.packets[0].kind == PacketKind::NoteOff);
    assert(transport.packets[0].channel == 7);
    assert(transport.packets[0].data1 == 60);
    expectCc123(transport.packets[1], 7);
}

}  // namespace

int main() {
    panicRecoversAChannelEvenWhenLocalOwnershipIsAlreadyEmpty();
    channelWideRecoveryWaitsForAnUnrelatedKnownOwner();
    return 0;
}
