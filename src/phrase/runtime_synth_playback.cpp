#include "runtime_synth_playback.h"

#include <cstdint>

namespace PhraseRuntime {
namespace {

void appendAction(RuntimeSynthPlaybackActions& actions,
                  RuntimeSynthPlaybackActionType type,
                  const RuntimeSynthEvent& event) {
  if (actions.count >= 2) return;
  RuntimeSynthPlaybackAction& action = actions.values[actions.count++];
  action.type = type;
  action.event = event;
}

bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

}  // namespace

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::acceptOnset(
    const RuntimeSynthEvent& event,
    uint32_t absoluteStartSubtick) {
  RuntimeSynthPlaybackActions actions{};
  if (active_) {
    appendAction(actions, RuntimeSynthPlaybackActionType::Release, activeEvent_);
  }

  activeEvent_ = event;
  active_ = true;
  const uint32_t duration = event.durationSubticks == 0
      ? 1u
      : static_cast<uint32_t>(event.durationSubticks);
  releaseAtSubtick_ = absoluteStartSubtick + duration;
  appendAction(actions, RuntimeSynthPlaybackActionType::Start, activeEvent_);
  return actions;
}

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::acceptRetrigger(
    const RuntimeSynthEvent& event) {
  RuntimeSynthPlaybackActions actions{};
  if (!active_ || event.note != activeEvent_.note) return actions;
  appendAction(actions, RuntimeSynthPlaybackActionType::Retrigger, event);
  return actions;
}

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::releaseDue(
    uint32_t absoluteSubtick) {
  RuntimeSynthPlaybackActions actions{};
  if (!active_ || !deadlineReached(absoluteSubtick, releaseAtSubtick_)) {
    return actions;
  }

  appendAction(actions, RuntimeSynthPlaybackActionType::Release, activeEvent_);
  active_ = false;
  activeEvent_ = RuntimeSynthEvent{};
  releaseAtSubtick_ = 0;
  return actions;
}

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::hardBarrier() {
  RuntimeSynthPlaybackActions actions{};
  if (!active_) return actions;

  appendAction(actions, RuntimeSynthPlaybackActionType::Release, activeEvent_);
  active_ = false;
  activeEvent_ = RuntimeSynthEvent{};
  releaseAtSubtick_ = 0;
  return actions;
}

}  // namespace PhraseRuntime
