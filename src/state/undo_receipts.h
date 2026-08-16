#pragma once

#include <cstdint>
#include <type_traits>

#include "../../scenes.h"

namespace GroovePuterUndo {

// Stable address + complete before-image for one persisted Synth Pattern.
// Page identity is required because only the active Pattern page is resident in
// Scene at a time; Undo never performs filesystem/page loading on demand.
struct SynthPatternUndoPayload {
  uint8_t pageIndex{0};
  uint8_t synthIndex{0};
  uint8_t bankIndex{0};
  uint8_t patternIndex{0};
  SynthPattern before{};
};

static_assert(sizeof(SynthPatternUndoPayload) == 116,
              "R2 Synth Pattern Undo receipt size changed");
static_assert(std::is_trivially_copyable<SynthPatternUndoPayload>::value,
              "Synth Pattern Undo receipt must remain a fixed value");

inline bool validSynthPatternAddress(const SynthPatternUndoPayload& receipt) {
  return receipt.pageIndex < kMaxPages &&
         receipt.synthIndex < 2 &&
         receipt.bankIndex < kBankCount &&
         receipt.patternIndex < Bank<SynthPattern>::kPatterns;
}

inline bool captureCurrentSynthPatternUndo(SceneManager& manager,
                                           int synthIndex,
                                           SynthPatternUndoPayload& receipt) {
  if (synthIndex < 0 || synthIndex >= 2) return false;
  const int page = manager.currentPageIndex();
  const int bank = manager.getCurrentBankIndex(synthIndex + 1);
  const int pattern = manager.getCurrentSynthPatternIndex(synthIndex);
  if (page < 0 || page >= kMaxPages ||
      bank < 0 || bank >= kBankCount ||
      pattern < 0 || pattern >= Bank<SynthPattern>::kPatterns) {
    return false;
  }

  receipt.pageIndex = static_cast<uint8_t>(page);
  receipt.synthIndex = static_cast<uint8_t>(synthIndex);
  receipt.bankIndex = static_cast<uint8_t>(bank);
  receipt.patternIndex = static_cast<uint8_t>(pattern);
  receipt.before = manager.getCurrentSynthPattern(synthIndex);
  return true;
}

inline bool synthPatternUndoTargetAvailable(
    const SceneManager& manager,
    const SynthPatternUndoPayload& receipt) {
  return validSynthPatternAddress(receipt) &&
         manager.currentPageIndex() == receipt.pageIndex;
}

inline void restoreSynthPatternUndo(SceneManager& manager,
                                    const SynthPatternUndoPayload& receipt) {
  Scene& scene = manager.currentScene();
  if (receipt.synthIndex == 0) {
    scene.synthABanks[receipt.bankIndex].patterns[receipt.patternIndex] =
        receipt.before;
  } else {
    scene.synthBBanks[receipt.bankIndex].patterns[receipt.patternIndex] =
        receipt.before;
  }
}

inline bool isCanonicalClearedSynthPattern(const SynthPattern& pattern) {
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

}  // namespace GroovePuterUndo
