#pragma once

#ifndef GROOVEPUTER_SRC_UI_UI_INPUT_H_
#define GROOVEPUTER_SRC_UI_UI_INPUT_H_

#include "ui_core.h"

// Unified input helpers for arrow-first navigation.
// Cardputer input can deliver arrows in scancode OR key, depending on firmware/driver path.
// This header normalizes navigation so pages behave consistently.

namespace UIInput {

static inline bool isArrowCode(int v) {
  return v == GROOVEPUTER_UP || v == GROOVEPUTER_DOWN ||
         v == GROOVEPUTER_LEFT || v == GROOVEPUTER_RIGHT;
}

// Normalize navigation: check scancode first, then key.
static inline int navCode(const UIEvent& e) {
  if (isArrowCode(e.scancode)) return e.scancode;
  if (isArrowCode(e.key)) return e.key;
  return 0;
}

static inline bool isUp(const UIEvent& e)    { return navCode(e) == GROOVEPUTER_UP; }
static inline bool isDown(const UIEvent& e)  { return navCode(e) == GROOVEPUTER_DOWN; }
static inline bool isLeft(const UIEvent& e)  { return navCode(e) == GROOVEPUTER_LEFT; }
static inline bool isRight(const UIEvent& e) { return navCode(e) == GROOVEPUTER_RIGHT; }

static inline bool isConfirm(const UIEvent& e) {
  return e.key == '\n' || e.key == '\r';
}

static inline bool isBack(const UIEvent& e) {
  return e.key == 0x1B /*esc*/ || e.key == 0x08; /*backspace*/ //||
         //e.key == 'b' || e.key == 'B' || e.key == '`';
}

static inline bool isTab(const UIEvent& e) {
  return e.key == '\t' || e.scancode == GROOVEPUTER_TAB;
}

// Converts the existing 80 ms Cardputer arrow repeat stream into a bounded
// value-step multiplier. A tap remains exact; a continuous hold ramps through
// x1 -> x2 -> x4 -> x5. Direction changes and gaps between events reset it.
// Pages opt in only for continuous numeric ranges so menu/list navigation does
// not accelerate accidentally.
class HoldAccelerator {
 public:
  int multiplier(int direction, bool forcedFast = false) {
    return multiplierAt(direction, millis(), forcedFast);
  }

  int multiplierAt(int direction, uint32_t nowMs, bool forcedFast = false) {
    if (direction == 0) {
      reset();
      return 1;
    }

    if (direction == last_direction_ &&
        static_cast<uint32_t>(nowMs - last_event_ms_) <= 160u) {
      if (streak_ < 32) ++streak_;
    } else {
      streak_ = 0;
    }
    last_direction_ = direction;
    last_event_ms_ = nowMs;

    if (forcedFast) return 5;
    if (streak_ >= 14) return 5;
    if (streak_ >= 8) return 4;
    if (streak_ >= 3) return 2;
    return 1;
  }

  void reset() {
    last_direction_ = 0;
    last_event_ms_ = 0;
    streak_ = 0;
  }

 private:
  int last_direction_ = 0;
  uint32_t last_event_ms_ = 0;
  uint8_t streak_ = 0;
};

// Global navigation keys are reserved at the app level.
// IMPORTANT: To avoid breaking in-page editing (303/drums), global page jumps require CTRL.
// Bracket paging remains global without modifiers.
static inline bool isGlobalNav(const UIEvent& e) {
  // Page cycling is always global.
  if (e.key == '[' || e.key == ']') return true;

  // Help/backtick/ESC remain global-ish.
  // Backspace is intentionally NOT global here: pages should get first chance
  // to handle local clear actions (REST, clear step/selection/pattern).
  if (e.key == 'h' || e.key == '`' || e.key == 0x1B) {
    return true;
  }

  // Direct page jumps: require CTRL (or META) to prevent stealing normal editing keys.
  if (!(e.ctrl || e.meta)) return false;
  switch (e.key) {
    case 'g': case 'd': case 'e':
    case 'y': case 'Y':
    case 't': case 'm': case 'p':
      return true;
    default:
      return false;
  }
}

} // namespace UIInput

#endif  // GROOVEPUTER_SRC_UI_UI_INPUT_H_
