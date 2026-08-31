#pragma once

#include "../../scenes.h"
#include "atlas_runtime.h"
#include "generated_phrase_p1r_materializer.h"
#include "mode_manager.h"
#include "phrase_generator.h"
#include "src/generation/migration/quantized_generation_commit.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/state/generation_request_state.h"
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace GeneratedPhraseSong {

constexpr uint32_t kGeneratedPhraseUndoTag = 0x44325048u;  // "D2PH"
constexpr int kMaxPreparedBars = 8;

enum class LifecycleStatus : uint8_t {
  Failed = 0,
  CommittedNow,
  PendingNextBar,
  Busy,
  TargetChanged,
  OutOfMemory,
};

struct Result {
  PhraseGenerator::PhraseResult phrase{};
  LifecycleStatus status = LifecycleStatus::Failed;
  GeneratedPhraseP1R::PreparationEvidence p1r{};

  explicit operator bool() const {
    return status == LifecycleStatus::CommittedNow ||
           status == LifecycleStatus::PendingNextBar;
  }
};

inline const char* statusText(const Result& result) {
  switch (result.status) {
    case LifecycleStatus::CommittedNow: return "PHRASE COMMITTED";
    case LifecycleStatus::PendingNextBar: return "PHRASE -> NEXT BAR";
    case LifecycleStatus::Busy: return "GENERATION BUSY";
    case LifecycleStatus::TargetChanged: return "PHRASE TARGET CHANGED";
    case LifecycleStatus::OutOfMemory: return "PHRASE PREPARE OOM";
    case LifecycleStatus::Failed:
      if (result.p1r.usedP1r) {
        return result.p1r.executionStatus ==
                       GroovePuterRhythm::PhraseExecutionStatus::Rejected
            ? "PHRASE LENGTH REJECTED"
            : "PHRASE EXEC FAILED";
      }
      return PhraseGenerator::errorText(result.phrase.error);
  }
  return "PHRASE ERROR";
}

// PMB-P1 bounded PREPARE: this is the compact plan PREPARE produces, and it
// deliberately holds NO 8-bar physical material array. PMB-A1/PMB-A2 proved
// (byte-level, host-tested, permanently regression-guarded) that every
// route's per-bar physical materialization is a pure, replayable function of
// this plan's fields, so PREPARE only needs to PROVE every bar will
// materialize (PREFLIGHT, one reused PhraseBar-sized scratch, discarded);
// COMMIT later replays the identical computation bar-by-bar and persists it
// immediately. See
// docs/contracts/0_9_9_PHRASE_PMB_P1_BOUNDED_PREPARE_COMMIT.md.
struct PreparedPhraseArrangement {
  PhraseGenerator::PhraseRequest request{};
  PhraseGenerator::PhraseResult result{};
  GroovePuterRhythm::QuantizedGenerationDetail::PatternTarget selectionTarget{};
  uint32_t baseRevision = 0;
  int songSlot = -1;
  int audibleSongRow = -1;
  int firstLocalSlot = -1;
  GeneratedPhraseP1R::PreparationEvidence p1r{};

  // Route + compact execution carrier, valid for the whole PREPARE->COMMIT
  // lifetime of one generate() call. Exactly one of the two shapes is used,
  // selected by useP1RRoute; genre is common to both (applyCurrentMigration
  // needs it on the legacy path).
  bool useP1RRoute = false;
  GroovePuterRhythm::PreparedPhraseExecution p1rExecution{};
  GenreSettings genre{};
  bool legacyAtlas = false;
  GenreRecipeId legacyRecipe = 0;
  GrooveboxMode legacyMappedMode = GrooveboxMode::Minimal;
  int legacyFlavor = 0;
  float legacyBpm = 0.0f;
  GenerativeParams legacyParams{};
  GenreBehavior legacyBehavior{};
};

struct GeneratedPhraseUndoPayload {
  uint32_t tag = kGeneratedPhraseUndoTag;
  Song beforeSong{};
  int16_t pageIndex = -1;
  int16_t songSlot = -1;
  int16_t songStart = -1;
  int16_t firstLocalSlot = -1;
  int16_t bars = 0;
  int16_t previousPatternBars = 1;
};

static_assert(std::is_trivially_copyable<GeneratedPhraseUndoPayload>::value,
              "generated Phrase Undo must remain fixed value state");
static_assert(sizeof(GeneratedPhraseUndoPayload) <=
                  GroovePuterUndo::kUndoPayloadBytes,
              "generated Phrase Undo must fit the canonical one-slot owner");
static_assert(std::is_trivially_copyable<PreparedPhraseArrangement>::value,
              "Phrase PREPARE staging must remain fixed size");
// PMB-P1 bound: the compact plan (route + execution state) must stay well
// under one PhraseGenerator::PhraseBar (1,416 B) -- if this ever grows past
// that, an 8-bar (or any per-bar) physical array has silently crept back in.
// See docs/contracts/0_9_9_PHRASE_PMB_P1_BOUNDED_PREPARE_COMMIT.md.
static_assert(sizeof(PreparedPhraseArrangement) <= 1024,
              "PMB-P1: PreparedPhraseArrangement must stay a compact plan, "
              "not per-bar physical material");

inline uint8_t atlasVariationForRole(PhraseGenerator::PhraseBarRole role) {
  switch (role) {
    case PhraseGenerator::PhraseBarRole::Base:
    case PhraseGenerator::PhraseBarRole::Return:
      return 0;
    case PhraseGenerator::PhraseBarRole::MicroVariation:
    case PhraseGenerator::PhraseBarRole::Development:
    case PhraseGenerator::PhraseBarRole::Build:
      return 1;
    case PhraseGenerator::PhraseBarRole::Breakdown:
    case PhraseGenerator::PhraseBarRole::Fill:
    case PhraseGenerator::PhraseBarRole::EndingFill:
      return 2;
  }
  return 0;
}

inline uint32_t phraseSeed(MiniAcid& engine,
                           int pageIndex,
                           int songStart,
                           int bars) {
  uint32_t seed = engine.modeManager().generationSeed();
  seed ^= static_cast<uint32_t>(pageIndex + 1) * 0x9E3779B9u;
  seed ^= static_cast<uint32_t>(songStart + 1) * 0x85EBCA6Bu;
  seed ^= static_cast<uint32_t>(bars + 1) * 0xC2B2AE35u;
  return seed == 0 ? 0x47525048u : seed;
}

inline GroovePuterRhythm::StrongRhythmMigrationContext migrationContextFor(
    const Scene& scene,
    int variationCoordinate,
    uint8_t phraseBarOrdinal) {
  GroovePuterRhythm::StrongRhythmMigrationContext context{};
  context.patternAddress = static_cast<int16_t>(variationCoordinate);
  context.level = GroovePuterState::currentGenerationLevel();
  const auto coordinates =
      GroovePuterRhythm::phraseTemporalCoordinatesForBar(phraseBarOrdinal);
  context.phraseBarOrdinal = coordinates.phraseBarOrdinal;
  context.evolutionOrdinal = coordinates.evolutionOrdinal;
  context.feelProfile = static_cast<GroovePuterRhythm::FeelProfileId>(
      scene.feel.timingProfile);

  float feelAmount = scene.generatorParams.microTimingAmount;
  if (feelAmount < 0.0f) feelAmount = 0.0f;
  if (feelAmount > 1.0f) feelAmount = 1.0f;
  context.feelAmount = static_cast<uint8_t>(feelAmount * 100.0f + 0.5f);

  int root = scene.generatorParams.scaleRoot % 12;
  if (root < 0) root += 12;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = static_cast<uint8_t>(root);
  context.scaleTypeValue = static_cast<GroovePuterRhythm::ScaleTypeValue>(
      scene.generatorParams.scale);
  return context;
}

inline void applyCurrentMigration(
    const Scene& scene,
    const GenreSettings& genre,
    int variationCoordinate,
    uint8_t phraseBarOrdinal,
    PhraseGenerator::PhraseBar& bar) {
  const auto context = migrationContextFor(
      scene, variationCoordinate, phraseBarOrdinal);
  (void)GroovePuterRhythm::migrateStrongRhythmMaterial(
      genre, context, bar.drums, bar.synthA, bar.synthB);
}

// PMB-P1 bounded legacy materialization: rebuilds bar `barIndex`'s content
// directly into the caller's single reused scratch buffer, from only the
// compact fields captured on `prepared` at PREPARE time (no persistent
// proceduralBase carrier -- PMB-A2 proved regenerating it fresh per bar is
// deterministic/idempotent, and PhraseGenerator::deriveBar tolerates
// base==output in-place aliasing). Called once per bar during PREFLIGHT
// (output discarded, inside prepareWithGenerationAttempt) and once per bar
// during COMMIT (output persisted, inside applyPreparedPersistent); PMB-A2
// proved both calls are byte-identical for the same inputs.
inline bool materializeLegacyBar(
    MiniAcid& engine,
    const Scene& scene,
    const PreparedPhraseArrangement& prepared,
    int barIndex,
    PhraseGenerator::PhraseBar& bar) {
  bar = PhraseGenerator::PhraseBar{};
  const GenreSettings& genre = prepared.genre;
  const auto role = PhraseGenerator::roleForBar(prepared.request.bars, barIndex);
  const uint8_t phraseBarOrdinal = static_cast<uint8_t>(barIndex);

  if (prepared.legacyAtlas) {
    const uint8_t variation = atlasVariationForRole(role);
    if (!AtlasRuntime::applyRecipe(
            prepared.legacyRecipe, variation,
            bar.synthA, bar.synthB, bar.drums, nullptr)) {
      return false;
    }
    applyCurrentMigration(scene, genre, variation, phraseBarOrdinal, bar);
    return true;
  }

  GrooveboxModeManager scratchMode(engine);
  scratchMode.setModeLocal(prepared.legacyMappedMode);
  scratchMode.setFlavorLocal(prepared.legacyFlavor);
  scratchMode.setGenerationSeed(prepared.request.seed);
  scratchMode.generatePattern(
      bar.synthA, prepared.legacyBpm, prepared.legacyParams, prepared.legacyBehavior, 0);
  scratchMode.generatePattern(
      bar.synthB, prepared.legacyBpm, prepared.legacyParams, prepared.legacyBehavior, 1);
  scratchMode.generateDrumPattern(
      bar.drums, prepared.legacyParams, prepared.legacyBehavior);

  applyCurrentMigration(scene, genre, 0, phraseBarOrdinal, bar);
  PhraseGenerator::deriveBar(bar, role, prepared.request.seed, barIndex, bar);
  return true;
}

inline bool exactPreparedSlotsRemainSafe(
    const Scene& scene,
    const PreparedPhraseArrangement& prepared) {
  if (prepared.firstLocalSlot < 0 ||
      prepared.firstLocalSlot + prepared.request.bars > kPatternsPerPage) {
    return false;
  }
  for (int bar = 0; bar < prepared.request.bars; ++bar) {
    if (!PhraseGenerator::localSlotIsSafeForPhrase(
            scene,
            prepared.request.pageIndex,
            prepared.firstLocalSlot + bar)) {
      return false;
    }
  }
  return true;
}

inline bool preparedTargetStillCommitSafe(
    MiniAcid& engine,
    const PreparedPhraseArrangement& prepared) {
  SceneManager& scenes = engine.sceneManager();
  const Scene& scene = scenes.currentScene();
  const auto revision = GroovePuterState::sceneRevisionSnapshot();
  if (revision.currentRevision != prepared.baseRevision ||
      engine.currentPageIndex() != prepared.request.pageIndex ||
      scenes.activeSongSlot() != prepared.songSlot ||
      !PhraseGenerator::songRowsAreAvailable(
          scene.songs[prepared.songSlot],
          prepared.request.songStart,
          prepared.request.bars) ||
      !exactPreparedSlotsRemainSafe(scene, prepared)) {
    return false;
  }

  if (engine.isPlaying()) {
    if (!engine.songModeEnabled() ||
        engine.songPlaybackSlot() != prepared.songSlot ||
        engine.currentSongPosition() != prepared.audibleSongRow ||
        !GroovePuterRhythm::QuantizedGenerationDetail::targetStillActive(
            scenes, prepared.selectionTarget)) {
      return false;
    }
  }
  return true;
}

// PMB-P1 bounded COMMIT: deterministically replays the exact same
// per-bar materialization PREFLIGHT already proved would succeed (PMB-A1
// for P1R, PMB-A2 for legacy), one reused PhraseBar-sized scratch at a
// time, persisting each bar immediately instead of reading it out of an
// already-populated 8-bar array. `engine` and `scene` must be the same
// engine/scene PREPARE ran against -- this whole PREPARE->COMMIT sequence
// is one synchronous, lease-protected call with no yielding in between, so
// nothing can mutate the inputs a materializer reads between PREFLIGHT and
// COMMIT (see docs/contracts/0_9_9_PHRASE_PMB_P1_BOUNDED_PREPARE_COMMIT.md).
inline void applyPreparedPersistent(
    MiniAcid& engine,
    Scene& scene,
    const PreparedPhraseArrangement& prepared) {
  Song& song = scene.songs[prepared.songSlot];
  PhraseGenerator::PhraseBar scratch{};
  for (int bar = 0; bar < prepared.request.bars; ++bar) {
    const int localSlot = prepared.firstLocalSlot + bar;
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    const int globalPattern = songPatternFromPageBankIndex(
        prepared.request.pageIndex, bank, index);

    if (prepared.useP1RRoute) {
      GeneratedPhraseP1R::materializeOneBar(
          engine, prepared.p1rExecution, static_cast<uint8_t>(bar),
          static_cast<int16_t>(globalPattern), scratch);
    } else {
      materializeLegacyBar(engine, scene, prepared, bar, scratch);
    }

    scene.synthABanks[bank].patterns[index] = scratch.synthA;
    scene.synthBBanks[bank].patterns[index] = scratch.synthB;
    scene.drumBanks[bank].patterns[index] = scratch.drums;

    SongPosition& position =
        song.positions[prepared.request.songStart + bar];
    position.patterns[static_cast<int>(SongTrack::SynthA)] =
        static_cast<int16_t>(globalPattern);
    position.patterns[static_cast<int>(SongTrack::SynthB)] =
        static_cast<int16_t>(globalPattern);
    position.patterns[static_cast<int>(SongTrack::Drums)] =
        static_cast<int16_t>(globalPattern);
  }
  song.length = std::max(
      song.length,
      prepared.request.songStart + prepared.request.bars);
  if (prepared.request.forceSingleBarRows) scene.feel.patternBars = 1;
}

inline GeneratedPhraseUndoPayload captureUndo(
    const Scene& scene,
    const PreparedPhraseArrangement& prepared) {
  GeneratedPhraseUndoPayload payload{};
  payload.beforeSong = scene.songs[prepared.songSlot];
  payload.pageIndex = static_cast<int16_t>(prepared.request.pageIndex);
  payload.songSlot = static_cast<int16_t>(prepared.songSlot);
  payload.songStart = static_cast<int16_t>(prepared.request.songStart);
  payload.firstLocalSlot = static_cast<int16_t>(prepared.firstLocalSlot);
  payload.bars = static_cast<int16_t>(prepared.request.bars);
  payload.previousPatternBars = static_cast<int16_t>(scene.feel.patternBars);
  return payload;
}

inline bool undoTargetAvailable(
    const SceneManager& scenes,
    const GeneratedPhraseUndoPayload& payload) {
  if (payload.tag != kGeneratedPhraseUndoTag ||
      payload.pageIndex < 0 || payload.pageIndex >= kMaxPages ||
      payload.songSlot < 0 || payload.songSlot > 1 ||
      payload.songStart < 0 || payload.bars < 1 || payload.bars > 8 ||
      payload.songStart + payload.bars > Song::kMaxPositions ||
      payload.firstLocalSlot < 0 ||
      payload.firstLocalSlot + payload.bars > kPatternsPerPage ||
      scenes.currentPageIndex() != payload.pageIndex) {
    return false;
  }

  const Scene& scene = scenes.currentScene();
  const Song& song = scene.songs[payload.songSlot];
  for (int bar = 0; bar < payload.bars; ++bar) {
    const int localSlot = payload.firstLocalSlot + bar;
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    const int expected = songPatternFromPageBankIndex(
        payload.pageIndex, bank, index);
    const SongPosition& row = song.positions[payload.songStart + bar];
    if (row.patterns[static_cast<int>(SongTrack::SynthA)] != expected ||
        row.patterns[static_cast<int>(SongTrack::SynthB)] != expected ||
        row.patterns[static_cast<int>(SongTrack::Drums)] != expected) {
      return false;
    }
  }
  return true;
}

inline void restoreUndo(
    SceneManager& scenes,
    const GeneratedPhraseUndoPayload& payload) {
  Scene& scene = scenes.currentScene();
  for (int bar = 0; bar < payload.bars; ++bar) {
    const int localSlot = payload.firstLocalSlot + bar;
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    scene.synthABanks[bank].patterns[index] = SynthPattern{};
    scene.synthBBanks[bank].patterns[index] = SynthPattern{};
    scene.drumBanks[bank].patterns[index] = DrumPatternSet{};
  }
  scene.songs[payload.songSlot] = payload.beforeSong;
  scene.feel.patternBars = payload.previousPatternBars;
}

inline bool ownsCurrentUndoReceipt() {
  auto& owner = GroovePuterUndo::undoOwner();
  if (owner.kind() != GroovePuterUndo::UndoKind::Generation ||
      owner.payloadSize() != sizeof(GeneratedPhraseUndoPayload)) {
    return false;
  }
  GeneratedPhraseUndoPayload payload{};
  return owner.read(GroovePuterUndo::UndoKind::Generation, payload) &&
         payload.tag == kGeneratedPhraseUndoTag;
}

template <typename Guard>
GroovePuterUndo::UndoResult undoLastGeneratedPhrase(
    MiniAcid& engine,
    Guard&& guard) {
  auto& owner = GroovePuterUndo::undoOwner();
  if (!ownsCurrentUndoReceipt()) {
    return GroovePuterUndo::UndoResult::TargetUnavailable;
  }

  GeneratedPhraseUndoPayload current{};
  if (!owner.read(GroovePuterUndo::UndoKind::Generation, current)) {
    return GroovePuterUndo::UndoResult::TargetUnavailable;
  }

  const uint32_t committedRevision = owner.committedRevision();
  const bool pending =
      GroovePuterRhythm::PhraseLiveArrangementDetail::
          hasPendingPhraseActivationForRevision(engine, committedRevision);
  const bool generatedTargetAudible = engine.isPlaying() &&
      engine.songModeEnabled() &&
      engine.songPlaybackSlot() == current.songSlot &&
      engine.currentSongPosition() >= current.songStart &&
      engine.currentSongPosition() < current.songStart + current.bars;
  if (generatedTargetAudible && !pending) {
    return GroovePuterUndo::UndoResult::TargetUnavailable;
  }

  auto&& applyGuard = guard;
  const auto result = owner.undoPrepared<GeneratedPhraseUndoPayload>(
      GroovePuterUndo::UndoKind::Generation,
      [&](const GeneratedPhraseUndoPayload& payload) {
        return undoTargetAvailable(engine.sceneManager(), payload);
      },
      [&](const GeneratedPhraseUndoPayload& payload) {
        const auto restore = [&]() {
          restoreUndo(engine.sceneManager(), payload);
        };
        applyGuard(restore);
      });

  if (result == GroovePuterUndo::UndoResult::Restored) {
    (void)GroovePuterRhythm::PhraseLiveArrangementDetail::
        cancelPendingPhraseActivationForRevision(engine, committedRevision);
  }
  return result;
}

inline bool prepareWithGenerationAttempt(
    MiniAcid& engine,
    uint8_t bars,
    int songStart,
    uint32_t generationAttemptOrdinal,
    bool attemptAvailable,
    PreparedPhraseArrangement& prepared) {
  SceneManager& scenes = engine.sceneManager();
  const Scene& scene = scenes.currentScene();

  prepared = PreparedPhraseArrangement{};
  prepared.request.bars = bars;
  prepared.request.songStart = songStart;
  prepared.request.pageIndex = engine.currentPageIndex();
  prepared.request.seed = phraseSeed(
      engine, prepared.request.pageIndex, songStart, bars);
  prepared.request.forceSingleBarRows = true;
  prepared.songSlot = std::clamp(scene.activeSongSlot, 0, 1);
  prepared.audibleSongRow = engine.currentSongPosition();
  prepared.baseRevision =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;
  prepared.selectionTarget =
      GroovePuterRhythm::QuantizedGenerationDetail::captureTarget(scenes);

  prepared.result.bars = bars;
  prepared.result.songStart = songStart;

  if (!PhraseGenerator::isSupportedLength(bars)) {
    prepared.result.error = PhraseGenerator::PhraseError::UnsupportedLength;
    return false;
  }
  if (prepared.request.pageIndex < 0 ||
      prepared.request.pageIndex >= kMaxPages) {
    prepared.result.error = PhraseGenerator::PhraseError::InvalidPage;
    return false;
  }
  if (songStart < 0 || songStart + bars > Song::kMaxPositions) {
    prepared.result.error = PhraseGenerator::PhraseError::SongOutOfRange;
    return false;
  }
  if (!PhraseGenerator::songRowsAreAvailable(
          scene.songs[prepared.songSlot], songStart, bars)) {
    prepared.result.error = PhraseGenerator::PhraseError::SongRowsOccupied;
    return false;
  }

  prepared.firstLocalSlot = PhraseGenerator::findSafeContiguousEmptySlots(
      scene, prepared.request.pageIndex, bars);
  if (prepared.firstLocalSlot < 0) {
    prepared.result.error =
        PhraseGenerator::PhraseError::NoContiguousPatternSlots;
    return false;
  }

  const GenreSettings genre = scene.genre;
  prepared.genre = genre;
  const auto p1rDisposition = GeneratedPhraseP1R::prepare(
      engine,
      scene,
      genre,
      bars,
      prepared.request.pageIndex,
      prepared.firstLocalSlot,
      generationAttemptOrdinal,
      attemptAvailable,
      prepared.p1rExecution,
      prepared.p1r);
  if (p1rDisposition == GeneratedPhraseP1R::PreparationDisposition::Failed) {
    prepared.result.error = PhraseGenerator::PhraseError::GenerationFailed;
    return false;
  }
  if (p1rDisposition == GeneratedPhraseP1R::PreparationDisposition::Ready) {
    prepared.useP1RRoute = true;
    prepared.result.error = PhraseGenerator::PhraseError::None;
    prepared.result.firstLocalSlot = prepared.firstLocalSlot;
    prepared.result.firstGlobalPattern = songPatternFromPageBankIndex(
        prepared.request.pageIndex,
        prepared.firstLocalSlot / Bank<SynthPattern>::kPatterns,
        prepared.firstLocalSlot % Bank<SynthPattern>::kPatterns);
    return true;
  }

  // Legacy strong-rhythm routes retain the frozen D2 physical preparer exactly.
  // P1R-capable routes never silently fall back here after a typed execution
  // rejection/failure.
  auto& genreManager = engine.genreManager();
  prepared.useP1RRoute = false;
  prepared.legacyRecipe = genreManager.recipe();
  const GenerativeMode activeGenre = genreManager.generativeMode();
  prepared.legacyParams = genreManager.getCompiledGenerativeParams();
  prepared.legacyBehavior = genreManager.getBehavior();
  prepared.legacyMappedMode = GenreManager::grooveboxModeForRecipe(
      prepared.legacyRecipe, activeGenre);
  prepared.legacyFlavor = engine.modeManager().flavor();
  prepared.legacyBpm = engine.bpm();
  prepared.legacyAtlas = AtlasRuntime::hasRecipe(prepared.legacyRecipe) &&
      AtlasRuntime::variationCount(prepared.legacyRecipe) >= 3;

  // PREFLIGHT: prove every bar materializes before any physical destination
  // is touched. Bounded to one reused PhraseBar-sized scratch (no 8-bar
  // array) -- COMMIT (applyPreparedPersistent) later calls
  // materializeLegacyBar again, bar by bar, and PMB-A2 proved that replay is
  // byte-identical to this preflight.
  PhraseGenerator::PhraseBar preflightScratch{};
  for (int barIndex = 0; barIndex < bars; ++barIndex) {
    if (!materializeLegacyBar(engine, scene, prepared, barIndex, preflightScratch)) {
      prepared.result.error = PhraseGenerator::PhraseError::GenerationFailed;
      return false;
    }
  }

  prepared.result.error = PhraseGenerator::PhraseError::None;
  prepared.result.firstLocalSlot = prepared.firstLocalSlot;
  prepared.result.firstGlobalPattern = songPatternFromPageBankIndex(
      prepared.request.pageIndex,
      prepared.firstLocalSlot / Bank<SynthPattern>::kPatterns,
      prepared.firstLocalSlot % Bank<SynthPattern>::kPatterns);
  return true;
}

inline bool prepare(
    MiniAcid& engine,
    uint8_t bars,
    int songStart,
    PreparedPhraseArrangement& prepared) {
  return prepareWithGenerationAttempt(
      engine, bars, songStart, 0u, false, prepared);
}

template <typename Guard>
Result generate(
    MiniAcid& engine,
    uint8_t bars,
    int songStart,
    Guard&& guard) {
  Result output{};
  output.phrase.bars = bars;
  output.phrase.songStart = songStart;

  using namespace GroovePuterRhythm::QuantizedGenerationDetail;
  const WriteLease lease = acquireWriteLease();
  if (lease.slot < 0) {
    output.status = LifecycleStatus::Busy;
    return output;
  }

  // PMB-P1: PreparedPhraseArrangement no longer holds an 8-bar physical
  // material array (see docs/contracts/0_9_9_PHRASE_PMB_P1_BOUNDED_PREPARE_
  // COMMIT.md), so it is small and bounded enough to live on the stack --
  // no heap allocation, and therefore no OutOfMemory path can be reached
  // here anymore. LifecycleStatus::OutOfMemory/statusText's "PHRASE PREPARE
  // OOM" case remain defined for API completeness (UI-P0 characterized
  // them), but nothing sets that status on this path any longer.
  PreparedPhraseArrangement preparedStorage{};
  PreparedPhraseArrangement* const prepared = &preparedStorage;

  if (engine.isPlaying() &&
      (!engine.songModeEnabled() ||
       engine.songPlaybackSlot() !=
           std::clamp(engine.sceneManager().activeSongSlot(), 0, 1))) {
    releaseWriteSlot(lease.slot);
    output.status = LifecycleStatus::TargetChanged;
    return output;
  }

  uint32_t generationAttemptOrdinal = 0;
  bool attemptAvailable = false;
  const GenreSettings genre = engine.sceneManager().currentScene().genre;
  if (GroovePuterRhythm::selectStrongRhythmRoute(genre) !=
      GroovePuterRhythm::StrongRhythmRoute::Legacy) {
    const auto attempt = GroovePuterState::allocateGenerationAttempt(
        genre.generativeMode,
        genre.recipe,
        GroovePuterState::currentGenerationLevel(),
        GeneratedPhraseP1R::kLogicalPhraseAttemptChannel);
    if (!attempt.ok()) {
      releaseWriteSlot(lease.slot);
      output.p1r.usedP1r = true;
      output.p1r.executionStatus =
          GroovePuterRhythm::PhraseExecutionStatus::InvalidContext;
      output.status = LifecycleStatus::Failed;
      return output;
    }
    generationAttemptOrdinal = attempt.ordinal;
    attemptAvailable = true;
  }

  if (!prepareWithGenerationAttempt(
          engine,
          bars,
          songStart,
          generationAttemptOrdinal,
          attemptAvailable,
          *prepared)) {
    releaseWriteSlot(lease.slot);
    output.phrase = prepared->result;
    output.p1r = prepared->p1r;
    output.status = LifecycleStatus::Failed;
    return output;
  }
  output.phrase = prepared->result;
  output.p1r = prepared->p1r;

  if (!preparedTargetStillCommitSafe(engine, *prepared)) {
    releaseWriteSlot(lease.slot);
    output.status = LifecycleStatus::TargetChanged;
    return output;
  }

  const GeneratedPhraseUndoPayload before = captureUndo(
      engine.sceneManager().currentScene(), *prepared);
  auto&& applyGuard = guard;

  if (engine.isPlaying()) {
    if (!GroovePuterRhythm::PhraseLiveArrangementDetail::armPhraseActivation(
            engine,
            lease.slot,
            prepared->selectionTarget,
            prepared->songSlot,
            prepared->request.songStart,
            prepared->request.bars,
            prepared->audibleSongRow)) {
      releaseWriteSlot(lease.slot);
      output.status = LifecycleStatus::TargetChanged;
      return output;
    }
  }

  const bool committed = GroovePuterUndo::undoOwner().commitPrepared(
      GroovePuterUndo::UndoKind::Generation,
      before,
      [&]() {
        const auto apply = [&]() {
          applyPreparedPersistent(
              engine, engine.sceneManager().currentScene(), *prepared);
        };
        applyGuard(apply);
      });

  if (!committed) {
    if (engine.isPlaying()) {
      GroovePuterRhythm::PhraseLiveArrangementDetail::abortPhraseActivation(
          lease.slot, GroovePuterRhythm::QuantizedGenerationStatus::Busy);
    } else {
      releaseWriteSlot(lease.slot);
    }
    output.status = LifecycleStatus::Busy;
    return output;
  }

  if (!engine.isPlaying()) {
    releaseWriteSlot(lease.slot);
    engine.setSongMode(true);
    engine.setSongPlaybackSlot(prepared->songSlot);
    engine.setSongPosition(prepared->request.songStart);
    output.status = LifecycleStatus::CommittedNow;
    return output;
  }

  const uint32_t committedRevision =
      GroovePuterUndo::undoOwner().committedRevision();
  GroovePuterRhythm::PhraseLiveArrangementDetail::completePhraseActivation(
      lease.slot, committedRevision);
  output.status = LifecycleStatus::PendingNextBar;
  return output;
}

inline std::size_t preparedPhraseArrangementSize() {
  return sizeof(PreparedPhraseArrangement);
}

inline std::size_t generatedPhraseUndoPayloadSize() {
  return sizeof(GeneratedPhraseUndoPayload);
}

}  // namespace GeneratedPhraseSong
