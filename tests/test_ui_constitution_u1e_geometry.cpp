#include <cassert>

#include "../src/ui/screen_geometry.h"

namespace {
constexpr int rectEnd(const LayoutRect& rect) {
  return rect.y + rect.h;
}
}  // namespace

int main() {
  static_assert(Layout::SCREEN_W == 240);
  static_assert(Layout::SCREEN_H == 135);

  static_assert(Layout::HEADER.x == 0);
  static_assert(Layout::HEADER.y == 0);
  static_assert(Layout::HEADER.w == 240);
  static_assert(Layout::HEADER.h == 16);

  // UI Constitution V1: the active surface body ends before the shell-owned
  // performance strip. Current pre-U1E geometry fails this at CONTENT.h=103.
  static_assert(Layout::CONTENT.x == 0);
  static_assert(Layout::CONTENT.y == 16);
  static_assert(Layout::CONTENT.w == 240);
  static_assert(Layout::CONTENT.h == 93);

  static_assert(Layout::PERFORMANCE_HUD.x == 0);
  static_assert(Layout::PERFORMANCE_HUD.y == 109);
  static_assert(Layout::PERFORMANCE_HUD.w == 240);
  static_assert(Layout::PERFORMANCE_HUD.h == 10);

  static_assert(Layout::FOOTER.x == 0);
  static_assert(Layout::FOOTER.y == 119);
  static_assert(Layout::FOOTER.w == 240);
  static_assert(Layout::FOOTER.h == 16);

  // Four standard zones form one exact vertical partition.
  static_assert(rectEnd(Layout::HEADER) == Layout::CONTENT.y);
  static_assert(rectEnd(Layout::CONTENT) == Layout::PERFORMANCE_HUD.y);
  static_assert(rectEnd(Layout::PERFORMANCE_HUD) == Layout::FOOTER.y);
  static_assert(rectEnd(Layout::FOOTER) == Layout::SCREEN_H);

  // The compact eighth baseline remains valid; U1E does not redesign existing
  // line-7 status/hint placement merely because only seven full 12px rows fit.
  constexpr int line7 = Layout::CONTENT.y + Layout::CONTENT_PAD_Y +
                        7 * Layout::LINE_HEIGHT;
  static_assert(line7 == 102);
  static_assert(line7 < Layout::PERFORMANCE_HUD.y);
  static_assert(Layout::MAX_LINES == 8);

  // The wide performance waveform remains completely shell-owned.
  static_assert(Layout::PERFORMANCE_WAVEFORM.y >= Layout::PERFORMANCE_HUD.y);
  static_assert(Layout::PERFORMANCE_WAVEFORM.y + Layout::PERFORMANCE_WAVEFORM.h <=
                Layout::PERFORMANCE_HUD.y + Layout::PERFORMANCE_HUD.h);

  return 0;
}
