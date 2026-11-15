#pragma once
// Layer rules for CC values, pure and host-testable. A controller on the wire is
// its base (what the user, a sweep or a preset set) plus the sum of the offset
// layers over it (TILT axes, later macros). Layers never change the base.
//
// Rules:
//  - no layer sum and nothing pending: nothing to send;
//  - a Centered controller without a known base starts from its neutral 64;
//  - a Send or Level controller without a known base gets no layer at all (there is
//    no value to return to), so nothing is sent and an old pending return is dropped;
//  - the wire value is base + sum, clamped to 0..127 and to the controller's minimum;
//  - when the sum is back at zero, the base is sent and the pending return completes
//    once the base is delivered (the caller checks delivery).
#include <cstdint>

#include "cutoff_sweep.h"

namespace GroovePuterMidi {

struct LayerStep {
    bool send{false};            // write `value` to the wire now
    uint8_t value{0};
    bool returnsToBase{false};   // the sum is zero: `value` is the base
    bool dropPending{false};     // no base to return to: forget a pending return
};

// fallbackBase (>= 0) is the value a controller rests at when nothing was set, such
// as EXPRESSION at 127; -1 keeps the rule above: Centered rests at 64, the rest have none.
inline LayerStep composeLayer(CutoffKind kind, bool baseKnown, uint8_t base, uint8_t minValue,
                              int sum, bool pending, int fallbackBase = -1) {
    LayerStep step{};
    if (sum == 0 && !pending) return step;
    if (!baseKnown) {
        if (fallbackBase >= 0) {
            base = static_cast<uint8_t>(fallbackBase);
        } else if (kind == CutoffKind::Centered) {
            base = kCenteredNeutral;
        } else {
            step.dropPending = sum == 0 && pending;
            return step;
        }
    }
    int value = static_cast<int>(base) + sum;
    if (value < static_cast<int>(minValue)) value = static_cast<int>(minValue);
    if (value > 127) value = 127;
    step.send = true;
    step.value = static_cast<uint8_t>(value);
    step.returnsToBase = sum == 0;
    return step;
}

// CC sends per service pass (all voices together). A pass that is not budgeted could
// spend its time on CC while Note On/Off waits; the budget bounds that work, and the
// start point rotates so every controller gets its turn.
inline constexpr uint8_t kCcSendsPerPass = 4;

class PassBudget {
public:
    explicit PassBudget(uint8_t limit = kCcSendsPerPass) : limit_(limit) {}
    // True when one more send fits in this pass (and counts it).
    bool take() {
        if (used_ >= limit_) return false;
        ++used_;
        return true;
    }
    uint8_t used() const { return used_; }

private:
    uint8_t limit_;
    uint8_t used_{0};
};

// The list position visited at `step` when the walk starts at `start`.
inline uint8_t rotatedIndex(uint8_t step, uint8_t count, uint8_t start) {
    return count == 0 ? 0 : static_cast<uint8_t>((start + step) % count);
}

}  // namespace GroovePuterMidi
