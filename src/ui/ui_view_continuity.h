#pragma once
#ifndef GROOVEPUTER_SRC_UI_UI_VIEW_CONTINUITY_H
#define GROOVEPUTER_SRC_UI_UI_VIEW_CONTINUITY_H

#include <cstdint>

namespace UI {

// Runtime-only navigation/view continuity. Musical values remain owned by
// their existing engine, scene and performance controllers.
struct UiViewContinuityState {
  uint8_t synthTab[2]{0, 0};
  uint8_t phraseCursorCell[2]{0, 0};
  uint8_t phraseGrid[2]{1, 1};
  // PERFORM tools layer: visible flag and tab share one byte (the [LOOP] tab
  // made it six tabs and six per-tab rows; the struct stays 16 bytes).
  // Bit-fields cannot have default member initialisers before C++20: they
  // zero-initialise with `UiViewContinuityState{}`, which every owner uses.
  uint8_t performToolsVisible : 1;
  uint8_t performContext : 3;
  uint8_t performRows[6]{0, 0, 0, 0, 0, 0};
  uint8_t feelFocus{0};
  uint8_t feelPreset{1};
  uint8_t genreFocus{0};
};

static_assert(sizeof(UiViewContinuityState) <= 16,
              "runtime UI view continuity must remain tiny");
static_assert(sizeof(UiViewContinuityState) == 16,
              "the [LOOP] tab packs its state into the existing 16 bytes");

}  // namespace UI

#endif  // GROOVEPUTER_SRC_UI_UI_VIEW_CONTINUITY_H
