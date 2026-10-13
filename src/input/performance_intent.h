#ifndef GROOVEPUTER_PERFORMANCE_INTENT_H
#define GROOVEPUTER_PERFORMANCE_INTENT_H

#include <cstdint>
#include <type_traits>

struct PerformanceInputIdentity {
    uint32_t generation{0};
    uint8_t source{0};
    uint8_t channel{0};
    uint8_t inputNote{0};
    uint8_t cable{0};
};
static_assert(sizeof(PerformanceInputIdentity) == 8,
              "performance input identity must remain an eight-byte value");
static_assert(std::is_trivially_copyable<PerformanceInputIdentity>::value,
              "performance input identity must be trivially copyable");

constexpr bool samePerformanceInput(const PerformanceInputIdentity& lhs,
                                    const PerformanceInputIdentity& rhs) {
    return lhs.generation == rhs.generation && lhs.source == rhs.source &&
           lhs.channel == rhs.channel && lhs.inputNote == rhs.inputNote &&
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
    SelectHarmonyChangePolicy,
    ToggleLatch,
};

struct PerformanceIntent {
    PerformanceIntentKind kind{PerformanceIntentKind::PlayNote};
    PerformanceInputIdentity input{};
    uint8_t note{0};
    uint8_t velocity{0};
    uint8_t value{0};
};
static_assert(std::is_trivially_copyable<PerformanceIntent>::value &&
                  sizeof(PerformanceIntent) <= 16,
              "performance intent must remain a bounded value type");

inline PerformanceIntent makeNotePerformanceIntent(PerformanceIntentKind kind,
                                                    PerformanceInputIdentity input,
                                                    uint8_t note,
                                                    uint8_t velocity = 0) {
    PerformanceIntent intent{};
    intent.kind = kind;
    intent.input = input;
    intent.note = note;
    intent.velocity = velocity;
    return intent;
}

#endif  // GROOVEPUTER_PERFORMANCE_INTENT_H
