#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/usb_midi_output.h"

namespace {

enum class PacketType : uint8_t { NoteOn, NoteOff, ControlChange };

struct Packet {
    PacketType type;
    uint8_t channel;
    uint8_t data1;
    uint8_t data2;
};

class FakeUsbMidiTransport final : public IUsbMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return mounted_; }

    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mounted_) return false;
        packets.push_back({PacketType::NoteOn, channel, note, velocity});
        return true;
    }

    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mounted_) return false;
        packets.push_back({PacketType::NoteOff, channel, note, velocity});
        return true;
    }

    bool sendControlChange(uint8_t channel,
                           uint8_t controller,
                           uint8_t value) override {
        if (!mounted_) return false;
        packets.push_back(
            {PacketType::ControlChange, channel, controller, value});
        return true;
    }

    void flush() override {}

    bool mounted_{true};
    std::vector<Packet> packets;
};

void reconnectReleasesNonSmfNotesOnTheirOldChannel() {
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    const MusicalEvent noteOn{
        MusicalEventType::NoteOn,
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthA,
        0,
        60,
        100,
    };
    output.handleMusicalEvent(noteOn);
    assert(transport.packets.size() == 1);
    assert(transport.packets[0].type == PacketType::NoteOn);
    assert(transport.packets[0].channel == 7);
    assert(output.wireOwnerCount(7, 60) == 1);

    transport.packets.clear();
    transport.mounted_ = false;
    output.pollConnection();
    assert(output.wireOwnerCount(7, 60) == 0);
    assert(transport.packets.empty());

    transport.mounted_ = true;
    output.pollConnection();

    assert(transport.packets.size() == 1);
    assert(transport.packets[0].type == PacketType::ControlChange);
    assert(transport.packets[0].channel == 7);
    assert(transport.packets[0].data1 == 123);
    assert(transport.packets[0].data2 == 0);
}

}  // namespace

int main() {
    reconnectReleasesNonSmfNotesOnTheirOldChannel();
    return 0;
}
