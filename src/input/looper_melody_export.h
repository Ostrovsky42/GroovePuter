#pragma once
#ifndef GROOVEPUTER_LOOPER_MELODY_EXPORT_H
#define GROOVEPUTER_LOOPER_MELODY_EXPORT_H

#include <cstdint>

#include "looper_layer.h"
#include "src/phrase/runtime_phrase_edit.h"
#include "src/phrase/runtime_synth_events.h"

// LOOPER > MELODY: the loop as a Working Melody, played as it was played.
// Pure and allocation-free (the caller owns the 1284-byte buffer); main loop
// only. No Smart Play, no chord or arpeggio conversion: the pitch is the
// recorded pitch, the length is the recorded length cut at the pass end.
//
// Skipped: SHADOW notes (hidden by the last undo unit), held notes (length 0:
// the person is still holding them), layers in muteMask (bit l = layer l).
// A note of one pitch is cut where the same pitch starts again (as the loop
// itself plays it); the same step + pitch in two layers is exported once.
// Notes of different pitches may overlap (the Melody plays them as a chord).
//
// Tick grid: 24 ticks per sixteenth step, 384 per bar, 16 subticks per tick,
// so one step is 384 subticks.
namespace GroovePuterLooper {

enum class MelodyExportResult : uint8_t {
    Ok = 0,
    Empty,     // no sounding note in the unmuted layers
    Overflow,  // more notes than the Melody holds
    Invalid,   // the Melody did not validate
    BadArgs,   // bars not 1/2/4/8 or passSteps != bars * 16
};

constexpr uint8_t kTicksPerStep = PhraseRuntime::kTicksPerBar / kStepsPerBar;  // 24

inline MelodyExportResult loopToMelody(
        const LoopLayer& layer, uint8_t passSteps, uint8_t bars, uint8_t muteMask,
        PhraseRuntime::RuntimeSynthEventBuffer& out,
        uint16_t maxEvents = PhraseRuntime::kMaxSynthEvents) {
    out = PhraseRuntime::RuntimeSynthEventBuffer{};
    const uint16_t lengthTicks = RuntimePhraseEdit::lengthTicksForBars(bars);
    if (lengthTicks == 0 || passSteps != bars * kStepsPerBar || passSteps > kMaxLoopSteps)
        return MelodyExportResult::BadArgs;
    out.lengthTicks = lengthTicks;
    if (maxEvents > PhraseRuntime::kMaxSynthEvents) maxEvents = PhraseRuntime::kMaxSynthEvents;

    const uint8_t slots = layer.slotCount();
    for (uint8_t i = 0; i < slots; ++i) {
        const LoopNote& n = layer.note(i);
        if (n.shadow() || n.held() || n.onset() >= passSteps) continue;
        if ((muteMask & (1u << n.layer())) != 0) continue;

        const uint8_t onset = n.onset();
        const uint8_t pitch = n.note();
        const uint16_t startTick = static_cast<uint16_t>(onset * kTicksPerStep);

        bool duplicate = false;
        for (uint16_t k = 0; k < out.count && !duplicate; ++k)
            duplicate = out.events[k].startTick == startTick && out.events[k].note == pitch;
        if (duplicate) continue;

        // Recorded length, to the pass end at most (127 = "to the pass end").
        const uint8_t room = static_cast<uint8_t>(passSteps - onset);
        uint8_t steps = n.length() == LoopNote::kLengthToEnd || n.length() > room ? room : n.length();
        // The same pitch starting again ends this note there (the loop's own rule).
        for (uint8_t j = 0; j < slots; ++j) {
            if (j == i) continue;
            const LoopNote& o = layer.note(j);
            if (o.shadow() || o.held() || o.note() != pitch || o.onset() >= passSteps ||
                (muteMask & (1u << o.layer())) != 0) continue;
            if (o.onset() > onset && o.onset() < onset + steps)
                steps = static_cast<uint8_t>(o.onset() - onset);
        }
        if (steps == 0) continue;

        if (out.count >= maxEvents) {
            out = PhraseRuntime::RuntimeSynthEventBuffer{};
            return MelodyExportResult::Overflow;
        }
        PhraseRuntime::RuntimeSynthEvent& event = out.events[out.count++];
        event.startTick = startTick;
        event.durationSubticks = static_cast<uint16_t>(
            steps * kTicksPerStep * PhraseRuntime::kSubticksPerTick);
        event.note = pitch;
        event.velocity = n.velocity() == 0 ? uint8_t{1} : n.velocity();
        event.probability = 100;
        event.flags = 0;
    }

    if (out.count == 0) {
        out = PhraseRuntime::RuntimeSynthEventBuffer{};
        out.lengthTicks = lengthTicks;
        return MelodyExportResult::Empty;
    }
    for (uint16_t i = 1; i < out.count; ++i) {  // stable insertion sort by onset
        const PhraseRuntime::RuntimeSynthEvent value = out.events[i];
        uint16_t j = i;
        while (j > 0 && out.events[j - 1u].startTick > value.startTick) {
            out.events[j] = out.events[j - 1u];
            --j;
        }
        out.events[j] = value;
    }
    if (!RuntimePhraseEdit::validate(out)) {
        out = PhraseRuntime::RuntimeSynthEventBuffer{};
        return MelodyExportResult::Invalid;
    }
    return MelodyExportResult::Ok;
}

}  // namespace GroovePuterLooper

#endif  // GROOVEPUTER_LOOPER_MELODY_EXPORT_H
