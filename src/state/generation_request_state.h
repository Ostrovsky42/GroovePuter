#pragma once

#include <cstdint>

#include "src/generation/rhythm/rhythm_types.h"

#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
#include <Preferences.h>
#endif

namespace GroovePuterState {

// P1/P2/P3 describes how the next generation request realizes the selected
// vocabulary. It is intentionally a device-session preference rather than Scene
// content: generated patterns are already persisted in Scene, while the level is
// an instruction for future G / phrase-audition requests.
inline GroovePuterRhythm::RealizationLevel sanitizeGenerationLevel(uint8_t raw) {
    using GroovePuterRhythm::RealizationLevel;
    return raw < static_cast<uint8_t>(RealizationLevel::Count)
        ? static_cast<RealizationLevel>(raw)
        : RealizationLevel::P2Variation;
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

inline bool& loadedStorage() {
    static bool loaded = false;
    return loaded;
}

inline void loadOnce() {
    if (loadedStorage()) return;
    loadedStorage() = true;
#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
    Preferences preferences;
    if (preferences.begin("gp-generation", true)) {
        const uint8_t raw = preferences.getUChar(
            "p-level",
            static_cast<uint8_t>(
                GroovePuterRhythm::RealizationLevel::P2Variation));
        levelStorage() = sanitizeGenerationLevel(raw);
        preferences.end();
    }
#endif
}

inline void persist() {
#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
    Preferences preferences;
    if (!preferences.begin("gp-generation", false)) return;
    preferences.putUChar(
        "p-level", static_cast<uint8_t>(levelStorage()));
    preferences.end();
#endif
}
}  // namespace generation_request_detail

inline GroovePuterRhythm::RealizationLevel currentGenerationLevel() {
    generation_request_detail::loadOnce();
    return generation_request_detail::levelStorage();
}

inline bool setGenerationLevel(GroovePuterRhythm::RealizationLevel level) {
    generation_request_detail::loadOnce();
    const auto sanitized = sanitizeGenerationLevel(static_cast<uint8_t>(level));
    if (generation_request_detail::levelStorage() == sanitized) return false;
    generation_request_detail::levelStorage() = sanitized;
    generation_request_detail::persist();
    return true;
}

inline GroovePuterRhythm::RealizationLevel cycleGenerationLevel(int direction = 1) {
    const auto next = nextGenerationLevel(currentGenerationLevel(), direction);
    setGenerationLevel(next);
    return next;
}

}  // namespace GroovePuterState
