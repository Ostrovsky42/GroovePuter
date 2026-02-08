#pragma once

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
  return e.key == '\t';
}

// Global navigation keys are reserved at the app level.
// IMPORTANT: To avoid breaking in-page editing (303/drums), global page jumps require CTRL.
// Bracket paging remains global without modifiers.
static inline bool isGlobalNav(const UIEvent& e) {
  // Page cycling is always global.
  if (e.key == '[' || e.key == ']') return true;

  // Help/back remain global-ish.
  if (e.key == 'h' || e.key == '`' || e.key == 0x1B || e.key == 0x08) {
    return true;
  }

  // Direct page jumps: require CTRL (or META) to prevent stealing normal editing keys.
  if (!(e.ctrl || e.meta)) return false;
  switch (e.key) {
    case 'g': case 'd': case 'e':
    case 'y': case 'Y':
    case 't': case 'm': case 's': case 'p':
      return true;
    default:
      return false;
  }
}

} // namespace UIInput
