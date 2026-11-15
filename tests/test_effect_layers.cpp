// Layer rules (src/midi/effect_layers.h): base + offset sums, neutral 64 for
// Centered controllers, no layer on Send and Level without a base.
#include <cassert>
#include <cstdint>

#include "src/midi/effect_layers.h"

using namespace GroovePuterMidi;

int main() {
    // Nothing applies: no send.
    assert(!composeLayer(CutoffKind::Centered, true, 40, 0, 0, false).send);

    // Centered without a base: starts from 64.
    LayerStep s = composeLayer(CutoffKind::Centered, false, 0, 0, 9, false);
    assert(s.send && s.value == 73 && !s.returnsToBase);
    // Centered without a base returning to zero: the neutral 64 is sent and completes.
    s = composeLayer(CutoffKind::Centered, false, 0, 0, 0, true);
    assert(s.send && s.value == 64 && s.returnsToBase && !s.dropPending);

    // Send without a base: no layer, nothing sent.
    s = composeLayer(CutoffKind::Send, false, 0, 0, 20, false);
    assert(!s.send && !s.dropPending);
    // Send without a base returning: the pending return is dropped (nothing to return to).
    s = composeLayer(CutoffKind::Send, false, 0, 0, 0, true);
    assert(!s.send && s.dropPending);
    // Level (VOLUME) behaves like Send without a base.
    assert(!composeLayer(CutoffKind::Level, false, 0, 0, -12, false).send);

    // Known base: base + sum.
    s = composeLayer(CutoffKind::Send, true, 30, 0, 20, false);
    assert(s.send && s.value == 50 && !s.returnsToBase);
    // Returning to zero sends the base.
    s = composeLayer(CutoffKind::Send, true, 30, 0, 0, true);
    assert(s.send && s.value == 30 && s.returnsToBase);

    // Clamping to 0..127 and to the controller's minimum (PAN 1..127).
    assert(composeLayer(CutoffKind::Centered, true, 120, 0, 20, false).value == 127);
    assert(composeLayer(CutoffKind::Centered, true, 5, 0, -20, false).value == 0);
    assert(composeLayer(CutoffKind::Centered, true, 0, 1, -5, false).value == 1);

    // Negative offsets on a centred base go below the neutral.
    assert(composeLayer(CutoffKind::Centered, false, 0, 0, -63, false).value == 1);
    // Budget: at most kCcSendsPerPass sends per pass, further ones are refused.
    PassBudget budget;
    for (uint8_t i = 0; i < kCcSendsPerPass; ++i) assert(budget.take());
    assert(!budget.take() && budget.used() == kCcSendsPerPass);
    PassBudget small(2);
    assert(small.take() && small.take() && !small.take());

    // Rotation: a walk from any start visits every controller once.
    for (uint8_t start = 0; start < 8; ++start) {
        bool seen[8] = {false, false, false, false, false, false, false, false};
        for (uint8_t step = 0; step < 8; ++step) seen[rotatedIndex(step, 8, start)] = true;
        for (bool v : seen) assert(v);
    }
    assert(rotatedIndex(0, 0, 3) == 0);
    return 0;
}
