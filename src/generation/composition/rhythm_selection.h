#pragma once

#include <cstdint>

#include "../generation_context.h"
#include "../rhythm/reference_vocabulary.h"
#include "rhythm_selection_types.h"

struct GenreSettings;

namespace GroovePuterRhythm {

struct RhythmCompatibilityCandidate {
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  uint8_t weight = 0;
};

struct RhythmCompatibilityView {
  const RhythmCompatibilityCandidate* candidates = nullptr;
  uint8_t count = 0;
};

struct RhythmSelectionIntent {
  RhythmSelectionMode mode = RhythmSelectionMode::Auto;
  RhythmArchetypeId manualArchetypeId = kNoArchetypeId;
};

enum class RhythmSelectionStatus : uint8_t {
  Ok = 0,
  NoCompatibleRhythm,
  Count,
};

struct RhythmSelectionResult {
  RhythmSelectionStatus status = RhythmSelectionStatus::NoCompatibleRhythm;
  RhythmSelectionMode mode = RhythmSelectionMode::Auto;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  bool normalizedToAuto = false;
};

// Genre/Variant compatibility is data-only. Vocabulary and realizer code do
// not inspect GenreSettings.
RhythmCompatibilityView rhythmCompatibilityFor(const GenreSettings& settings);

RhythmSelectionIntent rhythmSelectionIntent(const GenreSettings& settings);
bool isRhythmCompatible(const GenreSettings& settings,
                        RhythmArchetypeId archetypeId);

// Canonical enumeration is sorted by stable archetype ID and independent of
// the declaration order of the compatibility table.
uint8_t compatibleRhythmCount(const GenreSettings& settings);
RhythmArchetypeId compatibleRhythmId(const GenreSettings& settings,
                                     uint8_t canonicalIndex);

// Public view-based boundary permits order-invariance/property tests without
// exposing or mutating the built-in compatibility table.
RhythmSelectionResult resolveRhythmSelectionFromView(
    RhythmCompatibilityView compatibility,
    const RhythmSelectionIntent& intent,
    const GenerationContext& generation);

RhythmSelectionResult resolveRhythmSelection(
    const GenreSettings& settings,
    const GenerationContext& generation);

const char* rhythmSelectionName(RhythmArchetypeId archetypeId);

}  // namespace GroovePuterRhythm
