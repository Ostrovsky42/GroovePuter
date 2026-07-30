#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/usb_midi_output.h"

namespace {
enum class PacketType : uint8_t { NoteOn, NoteOff };

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
    void clear() { packets.clear(); flushCalls = 0; }

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

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100));
    assert(transport.packets.empty());
    assert(output.status() == UsbMidiStatus::Off);

    assert(output.begin());
    assert(output.status() == UsbMidiStatus::Wait);
    transport.mountedState = true;
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Ready);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 7);
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 7);
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 8);

    // Three independent ownership lanes: live A, pattern A and pattern B.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 48, 90,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 52, 80,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    assert(transport.packets.size() == 3);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 36, 100);
    expectPacket(transport.packets[1], PacketType::NoteOn, 7, 48, 90);
    expectPacket(transport.packets[2], PacketType::NoteOn, 8, 52, 80);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 36);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 48);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    // Pattern A replacement does not cancel live A or pattern B.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 50, 91,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 5);
    expectPacket(transport.packets[3], PacketType::NoteOff, 7, 48, 0);
    expectPacket(transport.packets[4], PacketType::NoteOn, 7, 50, 91);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 36);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 48, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 5);
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 6);
    expectPacket(transport.packets[5], PacketType::NoteOff, 7, 50, 0);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 36);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0));
    assert(transport.packets.size() == 7);
    expectPacket(transport.packets[6], PacketType::NoteOff, 7, 36, 0);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    assert(transport.packets.size() == 8);
    expectPacket(transport.packets[7], PacketType::NoteOff, 8, 52, 0);

    // Unsupported sources and live Synth B remain outside this stage.
    transport.clear();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 100,
                                    MusicalEventSource::Arpeggiator));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 61, 100,
                                    MusicalEventSource::MidiInput));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 62, 100,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthB));
    assert(transport.packets.empty());

    // Router fan-out remains independent.
    MusicalEventRouter router;
    CountingSink internalLikeSink;
    assert(router.addSink(internalLikeSink));
    assert(router.addSink(output));
    router.route(event(MusicalEventType::NoteOn, 55, 88,
                       MusicalEventSource::PatternPlayer,
                       MusicalEventTarget::SynthA));
    assert(internalLikeSink.count == 1);
    assert(transport.packets.size() == 1);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 55, 88);

    // Queue-full NoteOff failure retains ownership and prevents layering.
    transport.sendResult = false;
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 57, 77,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 1);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 55);
    transport.sendResult = true;
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 2);
    expectPacket(transport.packets[1], PacketType::NoteOff, 7, 55, 0);

    // Disconnect clears every lane and does not replay stale notes.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 64, 100,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    transport.mountedState = false;
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Wait);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == -1);
    transport.mountedState = true;
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 65, 70,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    expectPacket(transport.packets.back(), PacketType::NoteOn, 8, 65, 70);

    // Global disable releases all currently owned lanes.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 66, 71,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    const std::size_t beforeDisable = transport.packets.size();
    output.setEnabled(false);
    assert(output.status() == UsbMidiStatus::Off);
    assert(transport.packets.size() == beforeDisable + 2);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == -1);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == -1);

    // Invalid route values are bounded to channel 16 (zero-based 15).
    FakeUsbMidiTransport clampedTransport;
    clampedTransport.mountedState = true;
    UsbMidiOutput clampedOutput(clampedTransport,
        UsbMidiRouteConfig{99, 98, 97, true, true});
    assert(clampedOutput.begin());
    clampedOutput.pollConnection();
    clampedOutput.handleMusicalEvent(event(MusicalEventType::NoteOn, 62, 64,
                                            MusicalEventSource::PatternPlayer,
                                            MusicalEventTarget::SynthB));
    assert(clampedTransport.packets.size() == 1);
    expectPacket(clampedTransport.packets[0], PacketType::NoteOn, 15, 62, 64);

    // Pattern routes can be disabled independently at construction.
    FakeUsbMidiTransport liveOnlyTransport;
    liveOnlyTransport.mountedState = true;
    UsbMidiOutput liveOnly(liveOnlyTransport,
        UsbMidiRouteConfig{7, 7, 8, true, false});
    assert(liveOnly.begin());
    liveOnly.pollConnection();
    liveOnly.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 100,
                                      MusicalEventSource::PatternPlayer,
                                      MusicalEventTarget::SynthA));
    assert(liveOnlyTransport.packets.empty());

    return 0;
}
