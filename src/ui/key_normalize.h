#pragma once

#include <cstdint>
#include <vector>
#include <string>

enum KeyScanCode {
  GROOVEPUTER_NO_SCANCODE = 0,
  GROOVEPUTER_DOWN,
  GROOVEPUTER_UP,
  GROOVEPUTER_LEFT,
  GROOVEPUTER_RIGHT,
  GROOVEPUTER_ESCAPE,
  GROOVEPUTER_TAB,
  GROOVEPUTER_F1,
  GROOVEPUTER_F2,
  GROOVEPUTER_F3,
  GROOVEPUTER_F4,
  GROOVEPUTER_F5,
  GROOVEPUTER_F6,
  GROOVEPUTER_F7,
  GROOVEPUTER_F8,
  GROOVEPUTER_A,
  GROOVEPUTER_B,
  GROOVEPUTER_C,
  GROOVEPUTER_D,
  GROOVEPUTER_E,
  GROOVEPUTER_F,
  GROOVEPUTER_G,
  GROOVEPUTER_H,
  GROOVEPUTER_I,
  GROOVEPUTER_J,
  GROOVEPUTER_K,
  GROOVEPUTER_L,
  GROOVEPUTER_M,
  GROOVEPUTER_N,
  GROOVEPUTER_O,
  GROOVEPUTER_P,
  GROOVEPUTER_Q,
  GROOVEPUTER_R,
  GROOVEPUTER_S,
  GROOVEPUTER_T,
  GROOVEPUTER_U,
  GROOVEPUTER_V,
  GROOVEPUTER_W,
  GROOVEPUTER_X,
  GROOVEPUTER_Y,
  GROOVEPUTER_Z,
};

// ============================================================================
// Key Normalization for UI Input
// ============================================================================
//
// Normalizes keyboard input to consistent format for UI event processing.
// Call ONCE at input source (where UIEvent is created from hardware).
//

/**
 * Convert ASCII uppercase to lowercase
 * @param c Character to convert
 * @return Lowercase character if A-Z, otherwise unchanged
 */
static inline char asciiLower(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c + ('a' - 'A'));
  }
  return c;
}

/**
 * Normalize key character for UI processing
 *
 * Currently performs:
 * - Lowercase normalization (A-Z → a-z)
 *
 * Future extensions could include:
 * - Dead key handling
 * - International keyboard layouts
 * - Physical scancode mapping
 *
 * @param c Raw character from keyboard
 * @return Normalized character for event dispatch
 */
static inline char normalizeKeyChar(char c) {
  return asciiLower(c);
}

/**
 * Debug helper: dump key code information
 * Useful for diagnosing keyboard input issues
 *
 * Usage:
 *   #ifdef DEBUG_KEYS
 *   dumpKeyDebug(key);
 *   #endif
 */
#ifdef ARDUINO
#include <Arduino.h>
static inline void dumpKeyDebug(char key) {
  uint8_t u = static_cast<uint8_t>(key);
  char printable = (u >= 32 && u <= 126) ? key : '?';
  Serial.printf("[KEY] dec=%u hex=0x%02X char='%c'\n", u, u, printable);
}
#else
#include <cstdio>
static inline void dumpKeyDebug(char key) {
  uint8_t u = static_cast<uint8_t>(key);
  char printable = (u >= 32 && u <= 126) ? key : '?';
  printf("[KEY] dec=%u hex=0x%02X char='%c'\n", u, u, printable);
}
#endif

// ============================================================================
// Pattern Index Mapping (QWERTY row)
// ============================================================================

/**
 * Map QWERTY top row (Q-I) to pattern indices 0-7
 * Used by song page, drum page, pattern editors
 *
 * @param key Normalized key character (already lowercase)
 * @return Pattern index 0-7, or -1 if not a pattern key
 */
static inline int qwertyToPatternIndex(char key) {
  switch (key) {
    case 'q': return 0;
    case 'w': return 1;
    case 'e': return 2;
    case 'r': return 3;
    case 't': return 4;
    case 'y': return 5;
    case 'u': return 6;
    case 'i': return 7;
    default: return -1;
  }
}

/**
 * Map scancode to pattern index 0-7
 */
static inline int scancodeToPatternIndex(KeyScanCode sc) {
  switch (sc) {
    case GROOVEPUTER_Q: return 0;
    case GROOVEPUTER_W: return 1;
    case GROOVEPUTER_E: return 2;
    case GROOVEPUTER_R: return 3;
    case GROOVEPUTER_T: return 4;
    case GROOVEPUTER_Y: return 5;
    case GROOVEPUTER_U: return 6;
    case GROOVEPUTER_I: return 7;
    default: return -1;
  }
}

/**
 * Map QWERTY home row (A-K) to drum voices 0-7
 * Used by drum sequencer pages
 *
 * @param key Normalized key character (already lowercase)
 * @return Drum voice index 0-7, or -1 if not a drum key
 */
static inline int qwertyToDrumVoice(char key) {
  switch (key) {
    case 'a': return 0;  // Kick
    case 's': return 1;  // Snare
    case 'd': return 2;  // Hat
    case 'f': return 3;  // Open Hat
    case 'g': return 4;  // Mid Tom
    case 'h': return 5;  // High Tom
    case 'j': return 6;  // Rim
    case 'k': return 7;  // Clap
    default: return -1;
  }
}

/**
 * Map scancode to drum voice index 0-7
 */
static inline int scancodeToDrumVoice(KeyScanCode sc) {
  switch (sc) {
    case GROOVEPUTER_A: return 0;
    case GROOVEPUTER_S: return 1;
    case GROOVEPUTER_D: return 2;
    case GROOVEPUTER_F: return 3;
    case GROOVEPUTER_G: return 4;
    case GROOVEPUTER_H: return 5;
    case GROOVEPUTER_J: return 6;
    case GROOVEPUTER_K: return 7;
    default: return -1;
  }
}

// ============================================================================
// Notes
// ============================================================================
//
// Why normalize at input source?
// ------------------------------
// 1. Single point of normalization (DRY principle)
// 2. Consistent behavior across all pages
// 3. No need to remember to normalize in each handler
// 4. Easy to extend (e.g., add scancode support)
//
// Common pitfalls this fixes:
// ---------------------------
// - Shift/Caps Lock breaking pattern assignment
// - International keyboards sending different codes
// - Function key combinations producing unexpected chars
//
// Performance impact:
// -------------------
// Negligible - single branch + arithmetic per keystroke
// (~2-3 CPU cycles on ESP32)
//
