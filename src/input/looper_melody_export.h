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
// The Melody stays on the 1/16 grid; the loop is on ticks (6 to a step,
// looper_layer.h). Each onset goes to the nearest step (a note in the last
// half step of the pass stays on the last step), each note end to the nearest
// step, the length is at least one step. A triplet or 1/32 line therefore
// loses its rhythm here: two notes of one pitch that land on one step are
// exported once.
//
// Skipped: pending pickups, SHADOW notes (hidden by the last undo unit), held notes (length 0:
// the person is still holding them), layers in muteMask (bit l = layer l).
// A note of one pitch is cut where the same pitch starts again on a later
// step (as the loop itself plays it); the same step + pitch (in one layer
// after rounding, or in two layers) is exported once, the first in the pool.
// Notes of different pitches may overlap (the Melody plays them as a chord).
//
// Melody tick grid: 24 ticks per sixteenth step, 384 per bar, 16 subticks per
// tick, so one step is 384 subticks.
namespace GroovePuterLooper {

enum class MelodyExportResult : uint8_t {
    Ok = 0,
    Empty,     // no sounding note in the unmuted layers
    Overflow,  // more notes than the Melody holds
    Invalid,   // the Melody did not validate
    BadArgs,   // bars not 1/2/4/8 or passSteps != bars * 16
};

constexpr uint8_t kMelodyTicksPerStep = PhraseRuntime::kTicksPerBar / kStepsPerBar;  // 24

namespace detail {
// Onset of a loop note on the Melody's 1/16 grid.
inline uint8_t melodyOnsetStep(const LoopNote& n, uint8_t passSteps) {
    const uint8_t step = n.onsetStep();
    return step >= passSteps ? static_cast<uint8_t>(passSteps - 1u) : step;
}
inline bool melodyExports(const LoopNote& n, uint8_t passSteps, uint8_t muteMask) {
    return !n.shadow() && !n.pickup() && !n.held() && n.onsetTicks() < passSteps * kTicksPerStep &&
           (muteMask & (1u << n.layer())) == 0;
}
}  // namespace detail

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
    const uint16_t passTicks = static_cast<uint16_t>(passSteps * kTicksPerStep);
    for (uint8_t i = 0; i < slots; ++i) {
        const LoopNote& n = layer.note(i);
        if (!detail::melodyExports(n, passSteps, muteMask)) continue;

        const uint8_t onset = detail::melodyOnsetStep(n, passSteps);
        const uint8_t pitch = n.note();
        const uint16_t startTick = static_cast<uint16_t>(onset * kMelodyTicksPerStep);

        bool duplicate = false;
        for (uint16_t k = 0; k < out.count && !duplicate; ++k)
            duplicate = out.events[k].startTick == startTick && out.events[k].note == pitch;
        if (duplicate) continue;

        // Recorded end, at the pass end at most, on the nearest step.
        const uint16_t room = static_cast<uint16_t>(passTicks - n.onsetTicks());
        const uint16_t ticks = n.lengthTicks() == LoopNote::kLengthToEnd || n.lengthTicks() > room
            ? room : n.lengthTicks();
        uint8_t endStep = static_cast<uint8_t>(
            (n.onsetTicks() + ticks + kTicksPerStep / 2u) / kTicksPerStep);
        if (endStep > passSteps) endStep = passSteps;
        uint8_t steps = endStep > onset ? static_cast<uint8_t>(endStep - onset) : uint8_t{1};
        // The same pitch starting again on a later step ends this note there.
        for (uint8_t j = 0; j < slots; ++j) {
            if (j == i) continue;
            const LoopNote& o = layer.note(j);
            if (o.note() != pitch || !detail::melodyExports(o, passSteps, muteMask)) continue;
            const uint8_t other = detail::melodyOnsetStep(o, passSteps);
            if (other > onset && other < onset + steps)
                steps = static_cast<uint8_t>(other - onset);
        }

        if (out.count >= maxEvents) {
            out = PhraseRuntime::RuntimeSynthEventBuffer{};
            return MelodyExportResult::Overflow;
        }
        PhraseRuntime::RuntimeSynthEvent& event = out.events[out.count++];
        event.startTick = startTick;
        event.durationSubticks = static_cast<uint16_t>(
            steps * kMelodyTicksPerStep * PhraseRuntime::kSubticksPerTick);
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
