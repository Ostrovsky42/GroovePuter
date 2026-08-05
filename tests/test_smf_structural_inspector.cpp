#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/midi/smf_structural_inspector.h"

using namespace GroovePuterMidi;

namespace {
SmfEvent note(uint32_t tick, SmfEventKind kind, uint8_t channel,
              uint8_t pitch, uint8_t velocity = 100) {
    SmfEvent event{};
    event.tick = tick;
    event.kind = kind;
    event.channel = channel;
    event.data1 = pitch;
    event.data2 = velocity;
    return event;
}

SmfEvent program(uint32_t tick, uint8_t channel, uint8_t value) {
    SmfEvent event{};
    event.tick = tick;
    event.kind = SmfEventKind::ProgramChange;
    event.channel = channel;
    event.data1 = value;
    return event;
}

SmfEvent marker(uint32_t tick) {
    SmfEvent event{};
    event.tick = tick;
    event.kind = SmfEventKind::Tempo;
    event.value = 500000;
    return event;
}

void addStraightSixteenths(SmfStructuralInspectorState& state,
                           uint16_t track,
                           uint32_t firstBar,
                           uint32_t bars,
                           bool humanizedVelocity) {
    for (uint32_t bar = 0; bar < bars; ++bar) {
        for (uint32_t step = 0; step < 16; ++step) {
            const uint32_t on = (firstBar + bar) * 384u + step * 24u;
            const uint8_t velocity = humanizedVelocity
                ? static_cast<uint8_t>(48u + ((bar * 17u + step * 11u) % 72u))
                : 100u;
            const uint8_t pitch = static_cast<uint8_t>(36u + (step & 1u));
            state.observe(track, note(on, SmfEventKind::NoteOn, 1, pitch, velocity));
            state.observe(track, note(on + 12u, SmfEventKind::NoteOff, 1, pitch, 0));
        }
    }
}

void addSwungSixteenths(SmfStructuralInspectorState& state,
                        uint16_t track,
                        uint32_t firstBar,
                        uint32_t bars) {
    for (uint32_t bar = 0; bar < bars; ++bar) {
        const uint32_t barStart = (firstBar + bar) * 384u;
        for (uint32_t pair = 0; pair < 8; ++pair) {
            const uint32_t pairStart = barStart + pair * 48u;
            const uint8_t pitch = static_cast<uint8_t>(36u + (pair & 1u));
            state.observe(track,
                          note(pairStart, SmfEventKind::NoteOn, 1, pitch));
            state.observe(track,
                          note(pairStart + 8u, SmfEventKind::NoteOff,
                               1, pitch, 0));
            state.observe(track,
                          note(pairStart + 32u, SmfEventKind::NoteOn,
                               1, pitch));
            state.observe(track,
                          note(pairStart + 40u, SmfEventKind::NoteOff,
                               1, pitch, 0));
        }
    }
}
}  // namespace

int main() {
    const uint32_t generation = smfBeginSessionOpen();
    assert(generation != 0u);
    assert(smfCompleteSessionOpen(generation));

    {
        SmfTrackInspectorState state;
        state.reset(32, 64);
        state.setName(11, "Custom Bass Name");
        state.observe(11, program(0, 1, 33));
        state.observe(11, note(24, SmfEventKind::NoteOn, 1, 40));
        state.freeze();
        const auto snapshot = state.snapshot();
        assert(snapshot.generation == generation);
        assert(snapshot.trackCount == 32);
        assert(snapshot.declaredTrackCount == 64);
        assert(snapshot.tracksTruncated());
        assert(snapshot.tracks[11].audible());
        assert(snapshot.tracks[11].primaryChannel() == 1);
        assert(snapshot.tracks[11].firstProgram == 33);
        assert(snapshot.tracks[11].hasName());
        assert(std::strcmp(snapshot.tracks[11].name, "Finger Bass") == 0);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 4);
        state.observe(0, marker(0));
        addStraightSixteenths(state, 2, 0, 4, false);
        state.observe(0, marker(4u * 384u - 1u));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.generation == generation);
        assert(snapshot.layerCount == 1);
        assert(snapshot.layers[0].trackIndex == 2);
        assert(snapshot.layers[0].gridDenominator == 16);
        assert(snapshot.layers[0].swingPercent == 50);
        assert(snapshot.layers[0].loopBars == 1);
        assert(snapshot.layers[0].motion == SmfStructuralMotion::Low);
        assert(snapshot.layers[0].role == SmfStructuralRole::Bass);
        assert(snapshot.layers[0].notesPerBarX10 == 160);
        assert(snapshot.layers[0].activePermille >= 499);
        assert(snapshot.layers[0].activePermille <= 501);
        assert(snapshot.layers[0].noteCount == 64u);
        assert(snapshot.layers[0].form[0] > 0u);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 4);
        addSwungSixteenths(state, 2, 0, 4);
        state.observe(0, marker(4u * 384u - 1u));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layerCount == 1);
        assert(snapshot.layers[0].gridDenominator == 16);
        assert(snapshot.layers[0].swingPercent >= 65);
        assert(snapshot.layers[0].swingPercent <= 67);
        assert(snapshot.layers[0].loopBars == 1);
        assert(snapshot.layers[0].motion == SmfStructuralMotion::Low);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 4);
        addStraightSixteenths(state, 2, 0, 4, true);
        state.observe(0, marker(4u * 384u - 1u));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layerCount == 1);
        assert(snapshot.layers[0].gridDenominator == 16);
        assert(snapshot.layers[0].swingPercent == 50);
        assert(snapshot.layers[0].loopBars == 1);
        assert(snapshot.layers[0].motion == SmfStructuralMotion::Low);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 2);
        const uint8_t pitches[] = {60u, 64u, 67u};
        for (uint8_t pitch : pitches) {
            state.observe(1, note(0, SmfEventKind::NoteOn, 2, pitch));
        }
        for (uint8_t pitch : pitches) {
            state.observe(1, note(360, SmfEventKind::NoteOff, 2, pitch, 0));
        }
        state.observe(0, marker(383));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layerCount == 1);
        assert(snapshot.layers[0].notesPerBarX10 == 30);
        assert(snapshot.layers[0].activePermille >= 935);
        assert(snapshot.layers[0].role == SmfStructuralRole::Pad);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 12);
        for (uint16_t track = 11; track > 1; --track) {
            state.observe(track, note(track, SmfEventKind::NoteOn, 0, 60));
        }
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layerCount == 9);
        for (uint8_t i = 1; i < snapshot.layerCount; ++i) {
            assert(snapshot.layers[i - 1].trackIndex < snapshot.layers[i].trackIndex);
        }
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 3);
        for (uint32_t bar = 40; bar < 64; ++bar) {
            const uint32_t on = bar * 384u;
            state.observe(2, note(on, SmfEventKind::NoteOn, 0, 48));
            state.observe(2, note(on + 96u, SmfEventKind::NoteOff, 0, 48, 0));
        }
        state.observe(0, marker(64u * 384u - 1u));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layers[0].notesPerBarX10 == 10);
        assert(snapshot.layers[0].activePermille >= 249);
        assert(snapshot.layers[0].activePermille <= 251);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 2);
        for (uint32_t bar = 0; bar < 8; ++bar) {
            const uint8_t pitch = static_cast<uint8_t>(48u + bar);
            state.observe(1, note(bar * 384u, SmfEventKind::NoteOn, 0, pitch));
            state.observe(1, note(bar * 384u + 24u,
                                  SmfEventKind::NoteOff, 0, pitch, 0));
        }
        state.observe(0, marker(8u * 384u - 1u));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layers[0].loopBars == 0);
    }

    {
        constexpr uint32_t kDivision = 32767u;
        constexpr uint32_t kBarTicks = kDivision * 4u;
        SmfStructuralInspectorState state;
        state.reset(static_cast<uint16_t>(kDivision), 1);
        state.observe(0, note(0, SmfEventKind::NoteOn, 0, 60));
        state.observe(0, note(kBarTicks - 1u,
                              SmfEventKind::NoteOff, 0, 60, 0));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layers[0].activePermille >= 999);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 1);
        state.observe(0, note(256u * 384u, SmfEventKind::NoteOn, 0, 60));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.partial);
        assert(snapshot.analyzedBars == 256u);
    }

    static_assert(sizeof(SmfTrackInspectorState) <= 136,
                  "track metadata must fit the fixed-DRAM budget");
    static_assert(sizeof(SmfStructuralInspectorState) <= 2048,
                  "structural state must fit the bounded arrangement budget");
    return 0;
}