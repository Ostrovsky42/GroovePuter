#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_pattern_startup_routes.h"
#include "src/midi/midi_performance_route_projection.h"
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
    bool mounted() const override { return true; }
    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({PacketType::NoteOn, channel, note, velocity});
        return true;
    }
    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({PacketType::NoteOff, channel, note, velocity});
        return true;
    }
    bool sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
        packets.push_back({PacketType::ControlChange, channel, controller, value});
        return true;
    }
    void flush() override {}
    std::vector<Packet> packets;
};

MusicalEvent performanceEvent(MusicalEventType type,
                              MusicalEventTarget target,
                              uint8_t channel,
                              uint8_t note,
                              uint8_t velocity = 100,
                              MusicalEventSource source =
                                  MusicalEventSource::PerformanceKeyboard) {
    return MusicalEvent{type, source, target, channel, note, velocity};
}

void expect(const Packet& packet,
            PacketType type,
            uint8_t channel,
            uint8_t data1) {
    assert(packet.type == type);
    assert(packet.channel == channel);
    assert(packet.data1 == data1);
}

void publishProfile(GroovePuterMidi::MidiDeviceProfile profile) {
    GroovePuterMidi::publishMidiPatternStartupRoutes(
        GroovePuterMidi::makeDefaultMidiOutputSettings(profile));
}

void testSeqtrak() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 7);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthB) == 8);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::Dx) == 9);

    output.handleMusicalEvent(performanceEvent(
        MusicalEventType::NoteOn, MusicalEventTarget::SynthA, 0, 64));
    assert(transport.packets.size() == 2);
    expect(transport.packets[0], PacketType::ControlChange, 7,
           UsbMidiOutput::kSeqtrakMonoPolyController);
    expect(transport.packets[1], PacketType::NoteOn, 7, 64);

    transport.packets.clear();
    for (uint8_t lane = 0; lane < 7; ++lane) {
        output.handleMusicalEvent(performanceEvent(
            MusicalEventType::NoteOn, MusicalEventTarget::Drums, lane, 60));
        assert(transport.packets.size() == 1);
        expect(transport.packets[0], PacketType::NoteOn, lane, 60);
        output.handleMusicalEvent(performanceEvent(
            MusicalEventType::NoteOff, MusicalEventTarget::Drums, lane, 60, 0));
        assert(transport.packets.size() == 2);
        expect(transport.packets[1], PacketType::NoteOff, lane, 60);
        transport.packets.clear();
    }
}

void testGeneralMidi() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::GeneralMidi);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 0);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthB) == 1);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::Dx) == 2);

    output.handleMusicalEvent(performanceEvent(
        MusicalEventType::NoteOn, MusicalEventTarget::Dx, 0, 67));
    assert(transport.packets.size() == 1);
    expect(transport.packets[0], PacketType::NoteOn, 2, 67);
    for (const Packet& packet : transport.packets) {
        assert(packet.type != PacketType::ControlChange);
    }

    transport.packets.clear();
    constexpr uint8_t kNotes[7] = {36, 38, 42, 46, 43, 47, 37};
    for (uint8_t lane = 0; lane < 7; ++lane) {
        output.handleMusicalEvent(performanceEvent(
            MusicalEventType::NoteOn, MusicalEventTarget::Drums, lane, 60));
        assert(transport.packets.size() == 1);
        expect(transport.packets[0], PacketType::NoteOn, 9, kNotes[lane]);
        output.handleMusicalEvent(performanceEvent(
            MusicalEventType::NoteOff, MusicalEventTarget::Drums, lane, 60, 0));
        assert(transport.packets.size() == 2);
        expect(transport.packets[1], PacketType::NoteOff, 9, kNotes[lane]);
        transport.packets.clear();
    }

    // Shared wire ownership remains scoped when live GM Kick and SMF both own
    // CH10/N36. Releasing Performance must not silence the SMF owner.
    assert(output.handleSmfNoteOn(9, 36, 70));
    output.handleMusicalEvent(performanceEvent(
        MusicalEventType::NoteOn, MusicalEventTarget::Drums, 0, 60));
    assert(output.wireOwnerCount(9, 36) == 2);
    const std::size_t beforeOff = transport.packets.size();
    output.handleMusicalEvent(performanceEvent(
        MusicalEventType::NoteOff, MusicalEventTarget::Drums, 0, 60, 0));
    assert(transport.packets.size() == beforeOff);
    assert(output.wireOwnerCount(9, 36) == 1);
    assert(output.handleSmfNoteOff(9, 36));
    assert(output.wireOwnerCount(9, 36) == 0);
}

void testGeneric() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::GenericMidi);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(performanceEvent(
        MusicalEventType::NoteOn, MusicalEventTarget::SynthA, 0, 60));
    assert(transport.packets.size() == 1);
    expect(transport.packets[0], PacketType::NoteOn, 0, 60);
    assert(transport.packets[0].type != PacketType::ControlChange);

    transport.packets.clear();
    output.handleMusicalEvent(performanceEvent(
        MusicalEventType::NoteOn, MusicalEventTarget::Drums, 0, 60));
    assert(transport.packets.empty());
}

void testCustomFallsBackWithoutVendorControl() {
    auto settings = GroovePuterMidi::makeDefaultMidiOutputSettings(
        GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    settings.profile = GroovePuterMidi::MidiDeviceProfile::Custom;
    GroovePuterMidi::publishMidiPatternStartupRoutes(settings);

    const auto projection = GroovePuterMidi::projectMidiPerformanceRoutes(settings);
    assert(!projection.complete);
    assert(projection.receiverModeControl ==
           GroovePuterMidi::MidiReceiverModeControl::None);

    FakeUsbMidiTransport transport;
    UsbMidiOutput output(
        transport,
        UsbMidiRouteConfig{4, 7, 8, true, true, 6, 9, 8});
    assert(output.begin());
    output.pollConnection();
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 4);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthB) == 6);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::Dx) == 8);

    output.handleMusicalEvent(performanceEvent(
        MusicalEventType::NoteOn, MusicalEventTarget::SynthA, 0, 62));
    assert(transport.packets.size() == 1);
    expect(transport.packets[0], PacketType::NoteOn, 4, 62);
    assert(transport.packets[0].type != PacketType::ControlChange);
}

}  // namespace

int main() {
    testSeqtrak();
    testGeneralMidi();
    testGeneric();
    testCustomFallsBackWithoutVendorControl();
    return 0;
}
