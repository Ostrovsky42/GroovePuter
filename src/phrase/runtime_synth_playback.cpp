#include "runtime_synth_playback.h"

#include <cstdint>

namespace PhraseRuntime {
namespace {

void appendAction(RuntimeSynthPlaybackActions& actions,
                  RuntimeSynthPlaybackActionType type,
                  const RuntimeSynthEvent& event) {
  if (actions.count >= kMaxChordVoices + 1) return;
  RuntimeSynthPlaybackAction& action = actions.values[actions.count++];
  action.type = type;
  action.event = event;
}

bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

uint32_t releaseAtFor(const RuntimeSynthEvent& event, uint32_t start) {
  const uint32_t duration = event.durationSubticks == 0
      ? 1u
      : static_cast<uint32_t>(event.durationSubticks);
  return start + duration;
}

}  // namespace

void RuntimeSynthPlaybackState::removeVoice(uint8_t index) {
  if (index >= count_) return;
  for (uint8_t i = index; i + 1u < count_; ++i) voices_[i] = voices_[i + 1u];
  --count_;
  voices_[count_] = Voice{};
}

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::acceptOnset(
    const RuntimeSynthEvent& event,
    uint32_t absoluteStartSubtick) {
  RuntimeSynthPlaybackActions actions{};
  for (uint8_t i = 0; i < count_; ++i) {
    appendAction(actions, RuntimeSynthPlaybackActionType::Release,
                 voices_[i].event);
  }
  count_ = 0;
  for (Voice& voice : voices_) voice = Voice{};

  voices_[0].event = event;
  voices_[0].releaseAt = releaseAtFor(event, absoluteStartSubtick);
  count_ = 1;
  appendAction(actions, RuntimeSynthPlaybackActionType::Start, voices_[0].event);
  return actions;
}

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::acceptChordOnset(
    const RuntimeSynthEvent& event,
    uint32_t absoluteStartSubtick) {
  RuntimeSynthPlaybackActions actions{};
  // Notes ending exactly here release first, so a note that follows another
  // reaches MIDI as NoteOff -> NoteOn rather than as a legato overlap.
  for (uint8_t i = 0; i < count_;) {
    if (deadlineReached(absoluteStartSubtick, voices_[i].releaseAt)) {
      appendAction(actions, RuntimeSynthPlaybackActionType::Release,
                   voices_[i].event);
      removeVoice(i);
    } else {
      ++i;
    }
  }
  // The same pitch again restarts that voice instead of stacking.
  for (uint8_t i = 0; i < count_; ++i) {
    if (voices_[i].event.note == event.note) {
      appendAction(actions, RuntimeSynthPlaybackActionType::Release,
                   voices_[i].event);
      removeVoice(i);
      break;
    }
  }
  // Full: the oldest voice makes room.
  if (count_ >= kMaxChordVoices) {
    appendAction(actions, RuntimeSynthPlaybackActionType::Release,
                 voices_[0].event);
    removeVoice(0);
  }

  Voice& voice = voices_[count_++];
  voice.event = event;
  voice.event.flags = static_cast<uint8_t>(voice.event.flags | kEventChordVoice);
  voice.releaseAt = releaseAtFor(event, absoluteStartSubtick);
  appendAction(actions, RuntimeSynthPlaybackActionType::Start, voice.event);
  return actions;
}

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::acceptRetrigger(
    const RuntimeSynthEvent& event) {
  RuntimeSynthPlaybackActions actions{};
  for (uint8_t i = 0; i < count_; ++i) {
    if (voices_[i].event.note != event.note) continue;
    RuntimeSynthEvent retrigger = event;
    retrigger.flags = static_cast<uint8_t>(
        retrigger.flags | (voices_[i].event.flags & kEventChordVoice));
    appendAction(actions, RuntimeSynthPlaybackActionType::Retrigger, retrigger);
    break;
  }
  return actions;
}

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::releaseDue(
    uint32_t absoluteSubtick) {
  RuntimeSynthPlaybackActions actions{};
  for (uint8_t i = 0; i < count_;) {
    if (deadlineReached(absoluteSubtick, voices_[i].releaseAt)) {
      appendAction(actions, RuntimeSynthPlaybackActionType::Release,
                   voices_[i].event);
      removeVoice(i);
    } else {
      ++i;
    }
  }
  return actions;
}

RuntimeSynthPlaybackActions RuntimeSynthPlaybackState::hardBarrier() {
  RuntimeSynthPlaybackActions actions{};
  for (uint8_t i = 0; i < count_; ++i) {
    appendAction(actions, RuntimeSynthPlaybackActionType::Release,
                 voices_[i].event);
  }
  count_ = 0;
  for (Voice& voice : voices_) voice = Voice{};
  return actions;
}

int RuntimeSynthPlaybackState::topNote() const {
  int top = -1;
  for (uint8_t i = 0; i < count_; ++i) {
    if (static_cast<int>(voices_[i].event.note) > top) {
      top = voices_[i].event.note;
    }
  }
  return top;
}

}  // namespace PhraseRuntime
