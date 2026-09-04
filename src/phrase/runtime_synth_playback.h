#pragma once

#include <cstdint>
#include <type_traits>

#include "runtime_synth_events.h"

namespace PhraseRuntime {

enum class RuntimeSynthPlaybackActionType : uint8_t {
  Release = 0,
  Start,
  Retrigger,
};

struct RuntimeSynthPlaybackAction {
  RuntimeSynthPlaybackActionType type = RuntimeSynthPlaybackActionType::Release;
  RuntimeSynthEvent event{};
};

struct RuntimeSynthPlaybackActions {
  RuntimeSynthPlaybackAction values[2]{};
  uint8_t count = 0;
};

class RuntimeSynthPlaybackState {
 public:
  RuntimeSynthPlaybackActions acceptOnset(const RuntimeSynthEvent& event,
                                           uint32_t absoluteStartSubtick);
  RuntimeSynthPlaybackActions acceptRetrigger(const RuntimeSynthEvent& event);
  RuntimeSynthPlaybackActions releaseDue(uint32_t absoluteSubtick);
  RuntimeSynthPlaybackActions hardBarrier();

  bool active() const { return active_; }
  uint8_t activeNote() const { return activeEvent_.note; }
  uint32_t releaseAtSubtick() const { return releaseAtSubtick_; }

 private:
  bool active_ = false;
  RuntimeSynthEvent activeEvent_{};
  uint32_t releaseAtSubtick_ = 0;
};

static_assert(std::is_trivially_copyable<RuntimeSynthPlaybackAction>::value,
              "P2 playback action must remain trivially copyable");
static_assert(std::is_trivially_copyable<RuntimeSynthPlaybackActions>::value,
              "P2 playback action batch must remain trivially copyable");
static_assert(std::is_trivially_copyable<RuntimeSynthPlaybackState>::value,
              "P2 playback state must remain fixed/trivially copyable");
static_assert(sizeof(RuntimeSynthPlaybackActions) <= 32,
              "P2 playback action batch must stay bounded");

}  // namespace PhraseRuntime
