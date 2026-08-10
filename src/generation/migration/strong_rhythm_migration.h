#pragma once

#include <cstdint>

#include "../../../scenes.h"
#include "../composition/rhythm_selection.h"
#include "../feel/feel_pattern_adapter.h"
#include "../materialization/pattern_materializer.h"
#include "../rhythm/reference_vocabulary.h"
#include "../roles/bass_rhythm.h"
#include "../roles/chord_rhythm.h"
#include "../roles/melodic_motif.h"
#include "../roles/semantic_pattern_projector.h"

namespace GroovePuterRhythm {

enum class StrongRhythmRoute : uint8_t {
  Legacy = 0,
  AcidBase,
  TechnoBase,
  RaveBase,
  DrumAndBass,
  DubTechno,
  ChicagoJack,
  RollingAcid,
  DeepChord,
  Stage7Composition,
  Count,
};

enum class StrongRhythmMigrationStatus : uint8_t {
  Legacy = 0,
  Applied,
  InvalidContext,
  RealizationFailed,
  MaterializationFailed,
  CompatibilityBindingFailed,
  FeelApplyFailed,
  Count,
};

enum class SemanticSynthBRole : uint8_t {
  Chord = 0,
  Melodic,
  Count,
};

struct StrongRhythmMigrationContext {
  // Existing pattern address is an explicit deterministic variation coordinate.
  // Stage 5 does not add a persisted backend/seed/ordinal to Scene.
  int16_t patternAddress = 0;
  RealizationLevel level = RealizationLevel::P2Variation;
  FeelProfileId feelProfile = FeelProfileId::Straight;
  uint8_t feelAmount = 0;
};

struct StrongRhythmMigrationResult {
  StrongRhythmMigrationStatus status = StrongRhythmMigrationStatus::Legacy;
  StrongRhythmRoute route = StrongRhythmRoute::Legacy;
  ReferenceVocabulary::Archetype archetype =
      ReferenceVocabulary::Archetype::Count;
  RhythmSelectionMode selectionMode = RhythmSelectionMode::Auto;
  bool normalizedSelectionToAuto = false;
  RealizationStatus realizationStatus = RealizationStatus::InvalidConstraintSet;
  PatternMaterializeStatus materializationStatus =
      PatternMaterializeStatus::InvalidPlan;
  FeelPatternApplyStatus feelStatus = FeelPatternApplyStatus::Ok;
  BassRhythmStatus bassRhythmStatus = BassRhythmStatus::InvalidRequest;
  BassRhythmId bassRhythmId = BassRhythmId::Auto;
  SemanticPatternProjectStatus bassProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  FeelInterpretStatus bassFeelStatus = FeelInterpretStatus::Ok;
  ChordRhythmStatus chordRhythmStatus = ChordRhythmStatus::InvalidRequest;
  ChordRhythmId chordRhythmId = ChordRhythmId::Auto;
  SemanticPatternProjectStatus chordProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  FeelInterpretStatus chordFeelStatus = FeelInterpretStatus::Ok;
  MelodicMotifStatus melodicMotifStatus =
      MelodicMotifStatus::InvalidRequest;
  MelodicRhythmId melodicRhythmId = MelodicRhythmId::Auto;
  MotifShapeId motifShapeId = MotifShapeId::Auto;
  SemanticPatternProjectStatus melodicProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  FeelInterpretStatus melodicFeelStatus = FeelInterpretStatus::Ok;
  SemanticSynthBRole synthBRole = SemanticSynthBRole::Chord;

  // Ephemeral semantic topology. It is never persisted and carries no
  // pitch/voicing/physical-engine ownership.
  StepMask chordOnsets = 0;
  bool chordRhythmApplied = false;
  bool melodicRhythmApplied = false;
};

// Explicit Stage 5 allow-list. A non-zero recipe is authoritative: unsupported
// recipes remain legacy even when their base generative mode is a strong style.
// Cross-recipe morphs also remain legacy until GenreGenerationProfile owns
// weighted vocabulary selection.
StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings);

// Transactional drum-only primitive. The destination is untouched for
// Legacy/failure. On success only drum voices are replaced; legacy automation
// lanes and PatternGroove remain authoritative. chordOnsets exposes only the
// already-realized ChordRhythm topology for the compatibility adapter below.
StrongRhythmMigrationResult migrateStrongRhythmDrums(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& destination);

// Cross-role production transaction. Legacy generation supplies both pitch
// phrases; BassRhythm/ChordRhythm own only onset and continuation topology.
// Drums + Synth A + Synth B commit atomically after every semantic plan,
// projection and Feel step succeeds.
StrongRhythmMigrationResult migrateStrongRhythmMaterial(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB);

}  // namespace GroovePuterRhythm
