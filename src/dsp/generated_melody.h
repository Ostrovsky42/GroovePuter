#pragma once

#include <cstdint>
#include <memory>
#include <new>

#include "generated_phrase_song.h"
#include "src/state/generation_shape_state.h"
#include "src/phrase/runtime_phrase_edit.h"
#include "src/phrase/runtime_synth_events.h"

// G straight into a Melody (0.9.18): a 1/2/4/8-bar phrase for one synth, built
// bar by bar the way MATERIAL builds a TAKE (same generator, the bar ordinal in
// the rhythm migration, PhraseGenerator bar roles), so the bars differ and the
// harmony moves. Only that synth's part is kept; nothing is written to steps,
// slots or Song rows. Uses the legacy TAKE route (materializeLegacyBar); the
// P1R route needs slot/row reservations a Melody does not have. The salt also
// moves the rhythm-migration coordinate, so another press changes the rhythm
// cell and contour, not only the legacy seed.
namespace GeneratedMelody {

enum class Status : uint8_t {
  Ready,
  InvalidRequest,
  OutOfMemory,
  BarFailed,
  ProjectionFailed,
  TooManyNotes,
};

inline const char* statusText(Status status) {
  switch (status) {
    case Status::Ready: return "READY";
    case Status::InvalidRequest: return "CANNOT GENERATE HERE";
    case Status::OutOfMemory: return "NO MEMORY FOR MELODY";
    case Status::BarFailed: return "GENERATION FAILED";
    case Status::ProjectionFailed: return "GENERATION FAILED";
    case Status::TooManyNotes: return "TOO MANY NOTES";
  }
  return "GENERATION FAILED";
}

namespace detail {

inline uint8_t distinctPitches(const PhraseRuntime::RuntimeSynthEventBuffer& b) {
  uint32_t seen[4] = {0, 0, 0, 0};
  uint8_t count = 0;
  for (uint16_t i = 0; i < b.count; ++i) {
    const uint8_t note = static_cast<uint8_t>(b.events[i].note & 0x7F);
    const uint32_t bit = 1u << (note & 31u);
    if ((seen[note >> 5] & bit) == 0) {
      seen[note >> 5] |= bit;
      ++count;
    }
  }
  return count;
}

// One attempt at a given rhythm-migration coordinate.
inline Status generateAt(MiniAcid& engine, int voice, uint8_t bars,
                         uint32_t salt, int coordinate,
                         PhraseRuntime::RuntimeSynthEventBuffer& out) {
  const uint16_t barTicks = PhraseRuntime::kTicksPerBar;
  const uint16_t lengthTicks = RuntimePhraseEdit::lengthTicksForBars(bars);
  if (voice < 0 || voice > 1 || lengthTicks == 0 ||
      !RuntimePhraseEdit::validLengthTicks(lengthTicks)) {
    return Status::InvalidRequest;
  }
  const int page = engine.currentPageIndex();
  if (page < 0 || page >= kMaxPages) return Status::InvalidRequest;

  // Transient UI-thread scratch (about 4 KB), released on return.
  std::unique_ptr<GeneratedPhraseSong::PreparedPhraseArrangement> prepared(
      new (std::nothrow) GeneratedPhraseSong::PreparedPhraseArrangement());
  std::unique_ptr<PhraseGenerator::PhraseBar> bar(
      new (std::nothrow) PhraseGenerator::PhraseBar());
  std::unique_ptr<PhraseRuntime::RuntimeSynthEventBuffer> one(
      new (std::nothrow) PhraseRuntime::RuntimeSynthEventBuffer());
  if (!prepared || !bar || !one) return Status::OutOfMemory;

  const Scene& scene = engine.sceneManager().currentScene();
  auto& genreManager = engine.genreManager();
  prepared->request.bars = bars;
  prepared->request.pageIndex = page;
  prepared->request.seed =
      GeneratedPhraseSong::phraseSeed(engine, page, 0, bars) ^
      (salt * 0x2545F491u + static_cast<uint32_t>(voice + 1) * 0x9E3779B1u);
  if (prepared->request.seed == 0) prepared->request.seed = 0x4D454C59u;
  prepared->genre = scene.genre;
  prepared->legacyRecipe = genreManager.recipe();
  prepared->legacyParams = genreManager.getCompiledGenerativeParams();
  prepared->legacyBehavior = genreManager.getBehavior();
  prepared->legacyMappedMode = GenreManager::grooveboxModeForRecipe(
      prepared->legacyRecipe, genreManager.generativeMode());
  prepared->legacyFlavor = engine.modeManager().flavor();
  prepared->legacyBpm = engine.bpm();
  prepared->legacyAtlas = AtlasRuntime::hasRecipe(prepared->legacyRecipe) &&
      AtlasRuntime::variationCount(prepared->legacyRecipe) >= 3;

  // The projection Alt+R uses, so a generated bar sounds like the same bar in
  // steps would.
  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = static_cast<uint8_t>(voice);
  settings.gateLengthRatio = genreManager.getGrooveRecipe().gateLengthRatio;
  int swing = scene.feel.swingPct;
  if (swing < 50) swing = 50;
  if (swing > 75) swing = 75;
  settings.swingPercent = static_cast<uint8_t>(swing);
  const VoiceId voiceId = voice == 0 ? VoiceId::SynthA : VoiceId::SynthB;
  settings.swingEnabled =
      (scene.feel.swingMask & (1u << static_cast<int>(voiceId))) != 0;

  out = PhraseRuntime::RuntimeSynthEventBuffer{};
  out.lengthTicks = lengthTicks;
  for (int index = 0; index < bars; ++index) {
    if (!GeneratedPhraseSong::materializeLegacyBar(engine, scene, *prepared,
                                                   index, *bar, coordinate)) {
      return Status::BarFailed;
    }
    const SynthPattern& part = voice == 0 ? bar->synthA : bar->synthB;
    if (PhraseRuntime::projectPatternToRuntimeEvents(part, settings, *one) !=
        PhraseRuntime::PatternProjectionStatus::Ready) {
      return Status::ProjectionFailed;
    }
    const uint32_t barStart = static_cast<uint32_t>(index) * barTicks;
    for (uint16_t i = 0; i < one->count; ++i) {
      PhraseRuntime::RuntimeSynthEvent event = one->events[i];
      if (event.startTick >= barTicks) continue;
      // A note may not run into the next bar: there it would overlap that
      // bar's first note and play as a chord.
      const uint32_t maxSubticks =
          static_cast<uint32_t>(barTicks - event.startTick) *
          PhraseRuntime::kSubticksPerTick;
      if (event.durationSubticks > maxSubticks) {
        event.durationSubticks = static_cast<uint16_t>(maxSubticks);
      }
      if (event.durationSubticks == 0) continue;
      if (out.count >= PhraseRuntime::kMaxSynthEvents) return Status::TooManyNotes;
      event.startTick = static_cast<uint16_t>(barStart + event.startTick);
      out.events[out.count++] = event;
    }
  }
  return RuntimePhraseEdit::validate(out) ? Status::Ready
                                          : Status::ProjectionFailed;
}

// GEN panel NOTES: hold notes towards the next attack. Never shortens a note
// and never reaches the next attack, so nothing overlaps.
inline void applyNoteLength(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                            GroovePuterState::GenerationNoteLength length,
                            uint32_t salt) {
  using GroovePuterState::GenerationNoteLength;
  if (length == GenerationNoteLength::Short) return;
  constexpr uint32_t kRestTicks = PhraseRuntime::kTicksPerBar / 8;  // two steps
  for (uint16_t i = 0; i < phrase.count; ++i) {
    auto& event = phrase.events[i];
    uint32_t next = phrase.lengthTicks;
    for (uint16_t j = 0; j < phrase.count; ++j) {
      const uint32_t start = phrase.events[j].startTick;
      if (start > event.startTick && start < next) next = start;
    }
    uint32_t span = next - event.startTick;
    if (span > PhraseRuntime::kTicksPerBar) span = PhraseRuntime::kTicksPerBar;
    if (length == GenerationNoteLength::Mixed) {
      // Before a rest, two notes in three ring on; elsewhere one in three.
      const uint32_t hash = (event.startTick * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu);
      const bool beforeRest = next - event.startTick >= kRestTicks;
      if ((hash >> 16) % 3u >= (beforeRest ? 2u : 1u)) continue;
    }
    const uint32_t subticks = span * PhraseRuntime::kSubticksPerTick;
    if (subticks > event.durationSubticks && subticks <= 0xFFFFu) {
      event.durationSubticks = static_cast<uint16_t>(subticks);
    }
  }
}

}  // namespace detail

// `salt` separates successive presses: the same salt and settings give the
// same melody. Rest bars are allowed inside a phrase, so an attempt can come
// out (nearly) silent or on one pitch; a few coordinates are tried and the
// first usable phrase is kept, otherwise the richest one.
inline Status generate(MiniAcid& engine, int voice, uint8_t bars, uint32_t salt,
                       PhraseRuntime::RuntimeSynthEventBuffer& out) {
  constexpr int kAttempts = 6;
  std::unique_ptr<PhraseRuntime::RuntimeSynthEventBuffer> best;
  uint32_t bestScore = 0;
  Status status = Status::BarFailed;
  for (int attempt = 0; attempt < kAttempts; ++attempt) {
    // Any value in the pattern-address range the migration context accepts.
    const int coordinate = static_cast<int>(
        (salt * 37u + static_cast<uint32_t>(attempt) * 101u) %
        kMaxGlobalPatterns);
    status = detail::generateAt(engine, voice, bars, salt, coordinate, out);
    if (status != Status::Ready) return status;
    const uint8_t pitches = detail::distinctPitches(out);
    if (pitches >= 2 && out.count >= bars) {
      detail::applyNoteLength(out, GroovePuterState::generationNoteLength(), salt);
      return Status::Ready;
    }
    const uint32_t score = (static_cast<uint32_t>(pitches) << 16) | out.count;
    if (!best || score > bestScore) {
      if (!best) {
        best.reset(new (std::nothrow) PhraseRuntime::RuntimeSynthEventBuffer());
        if (!best) {  // keep this attempt
          detail::applyNoteLength(out, GroovePuterState::generationNoteLength(), salt);
          return Status::Ready;
        }
      }
      *best = out;
      bestScore = score;
    }
  }
  out = *best;
  detail::applyNoteLength(out, GroovePuterState::generationNoteLength(), salt);
  return Status::Ready;
}

}  // namespace GeneratedMelody
