#pragma once

#include <cstdint>

#include "../ui/ui_core.h"

namespace GroovePuterInput {

inline constexpr uint8_t kCardputerTabHid = 0x2B;

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
                               WordChar& value) {
  const WordChar rawValue = value;

  // M5Cardputer library versions differ: dedicated Tab can appear only in
  // KeysState::word or in both word and HID 0x2B. Preserve a word-only Tab
  // through the control-character filter and suppress the duplicate word copy
  // when the canonical HID event is present.
  if (rawValue == static_cast<WordChar>('\t') &&
      containsHid(current, kCardputerTabHid)) {
    return false;
  }

  const bool dispatch =
      !hadPrevious || !containsWord(previous, rawValue);
  if (dispatch && rawValue == static_cast<WordChar>('\t')) {
    value = static_cast<WordChar>(GROOVEPUTER_WORD_TAB_SENTINEL);
  }
  return dispatch;
}

inline uint16_t digitDispatchMask(char value) {
  const unsigned char digit = static_cast<unsigned char>(value);
  if (digit < '0' || digit > '9') return 0;
  return static_cast<uint16_t>(1u << (digit - '0'));
}

template <typename WordChar>
inline bool wordDigitAlreadyDispatched(WordChar value,
                                       uint16_t dispatchedDigitMask) {
  const unsigned char digit = static_cast<unsigned char>(value);
  if (digit < '0' || digit > '9') return false;
  return (dispatchedDigitMask &
          static_cast<uint16_t>(1u << (digit - '0'))) != 0;
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

// This header is included by GroovePuter.ino after miniacid_display.h. The
// sketch has exactly two display-event dispatch calls; the source regression
// guards that boundary. On Cardputer only, route those calls through the
// page-aware adapter so Phrase Alt+W reaches its destructive write command.
#if defined(ARDUINO_M5STACK_CARDPUTER)
#define handleEvent(...) handleCardputerEvent(__VA_ARGS__)
#endif
