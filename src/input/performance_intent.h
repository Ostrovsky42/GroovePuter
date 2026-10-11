#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_INTENT_H
#define GROOVEPUTER_PERFORMANCE_INTENT_H

#include <cstdint>

// Adapter-neutral identity for one held input note. `source` is assigned by the
// input adapter; `generation` changes whenever that adapter starts a new session.
// Keep the 32-bit generation first so the value stays eight bytes on ESP32 and host.
struct PerformanceInputIdentity {
    uint32_t generation{0};
    uint8_t source{0};
    uint8_t channel{0};
    uint8_t inputNote{0};
    uint8_t cable{0};
};
static_assert(sizeof(PerformanceInputIdentity) == 8,
              "performance source identity must remain an eight-byte value");

constexpr uint8_t kPerformanceInputSourceCardputer = 0;
constexpr uint8_t kPerformanceInputChannelKeyMatrix = 0xffu;

constexpr bool samePerformanceInput(const PerformanceInputIdentity& lhs,
                                    const PerformanceInputIdentity& rhs) {
    return lhs.generation == rhs.generation &&
           lhs.source == rhs.source &&
           lhs.channel == rhs.channel &&
           lhs.inputNote == rhs.inputNote &&
           lhs.cable == rhs.cable;
}

enum class PerformanceIntentKind : uint8_t {
    PlayNote = 0,
    ReleaseNote,
    Hold,
    ReleaseHold,
    ChangeOctave,
    SelectRole,
    SelectLeadMode,
    SelectChordMode,
    ToggleArp,
    SelectArpPitchSource,
    SelectArpContour,
    ChangeArpRate,
    SelectHarmonyChangePolicy,
    ToggleLatch,
    ChangeControl,
    TriggerPerformanceGesture,
    SelectScene,
    CaptureScene,
    RecallScene,
};

struct PerformanceIntent {
    PerformanceIntentKind kind{PerformanceIntentKind::PlayNote};
    PerformanceInputIdentity input{};
    uint8_t note{0};
    uint8_t velocity{0};
    // `parameter` identifies a semantic control, gesture, or scene operation.
    // `flags` carries bounded mode/start bits; `value` and `delta` carry the
    // requested setting. Transport boundaries are selected by the Scene owner.
    uint8_t parameter{0};
    uint8_t value{0};
    uint8_t flags{0};
    int16_t delta{0};
    uint32_t durationMs{0};
};

inline PerformanceIntent makeNotePerformanceIntent(
    PerformanceIntentKind kind,
    PerformanceInputIdentity input,
    uint8_t note,
    uint8_t velocity) {
    PerformanceIntent intent{};
    intent.kind = kind;
    intent.input = input;
    intent.note = note;
    intent.velocity = velocity;
    return intent;
}

inline bool isCardputerKeyInput(const PerformanceInputIdentity& input) {
    return input.source == kPerformanceInputSourceCardputer &&
           input.channel == kPerformanceInputChannelKeyMatrix;
}

inline PerformanceIntent makeControlPerformanceIntent(
    PerformanceInputIdentity input,
    uint8_t controlId,
    uint8_t changeMode,
    uint8_t value,
    int16_t delta) {
    PerformanceIntent intent{};
    intent.kind = PerformanceIntentKind::ChangeControl;
    intent.input = input;
    intent.parameter = controlId;
    intent.flags = changeMode;
    intent.value = value;
    intent.delta = delta;
    return intent;
}

inline PerformanceIntent makeGesturePerformanceIntent(
    PerformanceInputIdentity input,
    uint8_t gestureId,
    bool start,
    uint32_t durationMs) {
    PerformanceIntent intent{};
    intent.kind = PerformanceIntentKind::TriggerPerformanceGesture;
    intent.input = input;
    intent.parameter = gestureId;
    intent.flags = start ? 1u : 0u;
    intent.durationMs = durationMs;
    return intent;
}

inline PerformanceIntent makeSelectionPerformanceIntent(
    PerformanceIntentKind kind,
    PerformanceInputIdentity input,
    uint8_t value,
    uint8_t auxiliary = 0) {
    PerformanceIntent intent{};
    intent.kind = kind;
    intent.input = input;
    intent.value = value;
    intent.parameter = auxiliary;
    return intent;
}

inline PerformanceIntent makeScenePerformanceIntent(
    PerformanceIntentKind kind,
    PerformanceInputIdentity input,
    uint8_t slot,
    uint8_t transition) {
    PerformanceIntent intent{};
    intent.kind = kind;
    intent.input = input;
    intent.parameter = slot;
    intent.value = transition;
    return intent;
}

inline bool samePerformanceIntent(const PerformanceIntent& lhs,
                                  const PerformanceIntent& rhs) {
    if (lhs.kind != rhs.kind) return false;
    switch (lhs.kind) {
        case PerformanceIntentKind::PlayNote:
        case PerformanceIntentKind::ReleaseNote:
        case PerformanceIntentKind::Hold:
        case PerformanceIntentKind::ReleaseHold:
            return samePerformanceInput(lhs.input, rhs.input) &&
                   lhs.note == rhs.note && lhs.velocity == rhs.velocity;
        case PerformanceIntentKind::ChangeControl:
            return lhs.parameter == rhs.parameter && lhs.flags == rhs.flags &&
                   lhs.value == rhs.value && lhs.delta == rhs.delta;
        case PerformanceIntentKind::TriggerPerformanceGesture:
            return lhs.parameter == rhs.parameter && lhs.flags == rhs.flags &&
                   lhs.durationMs == rhs.durationMs;
        case PerformanceIntentKind::SelectScene:
        case PerformanceIntentKind::CaptureScene:
        case PerformanceIntentKind::RecallScene:
            return lhs.parameter == rhs.parameter && lhs.value == rhs.value;
        default:
            return lhs.value == rhs.value && lhs.parameter == rhs.parameter &&
                   lhs.flags == rhs.flags && lhs.delta == rhs.delta &&
                   lhs.durationMs == rhs.durationMs;
    }
}

static_assert(sizeof(PerformanceIntent) <= 24,
              "PerformanceIntent must remain a bounded queue value");

#endif  // GROOVEPUTER_PERFORMANCE_INTENT_H

