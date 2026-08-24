#pragma once

#include <cstdint>

#include "../ui/ui_core.h"

namespace GroovePuterInput {

inline constexpr uint8_t kCardputerLetterAHid = 0x04;
inline constexpr uint8_t kCardputerLetterZHid = 0x1D;
inline constexpr uint8_t kCardputerEnterHid = 0x28;
inline constexpr uint8_t kCardputerTabHid = 0x2B;
inline constexpr uint8_t kCardputerArrowUpHid = 0x33;
inline constexpr uint8_t kCardputerArrowLeftHid = 0x36;
inline constexpr uint8_t kCardputerArrowDownHid = 0x37;
inline constexpr uint8_t kCardputerArrowRightHid = 0x38;
inline constexpr uint8_t kCardputerKeypadEnterHid = 0x58;

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

inline uint8_t asciiLetterHid(char value) {
  if (value >= 'A' && value <= 'Z') {
    value = static_cast<char>(value + ('a' - 'A'));
  }
  if (value < 'a' || value > 'z') return 0;
  return static_cast<uint8_t>(kCardputerLetterAHid + (value - 'a'));
}

template <typename KeysState>
inline bool enterHidDown(const KeysState& state) {
  return containsHid(state, kCardputerEnterHid) ||
         containsHid(state, kCardputerKeypadEnterHid);
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

  // M5Cardputer library versions differ in whether dedicated keys appear in
  // KeysState::word, hid_keys, or both. Prefer the canonical HID event when it
  // exists. For the Alt-only combinations that must reach the UI dispatcher,
  // tunnel a word-only value through GroovePuter.ino's raw control/modifier
  // filter and restore the original logical key immediately before UIEvent
  // dispatch. Plain Enter and Ctrl paths intentionally keep prior behavior.
  if (rawValue == static_cast<WordChar>('\t') &&
      containsHid(current, kCardputerTabHid)) {
    return false;
  }

  const char rawChar = static_cast<char>(rawValue);
  const uint8_t letterHid = asciiLetterHid(rawChar);
  const bool altOnly = current.alt && !current.ctrl;
  const bool enterWord = rawValue == static_cast<WordChar>('\n') ||
                         rawValue == static_cast<WordChar>('\r');
  const bool altWordFallback = altOnly && (letterHid != 0 || enterWord);

  if (altWordFallback && enterWord && enterHidDown(current)) {
    return false;
  }
  if (altWordFallback && letterHid != 0 && containsHid(current, letterHid)) {
    return false;
  }

  bool dispatch = !hadPrevious || !containsWord(previous, rawValue);
  if (!dispatch && altWordFallback && hadPrevious &&
      current.alt && !previous.alt) {
    dispatch = true;
  }
  if (!dispatch) return false;

  if (rawValue == static_cast<WordChar>('\t')) {
    value = static_cast<WordChar>(GROOVEPUTER_WORD_TAB_SENTINEL);
  } else if (altWordFallback && enterWord) {
    value = static_cast<WordChar>(GROOVEPUTER_WORD_ENTER_SENTINEL);
  } else if (altWordFallback && letterHid != 0) {
    value = static_cast<WordChar>(stageWordAltLetterFallback(rawChar));
  }
  return true;
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
