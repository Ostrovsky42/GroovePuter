#include <cassert>
#include <cstdint>

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
}

int main() {
    {
        SmfStructuralInspectorState state;
        state.reset(96, 4);
        // Conductor events do not create a layer.
        SmfEvent tempo{};
        tempo.kind = SmfEventKind::Tempo;
        tempo.tick = 0;
        tempo.value = 500000;
        state.observe(0, tempo);

        // Four identical bars on physical track 2, straight sixteenth bass.
        for (uint32_t bar = 0; bar < 4; ++bar) {
            for (uint32_t step = 0; step < 16; ++step) {
                const uint32_t on = bar * 384 + step * 24;
                state.observe(2, note(on, SmfEventKind::NoteOn, 1, 36 + (step & 1u)));
                state.observe(2, note(on + 12, SmfEventKind::NoteOff, 1, 36 + (step & 1u), 0));
            }
        }
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layerCount == 1);
        assert(snapshot.layers[0].trackIndex == 2);
        assert(snapshot.layers[0].gridDenominator == 16);
        assert(snapshot.layers[0].loopBars == 1);
        assert(snapshot.layers[0].role == SmfStructuralRole::Bass);
        assert(snapshot.layers[0].notesPerBarX10 >= 150);
        assert(snapshot.layers[0].activePermille > 0);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 2);
        // A long chord has low note density but high active density.
        const uint8_t pitches[] = {60u, 64u, 67u};
        for (uint8_t pitch : pitches) {
            state.observe(1, note(0, SmfEventKind::NoteOn, 2, pitch));
        }
        for (uint8_t pitch : pitches) {
            state.observe(1, note(360, SmfEventKind::NoteOff, 2, pitch, 0));
        }
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.layerCount == 1);
        assert(snapshot.layers[0].notesPerBarX10 == 30);
        assert(snapshot.layers[0].activePermille >= 900);
        assert(snapshot.layers[0].role == SmfStructuralRole::Pad);
    }

    {
        SmfStructuralInspectorState state;
        state.reset(96, 12);
        // First nine audible physical tracks are retained and sorted.
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
        state.reset(96, 1);
        state.observe(0, note(64u * 384u, SmfEventKind::NoteOn, 0, 60));
        state.finalize();
        const auto snapshot = state.snapshot();
        assert(snapshot.partial);
        assert(snapshot.analyzedBars == 64);
    }

    return 0;
}
