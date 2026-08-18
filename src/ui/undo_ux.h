#pragma once

#include "ui_core.h"

namespace GroovePuterUndoUx {

inline bool isUndoShortcut(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN || !event.ctrl ||
      event.alt || event.meta || event.shift) {
    return false;
  }
  const unsigned char key = static_cast<unsigned char>(event.key);
  return event.scancode == GROOVEPUTER_Z || event.key == 'z' || event.key == 'Z' ||
         key == 26;  // Ctrl+Z control character on terminals/Cardputer paths.
}

inline bool promoteUndoShortcut(UIEvent& event) {
  if (!isUndoShortcut(event)) return false;
  event.event_type = GROOVEPUTER_APPLICATION_EVENT;
  event.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
  return true;
}

inline bool isUndoEvent(const UIEvent& event) {
  return event.event_type == GROOVEPUTER_APPLICATION_EVENT &&
         event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO;
}

// Ctrl+Z never navigates. A receipt owned by another page/subpage stays intact;
// Esc remains the explicit back/return gesture.
inline const char* fallbackToast(bool hasRetainedReceipt) {
  return hasRetainedReceipt ? "UNDO: NOT HERE" : "UNDO: EMPTY";
}

}  // namespace GroovePuterUndoUx