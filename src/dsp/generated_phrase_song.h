#pragma once

#include "../../scenes.h"
#include "atlas_runtime.h"
#include "mode_manager.h"
#include "phrase_generator.h"
#include "src/generation/migration/quantized_generation_commit.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/state/generation_request_state.h"
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <new>
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
      return PhraseGenerator::errorText(result.phrase.error);
  }
  return "PHRASE ERROR";
}

struct PreparedPhraseArrangement {
  PhraseGenerator::PhraseRequest request{};
  PhraseGenerator::PhraseResult result{};
  GroovePuterRhythm::QuantizedGenerationDetail::PatternTarget selectionTarget{};
  uint32_t baseRevision = 0;
  int songSlot = -1;
  int audibleSongRow = -1;
  int firstLocalSlot = -1;
  std::array<PhraseGenerator::PhraseBar, kMaxPreparedBars> material{};
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
    int variationCoordinate) {
  GroovePuterRhythm::StrongRhythmMigrationContext context{};
  context.patternAddress = static_cast<int16_t>(variationCoordinate);
  context.level = GroovePuterState::currentGenerationLevel();
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
    PhraseGenerator::PhraseBar& bar) {
  const auto context = migrationContextFor(scene, variationCoordinate);
  (void)GroovePuterRhythm::migrateStrongRhythmMaterial(
      genre, context, bar.drums, bar.synthA, bar.synthB);
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

inline void applyPreparedPersistent(
    Scene& scene,
    const PreparedPhraseArrangement& prepared) {
  Song& song = scene.songs[prepared.songSlot];
  for (int bar = 0; bar < prepared.request.bars; ++bar) {
    const int localSlot = prepared.firstLocalSlot + bar;
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    const PhraseGenerator::PhraseBar& material = prepared.material[bar];

    scene.synthABanks[bank].patterns[index] = material.synthA;
    scene.synthBBanks[bank].patterns[index] = material.synthB;
    scene.drumBanks[bank].patterns[index] = material.drums;

    const int globalPattern = songPatternFromPageBankIndex(
        prepared.request.pageIndex, bank, index);
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
    // D2 guarantees boundary-safe Undo-before-activation. Once the generated
    // material is already audible, retain the receipt rather than mutating the
    // playing row mid-bar; STOP makes the same receipt safely restorable.
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

inline bool prepare(
    MiniAcid& engine,
    uint8_t bars,
    int songStart,
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
  auto& genreManager = engine.genreManager();
  const GenreRecipeId recipe = genreManager.recipe();
  const GenerativeMode activeGenre = genreManager.generativeMode();
  const GenerativeParams params = genreManager.getCompiledGenerativeParams();
  const GenreBehavior behavior = genreManager.getBehavior();
  const GrooveboxMode mappedMode = GenreManager::grooveboxModeForRecipe(
      recipe, activeGenre);
  const bool atlasPhrase = AtlasRuntime::hasRecipe(recipe) &&
      AtlasRuntime::variationCount(recipe) >= 3;

  PhraseGenerator::PhraseBar proceduralBase{};
  bool proceduralBaseReady = false;
  GrooveboxModeManager scratchMode(engine);
  scratchMode.setModeLocal(mappedMode);
  scratchMode.setFlavorLocal(engine.modeManager().flavor());
  scratchMode.setGenerationSeed(prepared.request.seed);

  for (int barIndex = 0; barIndex < bars; ++barIndex) {
    const auto role = PhraseGenerator::roleForBar(bars, barIndex);
    PhraseGenerator::PhraseBar& bar = prepared.material[barIndex];
    if (atlasPhrase) {
      const uint8_t variation = atlasVariationForRole(role);
      if (!AtlasRuntime::applyRecipe(
              recipe, variation,
              bar.synthA, bar.synthB, bar.drums, nullptr)) {
        prepared.result.error = PhraseGenerator::PhraseError::GenerationFailed;
        return false;
      }
      applyCurrentMigration(scene, genre, variation, bar);
      continue;
    }

    if (!proceduralBaseReady) {
      scratchMode.generatePattern(
          proceduralBase.synthA, engine.bpm(), params, behavior, 0);
      scratchMode.generatePattern(
          proceduralBase.synthB, engine.bpm(), params, behavior, 1);
      scratchMode.generateDrumPattern(
          proceduralBase.drums, params, behavior);
      applyCurrentMigration(scene, genre, 0, proceduralBase);
      proceduralBaseReady = true;
    }
    PhraseGenerator::deriveBar(
        proceduralBase, role, prepared.request.seed, barIndex, bar);
  }

  prepared.result.error = PhraseGenerator::PhraseError::None;
  prepared.result.firstLocalSlot = prepared.firstLocalSlot;
  prepared.result.firstGlobalPattern = songPatternFromPageBankIndex(
      prepared.request.pageIndex,
      prepared.firstLocalSlot / Bank<SynthPattern>::kPatterns,
      prepared.firstLocalSlot % Bank<SynthPattern>::kPatterns);
  return true;
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

  std::unique_ptr<PreparedPhraseArrangement> prepared(
      new (std::nothrow) PreparedPhraseArrangement{});
  if (!prepared) {
    releaseWriteSlot(lease.slot);
    output.status = LifecycleStatus::OutOfMemory;
    return output;
  }

  if (engine.isPlaying() &&
      (!engine.songModeEnabled() ||
       engine.songPlaybackSlot() !=
           std::clamp(engine.sceneManager().activeSongSlot(), 0, 1))) {
    releaseWriteSlot(lease.slot);
    output.status = LifecycleStatus::TargetChanged;
    return output;
  }

  if (!prepare(engine, bars, songStart, *prepared)) {
    releaseWriteSlot(lease.slot);
    output.phrase = prepared->result;
    output.status = LifecycleStatus::Failed;
    return output;
  }
  output.phrase = prepared->result;

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
              engine.sceneManager().currentScene(), *prepared);
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
