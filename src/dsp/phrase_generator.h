#pragma once

#include "../../scenes.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace PhraseGenerator {

enum class PhraseBarRole : uint8_t {
  Base = 0,
  MicroVariation,
  Return,
  Development,
  Breakdown,
  Build,
  Fill,
  EndingFill,
};

enum class PhraseError : uint8_t {
  None = 0,
  UnsupportedLength,
  InvalidPage,
  SongOutOfRange,
  SongRowsOccupied,
  NoContiguousPatternSlots,
  GenerationFailed,
};

struct PhraseRequest {
  uint8_t bars = 4;
  int songStart = 0;
  int pageIndex = 0;
  uint32_t seed = 0x47525048u;
  bool forceSingleBarRows = true;
};

struct PhraseResult {
  PhraseError error = PhraseError::None;
  int bars = 0;
  int songStart = -1;
  int firstLocalSlot = -1;
  int firstGlobalPattern = -1;

  explicit operator bool() const { return error == PhraseError::None; }
};

struct PhraseBar {
  SynthPattern synthA{};
  SynthPattern synthB{};
  DrumPatternSet drums{};
};

inline const char* errorText(PhraseError error) {
  switch (error) {
    case PhraseError::None: return "OK";
    case PhraseError::UnsupportedLength: return "Use 1/2/4/8 bars";
    case PhraseError::InvalidPage: return "Pattern page unavailable";
    case PhraseError::SongOutOfRange: return "Not enough Song rows";
    case PhraseError::SongRowsOccupied: return "Song rows are not empty";
    case PhraseError::NoContiguousPatternSlots: return "Need consecutive empty slots";
    case PhraseError::GenerationFailed: return "Phrase generation failed";
  }
  return "Phrase error";
}

inline bool isSupportedLength(int bars) {
  return bars == 1 || bars == 2 || bars == 4 || bars == 8;
}

inline PhraseBarRole roleForBar(int bars, int barIndex) {
  if (bars <= 1 || barIndex <= 0) return PhraseBarRole::Base;
  if (bars == 2) return PhraseBarRole::MicroVariation;
  if (bars == 4) {
    static constexpr PhraseBarRole kRoles[4] = {
        PhraseBarRole::Base,
        PhraseBarRole::MicroVariation,
        PhraseBarRole::Return,
        PhraseBarRole::Fill,
    };
    return kRoles[std::clamp(barIndex, 0, 3)];
  }

  static constexpr PhraseBarRole kRoles[8] = {
      PhraseBarRole::Base,
      PhraseBarRole::MicroVariation,
      PhraseBarRole::Return,
      PhraseBarRole::Fill,
      PhraseBarRole::Development,
      PhraseBarRole::Breakdown,
      PhraseBarRole::Build,
      PhraseBarRole::EndingFill,
  };
  return kRoles[std::clamp(barIndex, 0, 7)];
}

inline bool synthPatternIsEmpty(const SynthPattern& pattern) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& value = pattern.steps[step];
    if (value.note != -1 || value.slide || value.accent || value.ghost ||
        value.velocity != 100 || value.timing != 0 || value.fx != 0 ||
        value.fxParam != 0 || value.probability != 100) {
      return false;
    }
  }
  return true;
}

inline bool drumPatternSetIsEmpty(const DrumPatternSet& pattern) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& value = pattern.voices[voice].steps[step];
      if (value.hit || value.accent || value.velocity != 100 ||
          value.timing != 0 || value.fx != 0 || value.fxParam != 0 ||
          value.probability != 100) {
        return false;
      }
    }
  }
  for (int lane = 0; lane < DrumPatternSet::kMaxLanes; ++lane) {
    if (pattern.lanes[lane].nodeCount != 0 ||
        pattern.lanes[lane].targetParam != DRUM_AUTOMATION_NONE) {
      return false;
    }
  }
  return pattern.groove.swing < 0.0f && pattern.groove.humanize < 0.0f;
}

inline bool localSlotIsEmpty(const Scene& scene, int localSlot) {
  if (localSlot < 0 || localSlot >= kPatternsPerPage) return false;
  const int bank = localSlot / Bank<SynthPattern>::kPatterns;
  const int index = localSlot % Bank<SynthPattern>::kPatterns;
  return synthPatternIsEmpty(scene.synthABanks[bank].patterns[index]) &&
         synthPatternIsEmpty(scene.synthBBanks[bank].patterns[index]) &&
         drumPatternSetIsEmpty(scene.drumBanks[bank].patterns[index]);
}

inline int findContiguousEmptySlots(const Scene& scene, int bars) {
  if (!isSupportedLength(bars) || bars > kPatternsPerPage) return -1;
  const int lastStart = kPatternsPerPage - bars;
  for (int start = 0; start <= lastStart; ++start) {
    bool allEmpty = true;
    for (int offset = 0; offset < bars; ++offset) {
      if (!localSlotIsEmpty(scene, start + offset)) {
        allEmpty = false;
        break;
      }
    }
    if (allEmpty) return start;
  }
  return -1;
}

inline bool globalPatternIsReferenced(const Scene& scene, int globalPattern) {
  if (globalPattern < 0) return true;
  for (int songSlot = 0; songSlot < 2; ++songSlot) {
    const Song& song = scene.songs[songSlot];
    for (int row = 0; row < Song::kMaxPositions; ++row) {
      for (int track = 0; track < 3; ++track) {
        if (song.positions[row].patterns[track] == globalPattern) return true;
      }
    }
  }
  return false;
}

inline bool localSlotIsSafeForPhrase(const Scene& scene,
                                     int pageIndex,
                                     int localSlot) {
  if (pageIndex < 0 || pageIndex >= kMaxPages ||
      !localSlotIsEmpty(scene, localSlot)) {
    return false;
  }
  const int bank = localSlot / Bank<SynthPattern>::kPatterns;
  const int index = localSlot % Bank<SynthPattern>::kPatterns;
  const int globalPattern = songPatternFromPageBankIndex(
      pageIndex, bank, index);
  return !globalPatternIsReferenced(scene, globalPattern);
}

inline int findSafeContiguousEmptySlots(const Scene& scene,
                                        int pageIndex,
                                        int bars) {
  if (pageIndex < 0 || pageIndex >= kMaxPages ||
      !isSupportedLength(bars) || bars > kPatternsPerPage) {
    return -1;
  }
  const int lastStart = kPatternsPerPage - bars;
  for (int start = 0; start <= lastStart; ++start) {
    bool allSafe = true;
    for (int offset = 0; offset < bars; ++offset) {
      if (!localSlotIsSafeForPhrase(scene, pageIndex, start + offset)) {
        allSafe = false;
        break;
      }
    }
    if (allSafe) return start;
  }
  return -1;
}

inline bool songRowsAreAvailable(const Song& song, int start, int bars) {
  if (start < 0 || bars < 1 || start + bars > Song::kMaxPositions) return false;
  for (int row = start; row < start + bars; ++row) {
    const SongPosition& position = song.positions[row];
    if (position.patterns[static_cast<int>(SongTrack::SynthA)] >= 0 ||
        position.patterns[static_cast<int>(SongTrack::SynthB)] >= 0 ||
        position.patterns[static_cast<int>(SongTrack::Drums)] >= 0) {
      return false;
    }
  }
  return true;
}

inline uint32_t nextRandom(uint32_t& state) {
  if (state == 0) state = 0x9E3779B9u;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

inline int findOccupiedSynthStep(const SynthPattern& pattern, uint32_t randomValue) {
  const int start = static_cast<int>(randomValue % SynthPattern::kSteps);
  for (int offset = 0; offset < SynthPattern::kSteps; ++offset) {
    const int step = (start + offset) % SynthPattern::kSteps;
    if (pattern.steps[step].note >= 0) return step;
  }
  return -1;
}

inline void varySynthAccent(SynthPattern& pattern, uint32_t& randomState) {
  const int step = findOccupiedSynthStep(pattern, nextRandom(randomState));
  if (step < 0) return;
  pattern.steps[step].accent = !pattern.steps[step].accent;
}

inline void developSynth(SynthPattern& pattern, uint32_t& randomState) {
  const int step = findOccupiedSynthStep(pattern, nextRandom(randomState));
  if (step < 0) return;
  SynthStep& value = pattern.steps[step];
  const int note = static_cast<int>(value.note);
  value.note = static_cast<int8_t>(note <= 115 ? note + 12 : note - 12);
  value.accent = true;
}

inline void thinSynth(SynthPattern& pattern) {
  int seen = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note < 0) continue;
    ++seen;
    if ((seen % 2) == 0) pattern.steps[step] = SynthStep{};
  }
}

inline void accentLastQuarter(SynthPattern& pattern) {
  for (int step = SynthPattern::kSteps - 1; step >= 12; --step) {
    if (pattern.steps[step].note < 0) continue;
    pattern.steps[step].accent = true;
    pattern.steps[step].slide = true;
    return;
  }
}

inline void placeDrumHit(DrumPatternSet& drums,
                         int voice,
                         int step,
                         uint8_t velocity,
                         bool accent) {
  if (voice < 0 || voice >= DrumPatternSet::kVoices ||
      step < 0 || step >= DrumPattern::kSteps) {
    return;
  }
  DrumStep& value = drums.voices[voice].steps[step];
  value.hit = 1;
  value.accent = accent ? 1 : 0;
  value.velocity = velocity;
  value.probability = 100;
}

inline void varyDrums(DrumPatternSet& drums, uint32_t& randomState) {
  const int step = 1 + static_cast<int>((nextRandom(randomState) % 8u) * 2u);
  placeDrumHit(drums, 2, step, 62, false);
}

inline void thinDrums(DrumPatternSet& drums) {
  for (int voice = 2; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      drums.voices[voice].steps[step] = DrumStep{};
    }
  }
}

inline void buildDrums(DrumPatternSet& drums) {
  for (int step = 9; step < DrumPattern::kSteps; step += 2) {
    placeDrumHit(drums, 2, step,
                 static_cast<uint8_t>(68 + (step - 9) * 4), false);
  }
}

inline void fillDrums(DrumPatternSet& drums, bool ending) {
  static constexpr int kVoices[4] = {1, 4, 5, 1};
  for (int i = 0; i < 4; ++i) {
    const uint8_t velocity =
        static_cast<uint8_t>((ending ? 82 : 70) + i * 10);
    placeDrumHit(drums, kVoices[i], 12 + i, velocity, ending || i == 3);
  }
}

inline void deriveBar(const PhraseBar& base,
                      PhraseBarRole role,
                      uint32_t seed,
                      int barIndex,
                      PhraseBar& output) {
  output = base;
  uint32_t randomState =
      seed ^ (0x9E3779B9u * static_cast<uint32_t>(barIndex + 1));

  switch (role) {
    case PhraseBarRole::Base:
    case PhraseBarRole::Return:
      return;
    case PhraseBarRole::MicroVariation:
      varySynthAccent(output.synthA, randomState);
      varySynthAccent(output.synthB, randomState);
      varyDrums(output.drums, randomState);
      return;
    case PhraseBarRole::Development:
      developSynth(output.synthA, randomState);
      developSynth(output.synthB, randomState);
      varyDrums(output.drums, randomState);
      return;
    case PhraseBarRole::Breakdown:
      thinSynth(output.synthA);
      thinSynth(output.synthB);
      thinDrums(output.drums);
      return;
    case PhraseBarRole::Build:
      varySynthAccent(output.synthA, randomState);
      varySynthAccent(output.synthB, randomState);
      buildDrums(output.drums);
      return;
    case PhraseBarRole::Fill:
      accentLastQuarter(output.synthA);
      accentLastQuarter(output.synthB);
      fillDrums(output.drums, false);
      return;
    case PhraseBarRole::EndingFill:
      accentLastQuarter(output.synthA);
      accentLastQuarter(output.synthB);
      fillDrums(output.drums, true);
      return;
  }
}

inline void clearCommittedBar(Scene& scene,
                              Song& song,
                              int songRow,
                              int localSlot) {
  const int bank = localSlot / Bank<SynthPattern>::kPatterns;
  const int index = localSlot % Bank<SynthPattern>::kPatterns;
  scene.synthABanks[bank].patterns[index] = SynthPattern{};
  scene.synthBBanks[bank].patterns[index] = SynthPattern{};
  scene.drumBanks[bank].patterns[index] = DrumPatternSet{};

  SongPosition& position = song.positions[songRow];
  position.patterns[static_cast<int>(SongTrack::SynthA)] = -1;
  position.patterns[static_cast<int>(SongTrack::SynthB)] = -1;
  position.patterns[static_cast<int>(SongTrack::Drums)] = -1;
}

template <typename BarGenerator>
PhraseResult generateBarsToSong(Scene& scene,
                                const PhraseRequest& request,
                                BarGenerator&& generateBar) {
  PhraseResult result{};
  result.bars = request.bars;
  result.songStart = request.songStart;

  if (!isSupportedLength(request.bars)) {
    result.error = PhraseError::UnsupportedLength;
    return result;
  }
  if (request.pageIndex < 0 || request.pageIndex >= kMaxPages) {
    result.error = PhraseError::InvalidPage;
    return result;
  }
  if (request.songStart < 0 ||
      request.songStart + request.bars > Song::kMaxPositions) {
    result.error = PhraseError::SongOutOfRange;
    return result;
  }

  const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
  Song& song = scene.songs[songSlot];
  if (!songRowsAreAvailable(song, request.songStart, request.bars)) {
    result.error = PhraseError::SongRowsOccupied;
    return result;
  }

  const int firstLocalSlot = findSafeContiguousEmptySlots(
      scene, request.pageIndex, request.bars);
  if (firstLocalSlot < 0) {
    result.error = PhraseError::NoContiguousPatternSlots;
    return result;
  }

  int committedBars = 0;
  auto&& generator = generateBar;
  for (int bar = 0; bar < request.bars; ++bar) {
    PhraseBar generated{};
    if (!generator(generated, roleForBar(request.bars, bar), bar)) {
      for (int rollback = 0; rollback < committedBars; ++rollback) {
        clearCommittedBar(scene, song, request.songStart + rollback,
                          firstLocalSlot + rollback);
      }
      result.error = PhraseError::GenerationFailed;
      return result;
    }

    const int localSlot = firstLocalSlot + bar;
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    scene.synthABanks[bank].patterns[index] = generated.synthA;
    scene.synthBBanks[bank].patterns[index] = generated.synthB;
    scene.drumBanks[bank].patterns[index] = generated.drums;

    const int globalPattern = songPatternFromPageBankIndex(
        request.pageIndex, bank, index);
    SongPosition& position = song.positions[request.songStart + bar];
    position.patterns[static_cast<int>(SongTrack::SynthA)] =
        static_cast<int16_t>(globalPattern);
    position.patterns[static_cast<int>(SongTrack::SynthB)] =
        static_cast<int16_t>(globalPattern);
    position.patterns[static_cast<int>(SongTrack::Drums)] =
        static_cast<int16_t>(globalPattern);
    ++committedBars;
  }

  song.length = std::max(song.length, request.songStart + request.bars);
  if (request.forceSingleBarRows) scene.feel.patternBars = 1;

  result.error = PhraseError::None;
  result.firstLocalSlot = firstLocalSlot;
  result.firstGlobalPattern = songPatternFromPageBankIndex(
      request.pageIndex,
      firstLocalSlot / Bank<SynthPattern>::kPatterns,
      firstLocalSlot % Bank<SynthPattern>::kPatterns);
  return result;
}

template <typename BaseGenerator>
PhraseResult generateToSong(Scene& scene,
                            const PhraseRequest& request,
                            BaseGenerator&& generateBase) {
  PhraseBar base{};
  std::forward<BaseGenerator>(generateBase)(base);
  return generateBarsToSong(
      scene, request,
      [&](PhraseBar& generated, PhraseBarRole role, int barIndex) {
        deriveBar(base, role, request.seed, barIndex, generated);
        return true;
      });
}

}  // namespace PhraseGenerator