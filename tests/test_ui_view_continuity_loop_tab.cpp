// 0.9.23 T14: the runtime UI view continuity keeps its size (16 bytes) while
// PERFORM gains the [LOOP] tab (six contexts, six rows). The tools-visible flag
// and the context share one byte (1 + 3 bits); the rows keep a byte each.
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/ui/ui_view_continuity.h"

int main() {
  static_assert(sizeof(UI::UiViewContinuityState) == 16, "must stay exactly 16 bytes");
  UI::UiViewContinuityState s{};
  assert(s.performToolsVisible == 0 && s.performContext == 0);
  assert(s.feelPreset == 1 && s.phraseGrid[0] == 1);
  // The widest values round-trip and do not leak into their neighbours.
  s.performToolsVisible = 1;
  s.performContext = 5;  // LOOP
  for (int i = 0; i < 6; ++i) s.performRows[i] = static_cast<uint8_t>(i + 1);
  s.feelFocus = 9;
  assert(s.performToolsVisible == 1 && s.performContext == 5);
  assert(s.performRows[5] == 6 && s.feelFocus == 9);
  s.performContext = 7;  // the 3-bit maximum
  assert(s.performToolsVisible == 1 && s.performContext == 7);
  s.performToolsVisible = 0;
  assert(s.performContext == 7);
  // six per-tab rows
  assert(sizeof(s.performRows) == 6);
  std::puts("test_ui_view_continuity_loop_tab: OK");
  return 0;
}
