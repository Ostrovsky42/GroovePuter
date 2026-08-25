#include "e3_listen_fixture_player.h"

#include "../../../scenes.h"
#include "../../dsp/genre_manager.h"
#include "../../dsp/miniacid_engine.h"
#include "e3_listen_fixture_generated.h"
#include "strong_rhythm_migration.h"

namespace GroovePuterRhythm {
namespace {

constexpr int kReviewBank = 1;
constexpr int kReviewPattern = 0;
constexpr int kReviewSongSlot = 1;
constexpr uint8_t kReviewBars = 4;
constexpr float kReviewBpm = 124.0f;
constexpr int16_t kReviewPatternAddress = 7;

GenreSettings reviewSettings() {
  GenreSettings settings{};
  // House is only a stable downstream tonal/profile context. The staged
  // review hook pins the frozen E3R-B archetype before any rhythm owner uses
  // the composition result.
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::House);
  settings.recipe = kBaseRecipeId;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext reviewContext(RealizationLevel level) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = kReviewPatternAddress;
  context.level = level;
  context.generationAttemptOrdinal = 0;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kDefaultScaleTypeValue;
  return context;
}

}  // namespace

uint8_t e3ListenCaseCount() {
  return E3ListenFixtureData::kCaseCount;
}

E3ListenCaseInfo e3ListenCaseInfo(uint8_t index) {
  E3ListenCaseInfo result{};
  if (index >= E3ListenFixtureData::kCaseCount) return result;
  const auto& source = E3ListenFixtureData::kCases[index];
  result.caseId = source.caseId;
  result.category = source.category;
  result.family = source.family;
  result.level = source.level == 1 ? "P2" : "P3";
  result.operation = source.operation;
  result.role = source.role;
  result.roleIndex = source.roleIndex;
  result.sourceStep = source.sourceStep;
  result.targetStep = source.targetStep;
  result.sourceClass = source.sourceClass;
  result.sourceKind = source.sourceKind;
  result.distance = source.distance;
  result.densityBefore = source.densityBefore;
  result.densityAfter = source.densityAfter;
  result.mutatedRoleExact = source.mutatedRoleExact;
  return result;
}

const char* e3ListenVariantName(E3ListenVariant variant) {
  switch (variant) {
    case E3ListenVariant::Canonical: return "C";
    case E3ListenVariant::Before: return "V";
    case E3ListenVariant::After: return "W";
    case E3ListenVariant::Count: break;
  }
  return "?";
}

const char* e3ListenAudibilityClassName(E3ListenAudibilityClass value) {
  switch (value) {
    case E3ListenAudibilityClass::ProductionContextAudition:
      return "CTX AUDITION";
  }
  return "INVALID";
}

bool applyE3ListenCase(MiniAcid& engine,
                       uint8_t index,
                       E3ListenVariant variant) {
  if (index >= E3ListenFixtureData::kCaseCount ||
      static_cast<uint8_t>(variant) >=
          static_cast<uint8_t>(E3ListenVariant::Count)) {
    return false;
  }

  const auto& meta = E3ListenFixtureData::kCases[index];
  const RealizationLevel level =
      static_cast<RealizationLevel>(meta.level);

  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};

  configureE3ListenReview(index, variant);
  const StrongRhythmMigrationResult migration =
      migrateStrongRhythmMaterial(
          reviewSettings(), reviewContext(level), drums, synthA, synthB);
  disableE3ListenReview();

  if (migration.status != StrongRhythmMigrationStatus::Applied) {
    return false;
  }

  SceneManager& manager = engine.sceneManager();

  // Deterministic side switch contract: stop, install one frozen side into the
  // explicit review sandbox, reset transport to row/bar zero, then start.
  engine.stop();
  engine.setSongMode(false);
  engine.setDrumBankIndex(kReviewBank);
  engine.setDrumPatternIndex(kReviewPattern);
  engine.set303BankIndex(0, kReviewBank);
  engine.set303BankIndex(1, kReviewBank);
  engine.set303PatternIndex(0, kReviewPattern);
  engine.set303PatternIndex(1, kReviewPattern);

  manager.editDrumPatternSet(kReviewPattern) = drums;
  manager.editSynthPattern(0, kReviewPattern) = synthA;
  manager.editSynthPattern(1, kReviewPattern) = synthB;

  const int patternAddress = songPatternFromPageBankIndex(
      engine.currentPageIndex(), kReviewBank, kReviewPattern);
  if (patternAddress < 0) return false;

  engine.setActiveSongSlot(kReviewSongSlot);
  for (uint8_t row = 0; row < kReviewBars; ++row) {
    engine.setSongPattern(row, SongTrack::SynthA, patternAddress);
    engine.setSongPattern(row, SongTrack::SynthB, patternAddress);
    engine.setSongPattern(row, SongTrack::Drums, patternAddress);
  }
  engine.setSongLength(kReviewBars);
  engine.setSongPosition(0);
  engine.setLoopRange(0, kReviewBars - 1);
  engine.setLoopMode(true);
  engine.setSongPlaybackSlot(kReviewSongSlot);
  engine.setBpm(kReviewBpm);
  engine.setSongMode(true);
  engine.start();
  return true;
}

}  // namespace GroovePuterRhythm
