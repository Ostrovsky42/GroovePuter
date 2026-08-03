#pragma once

#include <cstdint>

#include "../ui/ui_core.h"

namespace GroovePuterInput {

template <typename KeysState>
inline bool sameModifiers(const KeysState& a, const KeysState& b) {
  return a.alt == b.alt &&
         a.ctrl == b.ctrl &&
         a.shift == b.shift &&
         a.fn == b.fn;
}

template <typename KeysState>
inline bool modifierActivated(const KeysState& current,
                              const KeysState& previous) {
  return (current.alt && !previous.alt) ||
         (current.ctrl && !previous.ctrl) ||
         (current.shift && !previous.shift) ||
         (current.fn && !previous.fn);
}

template <typename KeysState>
inline bool modifierReleased(const KeysState& current,
                             const KeysState& previous) {
  return (!current.alt && previous.alt) ||
         (!current.ctrl && previous.ctrl) ||
         (!current.shift && previous.shift) ||
         (!current.fn && previous.fn);
}

template <typename KeysState>
inline bool containsHid(const KeysState& state, uint8_t hid) {
  for (const auto value : state.hid_keys) {
    if (static_cast<uint8_t>(value) == hid) return true;
  }
  return false;
}

template <typename KeysState, typename WordChar>
inline bool containsWord(const KeysState& state, WordChar value) {
  for (const auto current : state.word) {
    if (current == value) return true;
  }
  return false;
}

template <typename KeysState>
inline bool shouldDispatchHid(const KeysState& current,
                              const KeysState& previous,
                              bool hadPrevious,
                              uint8_t hid) {
  if (!hadPrevious) return true;
  if (!containsHid(previous, hid)) return true;
  return modifierActivated(current, previous);
}

template <typename KeysState, typename WordChar>
inline bool shouldDispatchWord(const KeysState& current,
                               const KeysState& previous,
                               bool hadPrevious,
                               WordChar value) {
  (void)current;
  return !hadPrevious || !containsWord(previous, value);
}

inline bool mayRepeat(const UIEvent& event) {
  if (event.alt || event.ctrl || event.shift || event.meta) return false;
  return event.scancode == GROOVEPUTER_UP ||
         event.scancode == GROOVEPUTER_DOWN ||
         event.scancode == GROOVEPUTER_LEFT ||
         event.scancode == GROOVEPUTER_RIGHT;
}

template <typename KeysState>
inline bool repeatKeyStillHeld(const KeysState& state,
                               uint8_t hid,
                               const UIEvent& event) {
  if (state.hid_keys.size() != 1) return false;
  if (!containsHid(state, hid)) return false;
  return state.alt == event.alt &&
         state.ctrl == event.ctrl &&
         state.shift == event.shift &&
         state.fn == event.meta;
}

}  // namespace GroovePuterInput
