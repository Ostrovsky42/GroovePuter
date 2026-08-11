#pragma once

#include <cstdint>

#include "../ui/ui_core.h"

namespace GroovePuterInput {

inline constexpr uint8_t kCardputerTabHid = 0x2B;
inline constexpr uint8_t kCardputerArrowUpHid = 0x33;
inline constexpr uint8_t kCardputerArrowLeftHid = 0x36;
inline constexpr uint8_t kCardputerArrowDownHid = 0x37;
inline constexpr uint8_t kCardputerArrowRightHid = 0x38;

#if defined(ARDUINO_M5STACK_CARDPUTER)
// Temporary #239 hardware-audition hook. It consumes only the exact
// Ctrl+Alt+O entry chord and, while active, owns Cardputer HID/word input so
// the user's project cannot be edited accidentally during the fixture test.
bool p23AuditionConsumeCardputerHid(bool alt,
                                    bool ctrl,
                                    bool shift,
                                    bool fn,
                                    uint8_t hid);
bool p23AuditionActive();
#endif

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
  const bool dispatch =
      !hadPrevious || !containsHid(previous, hid) ||
      modifierActivated(current, previous);
  if (!dispatch) return false;
#if defined(ARDUINO_M5STACK_CARDPUTER)
  if (p23AuditionConsumeCardputerHid(
          current.alt, current.ctrl, current.shift, current.fn, hid)) {
    return false;
  }
#endif
  return true;
}

template <typename KeysState, typename WordChar>
inline bool shouldDispatchWord(const KeysState& current,
                               const KeysState& previous,
                               bool hadPrevious,
                               WordChar& value) {
#if defined(ARDUINO_M5STACK_CARDPUTER)
  if (p23AuditionActive()) return false;
#endif
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

inline uint32_t letterDispatchMask(char value) {
  if (value >= 'A' && value <= 'Z') value = static_cast<char>(value + ('a' - 'A'));
  if (value < 'a' || value > 'z') return 0u;
  return uint32_t{1} << static_cast<uint32_t>(value - 'a');
}

template <typename WordChar>
inline bool wordLetterAlreadyDispatched(WordChar value, uint32_t mask) {
  return (mask & letterDispatchMask(static_cast<char>(value))) != 0u;
}

inline bool mayRepeat(const UIEvent& event) {
  if (event.alt || event.ctrl || event.shift || event.meta) return false;
  return event.scancode == GROOVEPUTER_UP ||
         event.scancode == GROOVEPUTER_DOWN ||
         event.scancode == GROOVEPUTER_LEFT ||
         event.scancode == GROOVEPUTER_RIGHT;
}

inline bool isCardputerArrowHid(uint8_t hid) {
  return hid == kCardputerArrowUpHid ||
         hid == kCardputerArrowDownHid ||
         hid == kCardputerArrowLeftHid ||
         hid == kCardputerArrowRightHid;
}

template <typename KeysState>
inline bool mayArmRepeatForPhysicalKey(const KeysState& state,
                                       uint8_t hid,
                                       const UIEvent& event) {
  if (!mayRepeat(event) || state.hid_keys.size() != 1) return false;

  // Cardputer arrow legends occupy punctuation positions. Some M5Cardputer
  // versions report the physical arrow simultaneously through hid_keys and
  // word. The canonical HID arrow remains repeatable in that representation;
  // word.empty() is only required for other repeat candidates.
  return state.word.empty() || isCardputerArrowHid(hid);
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
