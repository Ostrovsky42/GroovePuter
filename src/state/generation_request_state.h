#pragma once

#include <cstddef>
#include <cstdint>

#include "src/generation/rhythm/rhythm_types.h"

namespace GroovePuterState {

// P1/P2/P3 describes how the next generation request realizes the selected
// vocabulary. It is intentionally runtime/session state rather than Scene
// musical content: generated patterns are already persisted in Scene, while
// the level is an instruction for future G / phrase-audition requests.
//
// P2 is the compatibility default because live production was hard-coded to P2
// before the selector existed. Persistence is deliberately deferred: changing
// P-level must never perform synchronous flash/NVS writes on the input path.
inline GroovePuterRhythm::RealizationLevel sanitizeGenerationLevel(uint8_t raw) {
    using GroovePuterRhythm::RealizationLevel;
    return raw < static_cast<uint8_t>(RealizationLevel::Count)
        ? static_cast<RealizationLevel>(raw)
        : RealizationLevel::P2Variation;
}

inline const char* generationLevelCode(
        GroovePuterRhythm::RealizationLevel level) {
    using GroovePuterRhythm::RealizationLevel;
    switch (sanitizeGenerationLevel(static_cast<uint8_t>(level))) {
        case RealizationLevel::P1Canonical: return "P1";
        case RealizationLevel::P2Variation: return "P2";
        case RealizationLevel::P3Transformation: return "P3";
        case RealizationLevel::Count: break;
    }
    return "P2";
}

inline const char* generationLevelShortName(
        GroovePuterRhythm::RealizationLevel level) {
    using GroovePuterRhythm::RealizationLevel;
    switch (sanitizeGenerationLevel(static_cast<uint8_t>(level))) {
        case RealizationLevel::P1Canonical: return "P1 CANON";
        case RealizationLevel::P2Variation: return "P2 VAR";
        case RealizationLevel::P3Transformation: return "P3 TRANS";
        case RealizationLevel::Count: break;
    }
    return "P2 VAR";
}

inline GroovePuterRhythm::RealizationLevel nextGenerationLevel(
        GroovePuterRhythm::RealizationLevel current,
        int direction = 1) {
    using GroovePuterRhythm::RealizationLevel;
    constexpr int kCount = static_cast<int>(RealizationLevel::Count);
    int value = static_cast<int>(sanitizeGenerationLevel(
        static_cast<uint8_t>(current)));
    value += direction;
    while (value < 0) value += kCount;
    while (value >= kCount) value -= kCount;
    return static_cast<RealizationLevel>(value);
}

enum class GenerationAttemptStatus : uint8_t {
    Ok = 0,
    InvalidTuple,
    TableFull,
    OrdinalExhausted,
};

struct GenerationAttemptAllocation {
    GenerationAttemptStatus status = GenerationAttemptStatus::InvalidTuple;
    uint32_t ordinal = 0;

    bool ok() const { return status == GenerationAttemptStatus::Ok; }
};

namespace generation_request_detail {
inline GroovePuterRhythm::RealizationLevel& levelStorage() {
    static GroovePuterRhythm::RealizationLevel level =
        GroovePuterRhythm::RealizationLevel::P2Variation;
    return level;
}

// Reroll identity is deliberately session-only and allocation-free. The key is
// exactly (generativeMode, recipe, P-level, patternAddress). Current production
// has 16 GenerativeMode values, so four bits are sufficient and the remaining
// fields fit in one 30-bit packed key. A zero key slot means unused; stored keys
// are offset by one so the all-zero tuple remains representable.
//
// The table intentionally fails closed instead of evicting another tuple. That
// guarantees a failed/cancelled request can never reset or mutate the ordinal of
// a different generation tuple (GA-06). 64 live tuple identities cost 512 B of
// fixed session state and require no heap/NVS traffic.
constexpr std::size_t kGenerationAttemptCapacity = 64;
static_assert(kGenerationAttemptCapacity != 0 &&
              (kGenerationAttemptCapacity & (kGenerationAttemptCapacity - 1u)) == 0,
              "generation attempt table must remain power-of-two");

constexpr unsigned attemptCapacityBits() {
    std::size_t value = kGenerationAttemptCapacity;
    unsigned bits = 0;
    while (value > 1) {
        value >>= 1u;
        ++bits;
    }
    return bits;
}

struct GenerationAttemptEntry {
    uint32_t keyPlusOne = 0;
    uint32_t nextOrdinal = 0;
};

inline GenerationAttemptEntry* attemptStorage() {
    static GenerationAttemptEntry entries[kGenerationAttemptCapacity]{};
    return entries;
}

inline bool packAttemptKey(uint8_t generativeMode,
                           uint8_t recipe,
                           GroovePuterRhythm::RealizationLevel level,
                           int patternAddress,
                           uint32_t& packed) {
    using GroovePuterRhythm::RealizationLevel;
    if (generativeMode >= 16 ||
        static_cast<uint8_t>(level) >= static_cast<uint8_t>(RealizationLevel::Count) ||
        patternAddress < 0 || patternAddress > 0xFFFF) {
        return false;
    }
    packed = (static_cast<uint32_t>(generativeMode) << 26u) |
             (static_cast<uint32_t>(recipe) << 18u) |
             (static_cast<uint32_t>(level) << 16u) |
             static_cast<uint16_t>(patternAddress);
    return true;
}

inline std::size_t attemptStartIndex(uint32_t packed) {
    return static_cast<std::size_t>(
        (packed * 2654435761u) >> (32u - attemptCapacityBits()));
}
}  // namespace generation_request_detail

inline GroovePuterRhythm::RealizationLevel currentGenerationLevel() {
    return generation_request_detail::levelStorage();
}

inline bool setGenerationLevel(GroovePuterRhythm::RealizationLevel level) {
    const auto sanitized = sanitizeGenerationLevel(static_cast<uint8_t>(level));
    if (generation_request_detail::levelStorage() == sanitized) return false;
    generation_request_detail::levelStorage() = sanitized;
    return true;
}

inline GroovePuterRhythm::RealizationLevel cycleGenerationLevel(int direction = 1) {
    const auto next = nextGenerationLevel(currentGenerationLevel(), direction);
    setGenerationLevel(next);
    return next;
}

inline constexpr std::size_t generationAttemptCapacity() {
    return generation_request_detail::kGenerationAttemptCapacity;
}

inline GenerationAttemptAllocation allocateGenerationAttempt(
        uint8_t generativeMode,
        uint8_t recipe,
        GroovePuterRhythm::RealizationLevel level,
        int patternAddress) {
    uint32_t packed = 0;
    if (!generation_request_detail::packAttemptKey(
            generativeMode, recipe, level, patternAddress, packed)) {
        return {GenerationAttemptStatus::InvalidTuple, 0};
    }

    const uint32_t keyPlusOne = packed + 1u;
    auto* entries = generation_request_detail::attemptStorage();
    const std::size_t start = generation_request_detail::attemptStartIndex(packed);
    for (std::size_t probe = 0;
         probe < generation_request_detail::kGenerationAttemptCapacity;
         ++probe) {
        auto& entry = entries[
            (start + probe) & (generation_request_detail::kGenerationAttemptCapacity - 1u)];
        if (entry.keyPlusOne == keyPlusOne) {
            if (entry.nextOrdinal == UINT32_MAX) {
                return {GenerationAttemptStatus::OrdinalExhausted, 0};
            }
            const uint32_t ordinal = entry.nextOrdinal;
            ++entry.nextOrdinal;
            return {GenerationAttemptStatus::Ok, ordinal};
        }
        if (entry.keyPlusOne == 0) {
            entry.keyPlusOne = keyPlusOne;
            entry.nextOrdinal = 1;
            return {GenerationAttemptStatus::Ok, 0};
        }
    }
    return {GenerationAttemptStatus::TableFull, 0};
}

inline void resetGenerationAttemptState() {
    auto* entries = generation_request_detail::attemptStorage();
    for (std::size_t index = 0;
         index < generation_request_detail::kGenerationAttemptCapacity;
         ++index) {
        entries[index] = {};
    }
}

}  // namespace GroovePuterState
