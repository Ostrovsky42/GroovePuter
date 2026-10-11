#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_CONTROL_H
#define GROOVEPUTER_PERFORMANCE_CONTROL_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "performance_intent.h"

namespace GroovePuterPerformance {

enum class PerformanceControlId : uint8_t {
    Filter = 0,
    Resonance,
    Level,
    Drive,
    ReverbSend,
    DelaySend,
    Density,
    Count,
};

enum class ControlChangeMode : uint8_t { Absolute = 0, Relative, Count };

struct PerformanceControlIntent {
    PerformanceControlId control{PerformanceControlId::Filter};
    ControlChangeMode mode{ControlChangeMode::Absolute};
    uint8_t value{0};
    int16_t delta{0};
};

struct PerformanceControlState {
    static constexpr uint8_t kControlCount = static_cast<uint8_t>(PerformanceControlId::Count);
    uint8_t values[kControlCount]{96, 0, 100, 0, 0, 32, 100};

    uint8_t get(PerformanceControlId id) const {
        const uint8_t index = static_cast<uint8_t>(id);
        return index < kControlCount ? values[index] : 0;
    }

    bool set(PerformanceControlId id, uint8_t value) {
        const uint8_t index = static_cast<uint8_t>(id);
        if (index >= kControlCount || value > 127u) return false;
        values[index] = value;
        return true;
    }
};
static_assert(sizeof(PerformanceControlState) == 7,
              "semantic performance controls must remain seven bytes");

inline bool applyControlIntent(PerformanceControlState& state,
                               const PerformanceControlIntent& intent) {
    if (intent.control >= PerformanceControlId::Count ||
        intent.mode >= ControlChangeMode::Count || intent.value > 127u) return false;
    if (intent.mode == ControlChangeMode::Absolute) {
        return state.set(intent.control, intent.value);
    }
    const int current = state.get(intent.control);
    int next = current + intent.delta;
    if (next < 0) next = 0;
    if (next > 127) next = 127;
    return state.set(intent.control, static_cast<uint8_t>(next));
}

enum class PerformanceGestureKind : uint8_t {
    DelayThrow = 0,
    FilterPush,
    FilterClose,
    ReverbBloom,
    MuteHit,
    Stutter,
    Break,
    Count,
};

constexpr bool isImplementedPerformanceGesture(PerformanceGestureKind gesture) {
    switch (gesture) {
        case PerformanceGestureKind::DelayThrow:
        case PerformanceGestureKind::FilterPush:
        case PerformanceGestureKind::FilterClose:
        case PerformanceGestureKind::ReverbBloom:
            return true;
        case PerformanceGestureKind::MuteHit:
        case PerformanceGestureKind::Stutter:
        case PerformanceGestureKind::Break:
        case PerformanceGestureKind::Count:
            return false;
    }
    return false;
}

struct PerformanceGestureEvent {
    PerformanceGestureKind gesture{PerformanceGestureKind::DelayThrow};
    bool start{true};
    uint32_t durationMs{0}; // zero means hold until explicit release
};

inline PerformanceIntent toPerformanceIntent(
    const PerformanceControlIntent& control,
    PerformanceInputIdentity input) {
    return makeControlPerformanceIntent(
        input, static_cast<uint8_t>(control.control),
        static_cast<uint8_t>(control.mode), control.value, control.delta);
}

inline bool fromPerformanceIntent(const PerformanceIntent& intent,
                                  PerformanceControlIntent& control) {
    if (intent.kind != PerformanceIntentKind::ChangeControl ||
        intent.parameter >= static_cast<uint8_t>(PerformanceControlId::Count) ||
        intent.flags >= static_cast<uint8_t>(ControlChangeMode::Count) ||
        intent.value > 127u) return false;
    control = PerformanceControlIntent{
        static_cast<PerformanceControlId>(intent.parameter),
        static_cast<ControlChangeMode>(intent.flags), intent.value, intent.delta};
    return true;
}

inline PerformanceIntent toPerformanceIntent(
    const PerformanceGestureEvent& gesture,
    PerformanceInputIdentity input) {
    return makeGesturePerformanceIntent(
        input, static_cast<uint8_t>(gesture.gesture), gesture.start,
        gesture.durationMs);
}

inline bool fromPerformanceIntent(const PerformanceIntent& intent,
                                  PerformanceGestureEvent& gesture) {
    if (intent.kind != PerformanceIntentKind::TriggerPerformanceGesture ||
        intent.parameter >= static_cast<uint8_t>(PerformanceGestureKind::Count) ||
        intent.flags > 1u) return false;
    const auto kind = static_cast<PerformanceGestureKind>(intent.parameter);
    if (!isImplementedPerformanceGesture(kind)) return false;
    gesture = PerformanceGestureEvent{
        kind, intent.flags != 0, intent.durationMs};
    return true;
}

class PerformanceGestureOverlay {
public:
    static constexpr std::size_t kCapacity = 4;

    bool begin(PerformanceGestureKind gesture,
               const PerformanceControlState&,
               uint32_t nowMs,
               uint32_t durationMs = 0) {
        if (!isImplementedPerformanceGesture(gesture)) return false;
        ActiveGesture* activeGesture = find(gesture);
        if (activeGesture == nullptr) activeGesture = findFree();
        if (activeGesture == nullptr) return false;
        activeGesture->active = true;
        activeGesture->gesture = gesture;
        activeGesture->expiresAt = durationMs == 0 ? 0 : nowMs + durationMs;
        return true;
    }

    bool release(PerformanceGestureKind gesture) {
        ActiveGesture* activeGesture = find(gesture);
        if (activeGesture == nullptr) return false;
        *activeGesture = ActiveGesture{};
        return true;
    }

    bool active(PerformanceGestureKind gesture) const {
        for (const ActiveGesture& item : active_) {
            if (item.active && item.gesture == gesture) return true;
        }
        return false;
    }

    void expire(uint32_t nowMs) {
        for (ActiveGesture& item : active_) {
            if (item.active && item.expiresAt != 0 &&
                static_cast<int32_t>(nowMs - item.expiresAt) >= 0) {
                item = ActiveGesture{};
            }
        }
    }

    uint8_t activeMask(uint32_t nowMs) {
        expire(nowMs);
        uint8_t mask = 0;
        for (const ActiveGesture& item : active_) {
            if (item.active) mask |= static_cast<uint8_t>(
                1u << static_cast<uint8_t>(item.gesture));
        }
        return mask;
    }

    uint8_t value(PerformanceControlId control,
                  const PerformanceControlState& base,
                  uint32_t nowMs) {
        expire(nowMs);
        uint8_t result = base.get(control);
        for (const ActiveGesture& item : active_) {
            if (!item.active) continue;
            if ((item.gesture == PerformanceGestureKind::DelayThrow &&
                 control == PerformanceControlId::DelaySend) ||
                (item.gesture == PerformanceGestureKind::ReverbBloom &&
                 control == PerformanceControlId::ReverbSend)) {
                result = 127;
            } else if (item.gesture == PerformanceGestureKind::FilterPush &&
                       control == PerformanceControlId::Filter) {
                result = result < 112 ? 112 : result;
            } else if (item.gesture == PerformanceGestureKind::FilterClose &&
                       control == PerformanceControlId::Filter) {
                result = 8;
            }
        }
        return result;
    }

    PerformanceControlState effectiveState(const PerformanceControlState& base,
                                           uint32_t nowMs) {
        return effectiveStateForMask(base, activeMask(nowMs));
    }

    static PerformanceControlState effectiveStateForMask(
        const PerformanceControlState& base, uint8_t activeMask) {
        PerformanceControlState effective = base;
        for (uint8_t i = 0; i < PerformanceControlState::kControlCount; ++i) {
            const auto control = static_cast<PerformanceControlId>(i);
            uint8_t result = base.get(control);
            const bool delay = (activeMask & (1u << static_cast<uint8_t>(PerformanceGestureKind::DelayThrow))) != 0u;
            const bool push = (activeMask & (1u << static_cast<uint8_t>(PerformanceGestureKind::FilterPush))) != 0u;
            const bool close = (activeMask & (1u << static_cast<uint8_t>(PerformanceGestureKind::FilterClose))) != 0u;
            const bool reverb = (activeMask & (1u << static_cast<uint8_t>(PerformanceGestureKind::ReverbBloom))) != 0u;
            if (delay && control == PerformanceControlId::DelaySend) result = 127;
            if (reverb && control == PerformanceControlId::ReverbSend) result = 127;
            if (push && control == PerformanceControlId::Filter && result < 112u) result = 112u;
            if (close && control == PerformanceControlId::Filter) result = 8u;
            effective.values[i] = result;
        }
        return effective;
    }

private:
    struct ActiveGesture {
        bool active{false};
        PerformanceGestureKind gesture{PerformanceGestureKind::DelayThrow};
        uint32_t expiresAt{0};
    };

    ActiveGesture* find(PerformanceGestureKind gesture) {
        for (ActiveGesture& item : active_) {
            if (item.active && item.gesture == gesture) return &item;
        }
        return nullptr;
    }

    ActiveGesture* findFree() {
        for (ActiveGesture& item : active_) if (!item.active) return &item;
        return nullptr;
    }

    ActiveGesture active_[kCapacity]{};
};
static_assert(sizeof(PerformanceGestureOverlay) <= 32,
              "transient gesture owner must remain bounded to 32 bytes");

struct PerformanceCcBinding {
    uint8_t cc{0};
    PerformanceControlId target{PerformanceControlId::Filter};
    uint8_t minimum{0};
    uint8_t maximum{127};
    bool inverted{false};
};

inline bool mapCcToControl(const PerformanceCcBinding& binding,
                           uint8_t cc,
                           uint8_t value,
                           PerformanceControlIntent& out) {
    if (binding.target >= PerformanceControlId::Count || binding.cc != cc ||
        binding.minimum > binding.maximum || binding.maximum > 127u || value > 127u) {
        return false;
    }
    uint16_t normalized = binding.inverted ? static_cast<uint16_t>(127u - value) : value;
    const uint16_t span = static_cast<uint16_t>(binding.maximum - binding.minimum);
    out = PerformanceControlIntent{
        binding.target,
        ControlChangeMode::Absolute,
        static_cast<uint8_t>(binding.minimum + (normalized * span + 63u) / 127u),
        0};
    return true;
}

// Small generic surface used when no controller profile has been selected.
// Unknown CC numbers are intentionally left unbound and remain diagnostic only.
inline bool genericMidiCcBinding(uint8_t cc, PerformanceCcBinding& out) {
    PerformanceControlId target{};
    switch (cc) {
        case 74: target = PerformanceControlId::Filter; break;
        case 71: target = PerformanceControlId::Resonance; break;
        case 7: target = PerformanceControlId::Level; break;
        case 12: target = PerformanceControlId::Drive; break;
        case 91: target = PerformanceControlId::ReverbSend; break;
        case 93: target = PerformanceControlId::DelaySend; break;
        default: return false;
    }
    out = PerformanceCcBinding{cc, target, 0, 127, false};
    return true;
}

// Discrete PERFORM switches on the generic surface. The intent is absolute: value 64..127
// turns the switch on and 0..63 turns it off, so a repeated "on" never flips it back.
// CC80/CC81 are MIDI general-purpose controllers that the continuous table above leaves free.
inline bool genericMidiToggleCc(uint8_t cc, PerformanceIntentKind& kind) {
    switch (cc) {
        case 80: kind = PerformanceIntentKind::ToggleArp; return true;
        case 81: kind = PerformanceIntentKind::ToggleLatch; return true;
        default: return false;
    }
}

static_assert(std::is_trivially_copyable<PerformanceControlState>::value,
              "Performance control state must be snapshot safe");
static_assert(sizeof(PerformanceControlState) <= 8,
              "Performance control state exceeded its fixed budget");
static_assert(std::is_trivially_copyable<PerformanceControlIntent>::value,
              "Performance control intent must be queue safe");

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_CONTROL_H
