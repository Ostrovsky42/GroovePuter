#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_CONTROL_H
#define GROOVEPUTER_PERFORMANCE_CONTROL_H

#include "performance_intent.h"

namespace GroovePuterPerformance {

constexpr uint8_t kGenericMidiArpEnableCc = 80;
constexpr uint8_t kGenericMidiLatchEnableCc = 81;

inline bool makeGenericMidiToggleIntent(uint8_t controller, uint8_t value,
                                       PerformanceInputIdentity input,
                                       PerformanceIntent& out) {
    if (controller == kGenericMidiArpEnableCc) {
        out = PerformanceIntent{};
        out.kind = PerformanceIntentKind::SetArpEnabled;
    } else if (controller == kGenericMidiLatchEnableCc) {
        out = PerformanceIntent{};
        out.kind = PerformanceIntentKind::SetLatchEnabled;
    } else {
        return false;
    }
    out.input = input;
    out.value = value >= 64u ? 1u : 0u;
    return true;
}

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_CONTROL_H
