#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "../../scenes.h"
#include "../../src/generation/migration/strong_rhythm_migration.h"

namespace GroovePuterRhythm {
namespace GF2Measurement {

enum class MaterialProvenance : uint8_t {
  RequestedOperationAccepted = 0,
  PreviousMaterialRetained,
  NonAcceptedMaterialChanged,
};

struct GenerationObservation {
  // REQUEST: direct copies of production-owned semantic request/context fields.
  uint8_t requestedMode = 0;
  uint8_t requestedRecipe = 0;
  uint8_t requestedRhythmSelectionMode = 0;
  uint16_t requestedRhythmArchetypeId = 0;
  int16_t patternAddress = -1;
  uint8_t realizationLevel = 0;
  uint32_t generationAttemptOrdinal = 0;

  // EXECUTION: statuses produced by the real migration and quantized owner.
  uint8_t migrationStatus = 0;
  uint8_t generationOutcome = 0;
  bool generationAccepted = false;

  // RESULT: semantic identity reported by migration plus actual live material.
  uint8_t migrationRoute = 0;
  uint16_t migrationArchetype = 0;
  uint16_t bassRhythmId = 0;
  uint16_t chordRhythmId = 0;
  uint16_t progressionId = 0;
  uint16_t melodicRhythmId = 0;
  uint16_t motifShapeId = 0;
  uint8_t synthBRole = 0;
  uint16_t phraseLaw = 0;
  uint8_t phraseBars = 0;
  uint32_t effectiveMaterialFingerprint = 0;

  // PROVENANCE: whether the request actually owns the effective live result.
  uint32_t previousMaterialFingerprint = 0;
  MaterialProvenance provenance = MaterialProvenance::NonAcceptedMaterialChanged;
  bool requestedResultEffective = false;
};

inline uint32_t drumFingerprint(const DrumPatternSet& drums) {
  uint32_t hash = 2166136261u;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& event = drums.voices[voice].steps[step];
      const uint8_t bytes[] = {
          static_cast<uint8_t>(event.hit ? 1 : 0),
          static_cast<uint8_t>(event.accent ? 1 : 0),
          event.velocity,
          static_cast<uint8_t>(event.timing),
          event.fx,
          event.fxParam,
          event.probability,
      };
      for (uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 16777619u;
      }
    }
  }
  return hash;
}

inline uint32_t synthFingerprint(const SynthPattern& pattern) {
  uint32_t hash = 2166136261u;
  for (const SynthStep& event : pattern.steps) {
    const uint8_t bytes[] = {
        static_cast<uint8_t>(event.note),
        static_cast<uint8_t>(event.slide ? 1 : 0),
        static_cast<uint8_t>(event.accent ? 1 : 0),
        static_cast<uint8_t>(event.ghost ? 1 : 0),
        event.velocity,
        static_cast<uint8_t>(event.timing),
        event.fx,
        event.fxParam,
        event.probability,
    };
    for (uint8_t byte : bytes) {
      hash ^= byte;
      hash *= 16777619u;
    }
  }
  return hash;
}

inline uint32_t materialFingerprint(const DrumPatternSet& drums,
                                    const SynthPattern& synthA,
                                    const SynthPattern& synthB) {
  uint32_t hash = drumFingerprint(drums);
  hash ^= synthFingerprint(synthA);
  hash *= 16777619u;
  hash ^= synthFingerprint(synthB);
  hash *= 16777619u;
  return hash;
}

inline GenerationObservation observeGeneration(
    const GenreSettings& request,
    const StrongRhythmMigrationContext& context,
    const StrongRhythmMigrationResult& migration,
    uint8_t generationOutcome,
    bool generationAccepted,
    uint32_t previousMaterialFingerprint,
    uint32_t effectiveMaterialFingerprint) {
  GenerationObservation observation{};
  observation.requestedMode = request.generativeMode;
  observation.requestedRecipe = request.recipe;
  observation.requestedRhythmSelectionMode = request.rhythmSelectionMode;
  observation.requestedRhythmArchetypeId = request.rhythmArchetypeId;
  observation.patternAddress = context.patternAddress;
  observation.realizationLevel = static_cast<uint8_t>(context.level);
  observation.generationAttemptOrdinal = context.generationAttemptOrdinal;

  observation.migrationStatus = static_cast<uint8_t>(migration.status);
  observation.generationOutcome = generationOutcome;
  observation.generationAccepted = generationAccepted;

  observation.migrationRoute = static_cast<uint8_t>(migration.route);
  observation.migrationArchetype = static_cast<uint16_t>(migration.archetype);
  observation.bassRhythmId = static_cast<uint16_t>(migration.bassRhythmId);
  observation.chordRhythmId = static_cast<uint16_t>(migration.chordRhythmId);
  observation.progressionId = static_cast<uint16_t>(migration.progressionId);
  observation.melodicRhythmId = static_cast<uint16_t>(migration.melodicRhythmId);
  observation.motifShapeId = static_cast<uint16_t>(migration.motifShapeId);
  observation.synthBRole = static_cast<uint8_t>(migration.synthBRole);
  observation.phraseLaw = static_cast<uint16_t>(migration.phraseLaw);
  observation.phraseBars = migration.phraseBars;
  observation.effectiveMaterialFingerprint = effectiveMaterialFingerprint;
  observation.previousMaterialFingerprint = previousMaterialFingerprint;

  const bool applied = migration.status == StrongRhythmMigrationStatus::Applied;
  if (applied && generationAccepted) {
    observation.provenance = MaterialProvenance::RequestedOperationAccepted;
    observation.requestedResultEffective = true;
  } else if (previousMaterialFingerprint == effectiveMaterialFingerprint) {
    observation.provenance = MaterialProvenance::PreviousMaterialRetained;
    observation.requestedResultEffective = false;
  } else {
    observation.provenance = MaterialProvenance::NonAcceptedMaterialChanged;
    observation.requestedResultEffective = false;
  }
  return observation;
}

inline std::string toJson(const char* caseName,
                          const GenerationObservation& observation) {
  char buffer[2048]{};
  std::snprintf(
      buffer,
      sizeof(buffer),
      "{\"case\":\"%s\",\"request\":{\"mode\":%u,\"recipe\":%u,"
      "\"rhythm_selection_mode\":%u,\"rhythm_archetype_id\":%u,"
      "\"pattern_address\":%d,\"level\":%u,\"attempt\":%u},"
      "\"execution\":{\"migration_status\":%u,\"generation_outcome\":%u,"
      "\"accepted\":%s},\"result\":{\"route\":%u,\"archetype\":%u,"
      "\"bass_rhythm\":%u,\"chord_rhythm\":%u,\"progression\":%u,"
      "\"melodic_rhythm\":%u,\"motif_shape\":%u,\"synth_b_role\":%u,"
      "\"phrase_law\":%u,\"phrase_bars\":%u,"
      "\"effective_material_fingerprint\":%u},"
      "\"provenance\":{\"kind\":%u,\"requested_result_effective\":%s,"
      "\"previous_material_fingerprint\":%u}}",
      caseName,
      static_cast<unsigned>(observation.requestedMode),
      static_cast<unsigned>(observation.requestedRecipe),
      static_cast<unsigned>(observation.requestedRhythmSelectionMode),
      static_cast<unsigned>(observation.requestedRhythmArchetypeId),
      static_cast<int>(observation.patternAddress),
      static_cast<unsigned>(observation.realizationLevel),
      static_cast<unsigned>(observation.generationAttemptOrdinal),
      static_cast<unsigned>(observation.migrationStatus),
      static_cast<unsigned>(observation.generationOutcome),
      observation.generationAccepted ? "true" : "false",
      static_cast<unsigned>(observation.migrationRoute),
      static_cast<unsigned>(observation.migrationArchetype),
      static_cast<unsigned>(observation.bassRhythmId),
      static_cast<unsigned>(observation.chordRhythmId),
      static_cast<unsigned>(observation.progressionId),
      static_cast<unsigned>(observation.melodicRhythmId),
      static_cast<unsigned>(observation.motifShapeId),
      static_cast<unsigned>(observation.synthBRole),
      static_cast<unsigned>(observation.phraseLaw),
      static_cast<unsigned>(observation.phraseBars),
      static_cast<unsigned>(observation.effectiveMaterialFingerprint),
      static_cast<unsigned>(observation.provenance),
      observation.requestedResultEffective ? "true" : "false",
      static_cast<unsigned>(observation.previousMaterialFingerprint));
  return std::string(buffer);
}

inline bool equivalent(const GenerationObservation& left,
                       const GenerationObservation& right) {
  return toJson("observation", left) == toJson("observation", right);
}

}  // namespace GF2Measurement
}  // namespace GroovePuterRhythm
