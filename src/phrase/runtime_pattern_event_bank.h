#pragma once

#include <cstdint>
#include <type_traits>

#include "runtime_synth_events.h"

namespace PhraseRuntime {

constexpr uint8_t kPatternRuntimeSynthCount = 2;
constexpr uint8_t kPatternRuntimeMaxEvents = SynthPattern::kSteps;
constexpr int8_t kInvalidPatternRuntimePage = -1;

struct RuntimePatternEventBuffer {
  RuntimeSynthEvent events[kPatternRuntimeMaxEvents]{};
  uint8_t count = 0;

  constexpr uint16_t lengthTicks() const { return kTicksPerBar; }
};

enum class PatternBankRefreshStatus : uint8_t {
  Ready = 0,
  InvalidAddress,
  InvalidSettings,
  ProjectionFailed,
};

class RuntimePatternEventBank {
 public:
  PatternBankRefreshStatus refresh(
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

  const RuntimePatternEventBuffer& select(
      uint8_t synthIndex,
      uint8_t bankIndex,
      uint8_t patternIndex) const {
    if (!validAddress(synthIndex, bankIndex, patternIndex)) return empty_;
    return buffers_[synthIndex][bankIndex][patternIndex];
  }

  const RuntimePatternEventBuffer& selectForPage(
      int pageIdentity,
      uint8_t synthIndex,
      uint8_t bankIndex,
      uint8_t patternIndex) const {
    if (pageIdentity_ == kInvalidPatternRuntimePage ||
        pageIdentity != pageIdentity_) {
      return empty_;
    }
    return select(synthIndex, bankIndex, patternIndex);
  }

  int pageIdentity() const { return static_cast<int>(pageIdentity_); }

  bool publishPageIdentity(int pageIdentity) {
    if (pageIdentity < 0 || pageIdentity >= kMaxPages) return false;
    pageIdentity_ = static_cast<int8_t>(pageIdentity);
    return true;
  }

  void invalidatePageIdentity() {
    pageIdentity_ = kInvalidPatternRuntimePage;
  }

  const RuntimePatternEventBuffer& empty() const { return empty_; }

 private:
  static bool validAddress(uint8_t synthIndex,
                           uint8_t bankIndex,
                           uint8_t patternIndex) {
    return synthIndex < kPatternRuntimeSynthCount &&
           bankIndex < kBankCount &&
           patternIndex < Bank<SynthPattern>::kPatterns;
  }

  RuntimePatternEventBuffer buffers_[kPatternRuntimeSynthCount]
                                    [kBankCount]
                                    [Bank<SynthPattern>::kPatterns]{};
  RuntimePatternEventBuffer empty_{};
  int8_t pageIdentity_ = kInvalidPatternRuntimePage;
};

static_assert(kPatternRuntimeMaxEvents == SynthPattern::kSteps,
              "retained Pattern capacity must track physical Pattern steps");
static_assert(std::is_trivially_copyable<RuntimePatternEventBuffer>::value,
              "retained Pattern event buffer must remain trivially copyable");
static_assert(std::is_trivially_copyable<RuntimePatternEventBank>::value,
              "retained Pattern event bank must remain trivially copyable");
static_assert(sizeof(RuntimePatternEventBuffer) <= 162,
              "retained Pattern event buffer exceeded its fixed budget");
static_assert(sizeof(RuntimePatternEventBank) <= 5500,
              "retained Pattern event bank exceeded its fixed budget");

}  // namespace PhraseRuntime
