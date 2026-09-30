#pragma once

#include "../../scenes.h"
#include "atlas_runtime.h"
#include "generated_phrase_p1r_materializer.h"
#include "mode_manager.h"
#include "phrase_generator.h"
#include "slot_reuse.h"
#include "src/state/generated_phrase_undo_payload.h"
#include "src/audio/pattern_paging.h"
#include "src/generation/migration/quantized_generation_commit.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/state/generated_phrase_recipe.h"
#include "src/state/generated_synth_a_origin.h"
#include "src/state/generation_request_state.h"
#include "src/state/material_slot_access.h"
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace GeneratedPhraseSong {

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
  int16_t songSlot = -1;
  int16_t audibleSongRow = -1;
  int16_t firstLocalSlot = -1;
  int16_t legacyFlavor = 0;
  GroovePuterMaterial::MaterialIdReservation synthAReservation{};
  GeneratedPhraseP1R::PreparationEvidence p1r{};

  // Route + compact execution carrier, valid for the whole PREPARE->COMMIT
  // lifetime of one generate() call. Exactly one of the two shapes is used,
  // selected by useP1RRoute; genre is common to both (applyCurrentMigration
  // needs it on the legacy path).
  bool useP1RRoute = false;
  bool legacyAtlas = false;
  GenreRecipeId legacyRecipe = 0;
  GrooveboxMode legacyMappedMode = GrooveboxMode::Minimal;
  GroovePuterRhythm::PreparedPhraseExecution p1rExecution{};
  GenreSettings genre{};
  float legacyBpm = 0.0f;
  GenerativeParams legacyParams{};
  GenreBehavior legacyBehavior{};

  GroovePuterMaterial::MaterialId synthAId(uint8_t bar) const {
    return synthAReservation.idAt(bar);
  }
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
    MiniAcid& engine,
    const Scene& scene,
    const PreparedPhraseArrangement& prepared) {
  if (prepared.firstLocalSlot < 0 ||
      prepared.firstLocalSlot + prepared.request.bars > kPatternsPerPage) {
    return false;
  }
  for (int bar = 0; bar < prepared.request.bars; ++bar) {
    // Free now, or a marked slot whose token, id and holders are re-verified here (PML-C).
    if (!SlotReuse::usable(engine, scene, prepared.firstLocalSlot + bar)) return false;
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
      !exactPreparedSlotsRemainSafe(engine, scene, prepared)) {
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
// D1-B: the common part of the unpublished origin candidate comes straight
// from PREPARE's owner data (never from the generated notes). Per-bar bass
// evidence is filled during COMMIT; nothing is visible to the engine until
// commitPrepared succeeds.
inline GroovePuterMaterial::GeneratedSynthAOriginCandidate
beginOriginCandidate(const PreparedPhraseArrangement& prepared) {
  GroovePuterMaterial::GeneratedSynthAOriginCandidate candidate{};
  if (!prepared.useP1RRoute) {
    candidate.failed = true;  // Legacy never claims P1R evidence
    return candidate;
  }
  auto& common = candidate.origin.common;
  common.phraseGenerationIdentity = prepared.p1rExecution.phraseGenerationIdentity;
  common.barCount = static_cast<uint8_t>(prepared.request.bars);
  common.rootPitchClass = prepared.p1rExecution.materialization.rootPitchClass;
  common.scaleTypeValue = prepared.p1rExecution.materialization.scaleTypeValue;
  common.progressionSource = prepared.p1rExecution.progressionSource;
  return candidate;
}

// When `candidate` is non-null and the route is P1R, per-bar origin is filled
// from the exact evidence the materializer returned for that bar; any bar
// without valid evidence poisons the candidate (fail closed).
inline void applyPreparedPersistent(
    MiniAcid& engine,
    Scene& scene,
    const PreparedPhraseArrangement& prepared,
    GroovePuterMaterial::GeneratedSynthAOriginCandidate* candidate = nullptr,
    // P0 cycle: bars >= sectionBars come from `secondSection` (bar - sectionBars);
    // both sections are prepared from one recipe. Null keeps the single-execution shape.
    const GroovePuterRhythm::PreparedPhraseExecution* secondSection = nullptr,
    int sectionBars = 0) {
  Song& song = scene.songs[prepared.songSlot];
  PhraseGenerator::PhraseBar scratch{};
  for (int bar = 0; bar < prepared.request.bars; ++bar) {
    const bool inSecond = secondSection != nullptr && bar >= sectionBars;
    const GroovePuterRhythm::PreparedPhraseExecution& execution =
        inSecond ? *secondSection : prepared.p1rExecution;
    const int sectionBar = inSecond ? bar - sectionBars : bar;
    const int localSlot = prepared.firstLocalSlot + bar;
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    const int globalPattern = songPatternFromPageBankIndex(
        prepared.request.pageIndex, bank, index);

    GeneratedPhraseP1R::MaterializedSynthABarEvidence barEvidence{};
    if (prepared.useP1RRoute) {
      GeneratedPhraseP1R::materializeOneBar(
          engine, execution, static_cast<uint8_t>(sectionBar),
          static_cast<int16_t>(globalPattern), scratch, barEvidence);
    } else {
      materializeLegacyBar(engine, scene, prepared, bar, scratch);
    }

    // PML-C: a slot taken through an "allow replacement" mark is emptied (patterns AND both
    // descriptors) and the mark ends; for a slot that was free this is a no-op.
    SlotReuse::reclaim(engine, scene, localSlot);
    scene.synthABanks[bank].patterns[index] = scratch.synthA;
    scene.synthBBanks[bank].patterns[index] = scratch.synthB;
    scene.drumBanks[bank].patterns[index] = scratch.drums;
    // Song playback reads Synth events from the derived runtime bank, not from the Pattern:
    // publish this slot's events together with the Pattern (inside the audio guard), or the
    // engine keeps playing whatever the slot held before.
    (void)engine.refreshPatternRuntimeEvents(0, bank, index);
    (void)engine.refreshPatternRuntimeEvents(1, bank, index);

    // Session ledger: this generator wrote the slot and left exactly this content (MAKE ROOM offers
    // only slots whose content still equals it).
    engine.generatedLedgerForReuseModule().set(
        localSlot, prepared.request.pageIndex,
        GroovePuterMaterial::slotContentToken(scene, localSlot));

    // D1-A Checkpoint A1: canonical Synth A Material identity publication
    GroovePuterMaterial::setResidentDescriptor(
        scene, 0, localSlot,
        GroovePuterMaterial::MaterialKind::Pattern,
        prepared.synthAReservation.idAt(static_cast<uint8_t>(bar)));

    if (candidate != nullptr && prepared.useP1RRoute) {
      const auto id = prepared.synthAReservation.idAt(static_cast<uint8_t>(bar));
      if (!barEvidence.valid || barEvidence.phraseBarOrdinal != sectionBar ||
          !id.valid()) {
        candidate->failed = true;
      } else {
        auto& entry = candidate->origin.bars[bar];
        entry.material = GroovePuterMaterial::MaterialReference{
            GroovePuterMaterial::MaterialAddress{
                0, static_cast<uint8_t>(globalPattern)},
            id};
        entry.originPatternVersion = GroovePuterMaterial::versionForPattern(
            scene.synthABanks[bank].patterns[index]);
        entry.bassRhythm = barEvidence.bassRhythm;
        entry.bassPitchClasses = barEvidence.bassPitchClasses;
        entry.harmonicRhythm = execution.harmonicClock.bars[sectionBar].harmonicRhythm;
        entry.phraseBarOrdinal = static_cast<uint8_t>(bar);
        ++candidate->filledBars;
      }
    }

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

inline GroovePuterMaterial::GeneratedPhraseRecipe recipeFor(
    MiniAcid& engine,
    const PreparedPhraseArrangement& prepared) {
  GroovePuterMaterial::GeneratedPhraseRecipe recipe{};
  recipe.genre = prepared.p1rExecution.settings;
  recipe.materialization = prepared.p1rExecution.materialization;
  recipe.phraseGenerationIdentity = prepared.p1rExecution.phraseGenerationIdentity;
  recipe.contextFingerprint = GeneratedPhraseP1R::pitchSourceFingerprint(engine);
  recipe.pageIndex = static_cast<int16_t>(prepared.request.pageIndex);
  recipe.songSlot = prepared.songSlot;
  recipe.songStart = static_cast<int16_t>(prepared.request.songStart);
  recipe.firstLocalSlot = prepared.firstLocalSlot;
  recipe.bars = static_cast<uint8_t>(prepared.request.bars);
  return recipe;
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
    GroovePuterMaterial::clearResidentDescriptor(scene, 0, localSlot);
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
          // Cleared slots must not keep playing the undone events.
          for (int bar = 0; bar < payload.bars; ++bar) {
            const int localSlot = payload.firstLocalSlot + bar;
            const int bank = localSlot / Bank<SynthPattern>::kPatterns;
            const int index = localSlot % Bank<SynthPattern>::kPatterns;
            (void)engine.refreshPatternRuntimeEvents(0, bank, index);
            (void)engine.refreshPatternRuntimeEvents(1, bank, index);
          }
        };
        applyGuard(restore);
      });

  if (result == GroovePuterUndo::UndoResult::Restored) {
    // D1-B: the undone phrase is the latest generated one, i.e. the one the
    // sidecar describes. Fail closed to "no rich origin"; an older sidecar is
    // deliberately not restored (no ~300 B added to the Undo payload).
    engine.clearGeneratedSynthAOrigin();
    // P0: Undo of the phrase itself drops the recipe; Undo of the cycle keeps it so
    // the action can be repeated.
    if (const auto* recipe = engine.generatedPhraseRecipe()) {
      if (current.songStart == recipe->songStart) {
        engine.clearGeneratedPhraseRecipe();
      } else if (current.songStart == recipe->cycleSongStart) {
        engine.setGeneratedPhraseCycleStart(-1);
      }
    }
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

  prepared.firstLocalSlot = SlotReuse::findRun(
      engine, scene, prepared.request.pageIndex, bars);
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

  // Pre-commit canonical MaterialId reservation:
  // Durable SD high-water update occurs OUTSIDE the audio publication critical section.
  const auto reservation =
      PatternPagingService::reserveMaterialIds(prepared->request.bars);
  if (!reservation.valid()) {
    releaseWriteSlot(lease.slot);
    output.status = LifecycleStatus::Failed;
    return output;
  }
  prepared->synthAReservation = reservation;

  const GeneratedPhraseUndoPayload before = captureUndo(
      engine.sceneManager().currentScene(), *prepared);
  auto&& applyGuard = guard;
  // D1-B: unpublished origin candidate (stack, no heap); visible to the engine
  // only after commitPrepared succeeds below.
  GroovePuterMaterial::GeneratedSynthAOriginCandidate originCandidate =
      beginOriginCandidate(*prepared);

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
              engine, engine.sceneManager().currentScene(), *prepared,
              &originCandidate);
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

  // D1-B: physical Material committed. Publish P1R origin only if every bar
  // carried valid owner evidence; a Legacy (or evidence-less) generation makes
  // "latest generated phrase" provenance-free, so the old sidecar is cleared
  // rather than left describing a phrase that is no longer the latest.
  if (!(prepared->useP1RRoute && engine.publishGeneratedSynthAOrigin(originCandidate))) {
    engine.clearGeneratedSynthAOrigin();
  }
  // P0: the recipe of the latest generated phrase; Legacy has none.
  if (prepared->useP1RRoute) {
    engine.publishGeneratedPhraseRecipe(recipeFor(engine, *prepared));
  } else {
    engine.clearGeneratedPhraseRecipe();
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

// ---------------------------------------------------------------------------
// P0: DEVELOP + BREAK cycle for the kept generated phrase.
//
// One engine action, no UI. Both 4-bar sections are prepared from the ONE stored recipe of
// the kept phrase (never from each other), verified (R0 replay of the kept phrase, R1
// context), and published as eight bars after it in a single Undo receipt. The kept phrase
// is not modified. See docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md sections 2-3.
// ---------------------------------------------------------------------------
enum class CycleStatus : uint8_t {
  CommittedNow = 0,
  PendingNextBar,
  NoRecipe,             // no generated phrase known (never generated, Undone, Legacy, scene load)
  CycleAlreadyPublished,
  NothingToAdd,         // both requested sections would repeat the kept phrase
  NotAdmitted,          // archetype/scenario admits no evolution (Acid, House, no trajectory)
  DepthNotP3,           // developing a P2 phrase would change law and depth at once
  ContextChanged,       // R1: genre, tonal or pitch-source context differs from the kept phrase
  EditedSinceGeneration,  // R0: the kept phrase no longer equals its rebuild
  NoSafeSlots,
  RowsOccupied,
  ReservationFailed,
  TargetChanged,
  Busy,
  Failed,
};

constexpr uint8_t kCycleSectionBars = 4;

struct CycleResult {
  CycleStatus status = CycleStatus::Failed;
  // The kept phrase's own law already is DevelopReturn (or SparseDrift): that section would
  // repeat it, so it is not published. `bars` is what was published (4 or 8).
  bool developSkipped = false;
  bool breakSkipped = false;
  uint8_t bars = 0;
  explicit operator bool() const {
    return status == CycleStatus::CommittedNow ||
           status == CycleStatus::PendingNextBar;
  }
};

inline bool sameGenre(const GenreSettings& a, const GenreSettings& b) {
  return a.generativeMode == b.generativeMode && a.recipe == b.recipe &&
         a.morphTarget == b.morphTarget && a.morphAmount == b.morphAmount &&
         a.rhythmSelectionMode == b.rhythmSelectionMode &&
         a.rhythmArchetypeId == b.rhythmArchetypeId;
}

inline bool sameSettingsExceptOrdinal(
    const GroovePuterRhythm::PhraseExecutionMaterializationSettings& a,
    const GroovePuterRhythm::PhraseExecutionMaterializationSettings& b) {
  return a.level == b.level && a.feelProfile == b.feelProfile &&
         a.feelAmount == b.feelAmount &&
         a.tonalMaterializationEnabled == b.tonalMaterializationEnabled &&
         a.rootPitchClass == b.rootPitchClass &&
         a.scaleTypeValue == b.scaleTypeValue;
}

// R0: rebuild the kept phrase from the recipe (its original law) into `execution` and compare
// the canonical musical content of every lane of every bar with what the Song rows play now.
inline CycleStatus verifyKeptPhrase(
    MiniAcid& engine,
    const Scene& scene,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    GroovePuterRhythm::PreparedPhraseExecution& execution) {
  GroovePuterRhythm::PhraseExecutionScratch scratch{};
  if (GroovePuterRhythm::preparePhraseExecution(
          recipe.genre, recipe.materialization, recipe.phraseGenerationIdentity,
          recipe.bars, scratch, execution) !=
      GroovePuterRhythm::PhraseExecutionStatus::Ready) {
    return CycleStatus::EditedSinceGeneration;
  }
  const Song& song = scene.songs[recipe.songSlot];
  PhraseGenerator::PhraseBar rebuilt{};
  for (uint8_t bar = 0; bar < recipe.bars; ++bar) {
    const int localSlot = recipe.firstLocalSlot + bar;
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    const int globalPattern =
        songPatternFromPageBankIndex(recipe.pageIndex, bank, index);
    const SongPosition& row = song.positions[recipe.songStart + bar];
    if (row.patterns[static_cast<int>(SongTrack::SynthA)] != globalPattern ||
        row.patterns[static_cast<int>(SongTrack::SynthB)] != globalPattern ||
        row.patterns[static_cast<int>(SongTrack::Drums)] != globalPattern) {
      return CycleStatus::EditedSinceGeneration;
    }
    if (!GeneratedPhraseP1R::materializeOneBar(
            engine, execution, bar, static_cast<int16_t>(globalPattern), rebuilt)) {
      return CycleStatus::EditedSinceGeneration;
    }
    if (GeneratedPhraseP1R::canonicalBarHash(
            rebuilt.drums, rebuilt.synthA, rebuilt.synthB) !=
        GeneratedPhraseP1R::canonicalBarHash(
            scene.drumBanks[bank].patterns[index],
            scene.synthABanks[bank].patterns[index],
            scene.synthBBanks[bank].patterns[index])) {
      return CycleStatus::EditedSinceGeneration;
    }
  }
  return CycleStatus::CommittedNow;  // "verified"
}

inline CycleStatus statusForLaw(GroovePuterRhythm::PhraseLawApplyStatus status) {
  using S = GroovePuterRhythm::PhraseLawApplyStatus;
  switch (status) {
    case S::Applied: return CycleStatus::CommittedNow;
    case S::NotAdmitted:
    case S::NoEligibleTrajectory: return CycleStatus::NotAdmitted;
    case S::InvalidContext: return CycleStatus::Failed;
  }
  return CycleStatus::Failed;
}

template <typename Guard>
CycleResult generateCycle(MiniAcid& engine, Guard&& guard) {
  CycleResult output{};
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;
  const WriteLease lease = acquireWriteLease();
  if (lease.slot < 0) {
    output.status = CycleStatus::Busy;
    return output;
  }
  const auto refuse = [&](CycleStatus status) {
    releaseWriteSlot(lease.slot);
    output.status = status;
    return output;
  };

  const auto* recipePtr = engine.generatedPhraseRecipe();
  if (recipePtr == nullptr) return refuse(CycleStatus::NoRecipe);
  const GroovePuterMaterial::GeneratedPhraseRecipe recipe = *recipePtr;
  if (recipe.cycleSongStart >= 0) return refuse(CycleStatus::CycleAlreadyPublished);

  SceneManager& scenes = engine.sceneManager();
  const Scene& scene = scenes.currentScene();
  if (engine.currentPageIndex() != recipe.pageIndex ||
      std::clamp(scene.activeSongSlot, 0, 1) != recipe.songSlot) {
    return refuse(CycleStatus::TargetChanged);
  }
  if (engine.isPlaying() &&
      (!engine.songModeEnabled() ||
       engine.songPlaybackSlot() != recipe.songSlot)) {
    return refuse(CycleStatus::TargetChanged);
  }

  if (recipe.materialization.level !=
      GroovePuterRhythm::RealizationLevel::P3Transformation) {
    return refuse(CycleStatus::DepthNotP3);
  }

  // R1: everything the rebuild reads that the recipe does not store.
  if (!sameGenre(scene.genre, recipe.genre) ||
      GeneratedPhraseP1R::pitchSourceFingerprint(engine) != recipe.contextFingerprint ||
      !sameSettingsExceptOrdinal(
          GeneratedPhraseP1R::materializationSettingsFor(
              scene, GroovePuterState::currentGenerationLevel(),
              recipe.materialization.generationAttemptOrdinal),
          recipe.materialization)) {
    return refuse(CycleStatus::ContextChanged);
  }

  PreparedPhraseArrangement preparedStorage{};
  PreparedPhraseArrangement* const prepared = &preparedStorage;
  GroovePuterRhythm::PreparedPhraseExecution breakExecution{};

  // R0 uses the BREAK slot as scratch; it is rebuilt below.
  const CycleStatus verified = verifyKeptPhrase(engine, scene, recipe, breakExecution);
  if (verified != CycleStatus::CommittedNow) return refuse(verified);

  // The origin sidecar describes the LATEST generated phrase. It is evidence about A only while it
  // still describes A (same first slot). After a cycle it describes the cycle (more bars) and says
  // nothing against A; A's own contract (recipe + R0 above) is what decides.
  const auto* origin = engine.generatedSynthAOrigin();
  if (origin != nullptr && origin->common.barCount > 0) {
    const int keptFirstGlobal = songPatternFromPageBankIndex(
        recipe.pageIndex, recipe.firstLocalSlot / Bank<SynthPattern>::kPatterns,
        recipe.firstLocalSlot % Bank<SynthPattern>::kPatterns);
    const bool describesKept =
        origin->bars[0].material.address.globalSlot == static_cast<uint8_t>(keptFirstGlobal);
    if (describesKept &&
        (origin->common.phraseGenerationIdentity != recipe.phraseGenerationIdentity ||
         origin->common.barCount != recipe.bars)) {
      return refuse(CycleStatus::EditedSinceGeneration);
    }
  }

  // The kept phrase carries its OWN natural law (chosen from its identity, never assumed Loop).
  // `breakExecution` still holds the R0 rebuild of it: remember its programme, then reuse the slot.
  const auto naturalTrajectory = breakExecution.phraseTrajectory;

  // Both sections come from the same recipe; neither depends on the other. A section whose
  // programme equals the kept phrase's own would publish a copy of it, so it is skipped.
  GroovePuterRhythm::PhraseExecutionScratch scratch{};
  const auto build = [&](GroovePuterRhythm::PreparedPhraseExecution& execution,
                         GroovePuterRhythm::PhraseEvolutionLawId law) {
    if (GroovePuterRhythm::preparePhraseExecution(
            recipe.genre, recipe.materialization, recipe.phraseGenerationIdentity,
            kCycleSectionBars, scratch, execution) !=
        GroovePuterRhythm::PhraseExecutionStatus::Ready) {
      return CycleStatus::Failed;
    }
    return statusForLaw(GroovePuterRhythm::applyPhraseLawToExecution(execution, law));
  };
  using Law = GroovePuterRhythm::PhraseEvolutionLawId;
  CycleStatus built = build(prepared->p1rExecution, Law::DevelopReturn);
  if (built != CycleStatus::CommittedNow) return refuse(built);
  const bool developSkipped = prepared->p1rExecution.phraseTrajectory == naturalTrajectory;

  GroovePuterRhythm::PreparedPhraseExecution* const breakSlot =
      developSkipped ? &prepared->p1rExecution : &breakExecution;
  built = build(*breakSlot, Law::SparseDrift);
  if (built != CycleStatus::CommittedNow) return refuse(built);
  const bool breakSkipped = breakSlot->phraseTrajectory == naturalTrajectory;
  if (developSkipped && breakSkipped) return refuse(CycleStatus::NothingToAdd);
  const bool twoSections = !developSkipped && !breakSkipped;
  const uint8_t kCycleBars = twoSections ? 2 * kCycleSectionBars : kCycleSectionBars;
  output.developSkipped = developSkipped;
  output.breakSkipped = breakSkipped;
  output.bars = kCycleBars;

  const int cycleStart = recipe.songStart + recipe.bars;
  prepared->request.bars = kCycleBars;
  prepared->request.songStart = cycleStart;
  prepared->request.pageIndex = recipe.pageIndex;
  prepared->request.forceSingleBarRows = true;
  prepared->songSlot = recipe.songSlot;
  prepared->audibleSongRow = engine.currentSongPosition();
  prepared->baseRevision = GroovePuterState::sceneRevisionSnapshot().currentRevision;
  prepared->selectionTarget = captureTarget(scenes);
  prepared->genre = recipe.genre;
  prepared->useP1RRoute = true;
  prepared->result.bars = kCycleBars;
  prepared->result.songStart = cycleStart;

  if (cycleStart + kCycleBars > Song::kMaxPositions ||
      !PhraseGenerator::songRowsAreAvailable(
          scene.songs[recipe.songSlot], cycleStart, kCycleBars)) {
    return refuse(CycleStatus::RowsOccupied);
  }
  prepared->firstLocalSlot = SlotReuse::findRun(
      engine, scene, recipe.pageIndex, kCycleBars);
  if (prepared->firstLocalSlot < 0) return refuse(CycleStatus::NoSafeSlots);

  // PREFLIGHT: prove all eight bars materialize before any destination is touched.
  {
    PhraseGenerator::PhraseBar preflightScratch{};
    for (uint8_t bar = 0; bar < kCycleBars; ++bar) {
      const bool second = twoSections && bar >= kCycleSectionBars;
      const int localSlot = prepared->firstLocalSlot + bar;
      const int globalPattern = songPatternFromPageBankIndex(
          recipe.pageIndex,
          localSlot / Bank<SynthPattern>::kPatterns,
          localSlot % Bank<SynthPattern>::kPatterns);
      if (!GeneratedPhraseP1R::materializeOneBar(
              engine, second ? breakExecution : prepared->p1rExecution,
              static_cast<uint8_t>(second ? bar - kCycleSectionBars : bar),
              static_cast<int16_t>(globalPattern), preflightScratch)) {
        return refuse(CycleStatus::Failed);
      }
    }
  }

  if (!preparedTargetStillCommitSafe(engine, *prepared)) {
    return refuse(CycleStatus::TargetChanged);
  }

  const auto reservation = PatternPagingService::reserveMaterialIds(kCycleBars);
  if (!reservation.valid()) return refuse(CycleStatus::ReservationFailed);
  prepared->synthAReservation = reservation;

  const GeneratedPhraseUndoPayload before = captureUndo(scene, *prepared);
  auto&& applyGuard = guard;
  GroovePuterMaterial::GeneratedSynthAOriginCandidate originCandidate =
      beginOriginCandidate(*prepared);

  if (engine.isPlaying()) {
    if (!GroovePuterRhythm::PhraseLiveArrangementDetail::armPhraseActivation(
            engine, lease.slot, prepared->selectionTarget, prepared->songSlot,
            cycleStart, kCycleBars, prepared->audibleSongRow)) {
      return refuse(CycleStatus::TargetChanged);
    }
  }

  const bool committed = GroovePuterUndo::undoOwner().commitPrepared(
      GroovePuterUndo::UndoKind::Generation,
      before,
      [&]() {
        const auto apply = [&]() {
          applyPreparedPersistent(
              engine, engine.sceneManager().currentScene(), *prepared,
              &originCandidate, twoSections ? &breakExecution : nullptr,
              kCycleSectionBars);
        };
        applyGuard(apply);
      });
  if (!committed) {
    if (engine.isPlaying()) {
      GroovePuterRhythm::PhraseLiveArrangementDetail::abortPhraseActivation(
          lease.slot, GroovePuterRhythm::QuantizedGenerationStatus::Busy);
      output.status = CycleStatus::Busy;
      return output;
    }
    return refuse(CycleStatus::Busy);
  }

  if (!engine.publishGeneratedSynthAOrigin(originCandidate)) {
    engine.clearGeneratedSynthAOrigin();
  }
  engine.setGeneratedPhraseCycleStart(static_cast<int16_t>(cycleStart));

  if (!engine.isPlaying()) {
    releaseWriteSlot(lease.slot);
    engine.setSongMode(true);
    engine.setSongPlaybackSlot(prepared->songSlot);
    engine.setSongPosition(cycleStart);
    output.status = CycleStatus::CommittedNow;
    return output;
  }
  GroovePuterRhythm::PhraseLiveArrangementDetail::completePhraseActivation(
      lease.slot, GroovePuterUndo::undoOwner().committedRevision());
  output.status = CycleStatus::PendingNextBar;
  return output;
}

inline std::size_t preparedPhraseArrangementSize() {
  return sizeof(PreparedPhraseArrangement);
}

inline std::size_t generatedPhraseUndoPayloadSize() {
  return sizeof(GeneratedPhraseUndoPayload);
}

}  // namespace GeneratedPhraseSong
