#pragma once

#include <algorithm>
#include <cstdint>

#include "src/dsp/atlas_runtime.h"
#include "src/dsp/miniacid_engine.h"
#include "src/dsp/pattern_generator.h"
#include "src/dsp/song_pattern_materializer.h"
#include "src/pattern/pattern_address.h"

namespace SongPatternCandidate {

struct Candidate {
  SynthPattern synth{};
  DrumPatternSet drums{};
};

inline uint32_t legacyCompatibleSeed(MiniAcid& engine,
                                     int row,
                                     int pageIndex,
                                     int localSlot,
                                     SmartPatternGenerator::Mode genMode) {
  SongPatternMaterializer::Request request{};
  request.row = row;
  request.pageIndex = pageIndex;
  request.seed = engine.modeManager().generationSeed();
  const uint8_t genreTag = static_cast<uint8_t>(
      engine.genreManager().generativeMode());
  const uint8_t recipeTag = static_cast<uint8_t>(
      engine.genreManager().recipe());
  request.modeTag = static_cast<uint8_t>(
      genreTag * 17u + recipeTag * 5u + static_cast<uint8_t>(genMode));
  return SongPatternMaterializer::actionSeed(request, localSlot);
}

inline bool produce(MiniAcid& engine,
                    SongTrack track,
                    int row,
                    int pageIndex,
                    int localSlot,
                    SmartPatternGenerator::Mode genMode,
                    Candidate& candidate) {
  if (row < 0 || row >= Song::kMaxPositions ||
      pageIndex < 0 || pageIndex >= kMaxPages ||
      localSlot < 0 || localSlot >= kPatternsPerPage ||
      SongPatternMaterializer::editableTrackIndex(track) < 0) {
    return false;
  }

  auto& genreManager = engine.genreManager();
  const GenerativeMode activeGenre = genreManager.generativeMode();
  const GenreRecipeId activeRecipe = genreManager.recipe();
  const GenerativeParams& params = genreManager.getCompiledGenerativeParams();
  const GenreBehavior behavior = genreManager.getBehavior();
  const GrooveboxMode mappedMode = GenreManager::grooveboxModeForRecipe(
      activeRecipe, activeGenre);

  SynthPattern atlasA{};
  SynthPattern atlasB{};
  DrumPatternSet atlasDrums{};
  const uint8_t variationCount = AtlasRuntime::variationCount(activeRecipe);
  const uint8_t variation = variationCount == 0
      ? 0
      : static_cast<uint8_t>(std::min(
            static_cast<int>(variationCount) - 1,
            static_cast<int>(genMode)));
  const bool atlasReady = AtlasRuntime::hasRecipe(activeRecipe) &&
      AtlasRuntime::applyRecipe(activeRecipe, variation,
                                atlasA, atlasB, atlasDrums, nullptr);

  candidate = Candidate{};
  if (atlasReady) {
    switch (track) {
      case SongTrack::SynthA:
        candidate.synth = atlasA;
        return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(
            candidate.synth);
      case SongTrack::SynthB:
        candidate.synth = atlasB;
        return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(
            candidate.synth);
      case SongTrack::Drums:
        candidate.drums = atlasDrums;
        return !SongPatternMaterializer::drumPatternSetIsStrictlyEmpty(
            candidate.drums);
      case SongTrack::Voice:
        return false;
    }
  }

  GrooveboxModeManager generator(engine);
  generator.setModeLocal(mappedMode);
  generator.setFlavorLocal(0);
  generator.setGenerationSeed(
      legacyCompatibleSeed(engine, row, pageIndex, localSlot, genMode));

  switch (track) {
    case SongTrack::SynthA:
      generator.generatePattern(
          candidate.synth, engine.bpm(), params, behavior, 0);
      return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(
          candidate.synth);
    case SongTrack::SynthB:
      generator.generatePattern(
          candidate.synth, engine.bpm(), params, behavior, 1);
      return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(
          candidate.synth);
    case SongTrack::Drums:
      generator.generateDrumPattern(candidate.drums, params, behavior);
      return !SongPatternMaterializer::drumPatternSetIsStrictlyEmpty(
          candidate.drums);
    case SongTrack::Voice:
      return false;
  }
  return false;
}

inline bool writeToLeasedAddress(Scene& scene,
                                 SongTrack track,
                                 int globalPattern,
                                 const Candidate& candidate) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  if (!address.valid() || SongPatternMaterializer::editableTrackIndex(track) < 0) {
    return false;
  }

  switch (track) {
    case SongTrack::SynthA:
      scene.synthABanks[address.bank].patterns[address.slot] = candidate.synth;
      return true;
    case SongTrack::SynthB:
      scene.synthBBanks[address.bank].patterns[address.slot] = candidate.synth;
      return true;
    case SongTrack::Drums:
      scene.drumBanks[address.bank].patterns[address.slot] = candidate.drums;
      return true;
    case SongTrack::Voice:
      return false;
  }
  return false;
}

}  // namespace SongPatternCandidate
