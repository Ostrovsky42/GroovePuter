#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "src/input/midi_input_router.h"

namespace {
class CaptureSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        assert(count < 64u);
        events[count++] = event;
    }
    MusicalEvent events[64]{};
    std::size_t count{0};
};

NormalizedMidiInputMessage note(uint8_t status, uint8_t pitch, uint8_t velocity,
                                MidiInputSessionId session = 1u) {
    NormalizedMidiInputMessage m{};
    assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        status, pitch, velocity, 1u, session, 10u, m));
    return m;
}

void assertEvent(const MusicalEvent& e, MusicalEventType type,
                 uint8_t lane, uint8_t noteValue, uint8_t velocity) {
    assert(e.type == type);
    assert(e.source == MusicalEventSource::MidiInput);
    assert(e.target == MusicalEventTarget::Drums);
    assert(e.channel == lane);
    assert(e.note == noteValue);
    assert(e.velocity == velocity);
}
}

int main() {
    MusicalEventRouter fanout;
    CaptureSink sink;
    assert(fanout.addSink(sink));
    MidiInputRouter input(fanout);

    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Drums;
    assert(input.setConfig(config));

    struct Mapping { uint8_t note; uint8_t lane; };
    constexpr Mapping mappings[] = {
        {36u, 0u}, {38u, 1u}, {42u, 2u}, {46u, 3u},
        {43u, 4u}, {47u, 5u}, {37u, 6u}, {39u, 7u},
    };

    for (const auto& mapping : mappings) {
        const std::size_t before = sink.count;
        assert(input.handle(note(0x99u, mapping.note, 100u)));
        assertEvent(sink.events[before], MusicalEventType::NoteOn,
                    mapping.lane, mapping.note, 100u);
        assert(input.handle(note(0x89u, mapping.note, 17u)));
        assertEvent(sink.events[before + 1u], MusicalEventType::NoteOff,
                    mapping.lane, mapping.note, 17u);
    }

    const std::size_t beforeUnmapped = sink.count;
    assert(!input.handle(note(0x99u, 40u, 90u)));
    assert(sink.count == beforeUnmapped);
    assert(input.diagnostics().unmappedDrumNotes == 1u);

    // Config cleanup releases the retained logical lane, not the new target.
    assert(input.handle(note(0x99u, 36u, 110u)));
    const std::size_t beforeChange = sink.count;
    assert(input.setTarget(MidiInputTarget::SynthA));
    assertEvent(sink.events[beforeChange], MusicalEventType::NoteOff,
                0u, 36u, 0u);
    assert(input.activeNoteCount() == 0u);

    std::cout << "0.9.10 R4 GM drum input mapping: PASS router="
              << sizeof(MidiInputRouter) << " bytes\n";
    return 0;
}
