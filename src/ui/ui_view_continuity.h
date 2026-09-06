#pragma once

#include <cstdint>

namespace UI {

// Runtime-only navigation/view continuity. Musical values remain owned by
// their existing engine, scene and performance controllers.
struct UiViewContinuityState {
  uint8_t synthTab[2]{0, 0};
  uint8_t performToolsVisible{0};
  uint8_t performContext{0};
  uint8_t performRows[4]{0, 0, 0, 0};
  uint8_t feelFocus{0};
  uint8_t feelPreset{1};
  uint8_t genreFocus{0};
};

static_assert(sizeof(UiViewContinuityState) <= 16,
              "runtime UI view continuity must remain tiny");

}  // namespace UI
