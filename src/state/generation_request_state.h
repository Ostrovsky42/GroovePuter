#pragma once

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

namespace generation_request_detail {
inline GroovePuterRhythm::RealizationLevel& levelStorage() {
    static GroovePuterRhythm::RealizationLevel level =
        GroovePuterRhythm::RealizationLevel::P2Variation;
    return level;
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

}  // namespace GroovePuterState
