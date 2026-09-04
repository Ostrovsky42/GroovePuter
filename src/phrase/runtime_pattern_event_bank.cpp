#include "runtime_pattern_event_bank.h"

namespace PhraseRuntime {
namespace {

bool validAddress(uint8_t synthIndex,
                  uint8_t bankIndex,
                  uint8_t patternIndex) {
  return synthIndex < kPatternRuntimeSynthCount &&
         bankIndex < kBankCount &&
         patternIndex < Bank<SynthPattern>::kPatterns;
}

}  // namespace

PatternBankRefreshStatus RuntimePatternEventBank::refresh(
    uint8_t synthIndex,
    uint8_t bankIndex,
    uint8_t patternIndex,
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings) {
  if (!validAddress(synthIndex, bankIndex, patternIndex)) {
    return PatternBankRefreshStatus::InvalidAddress;
  }
  if (settings.synthIndex != synthIndex) {
    return PatternBankRefreshStatus::InvalidSettings;
  }

  RuntimeSynthEventBuffer projected{};
  if (projectPatternToRuntimeEvents(pattern, settings, projected) !=
      PatternProjectionStatus::Ready) {
    return PatternBankRefreshStatus::ProjectionFailed;
  }
  if (projected.count > kPatternRuntimeMaxEvents) {
    return PatternBankRefreshStatus::ProjectionFailed;
  }

  RuntimePatternEventBuffer candidate{};
  candidate.count = static_cast<uint8_t>(projected.count);
  for (uint8_t i = 0; i < candidate.count; ++i) {
    candidate.events[i] = projected.events[i];
  }

  buffers_[synthIndex][bankIndex][patternIndex] = candidate;
  return PatternBankRefreshStatus::Ready;
}

const RuntimePatternEventBuffer& RuntimePatternEventBank::select(
    uint8_t synthIndex,
    uint8_t bankIndex,
    uint8_t patternIndex) const {
  if (!validAddress(synthIndex, bankIndex, patternIndex)) return empty_;
  return buffers_[synthIndex][bankIndex][patternIndex];
}

}  // namespace PhraseRuntime
