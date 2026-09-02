#pragma once

namespace UI {

constexpr int songPatternPageShortcut(char key,
                                      bool ctrl,
                                      bool meta,
                                      bool alt) {
  if (!ctrl || alt || key < '1' || key > '8') return -1;
  return (key - '1') + (meta ? 8 : 0);
}

static_assert(songPatternPageShortcut('1', true, false, false) == 0,
              "Ctrl+1 must select pattern page 1");
static_assert(songPatternPageShortcut('8', true, false, false) == 7,
              "Ctrl+8 must select pattern page 8");
static_assert(songPatternPageShortcut('1', true, true, false) == 8,
              "Ctrl+Fn+1 must select pattern page 9");
static_assert(songPatternPageShortcut('8', true, true, false) == 15,
              "Ctrl+Fn+8 must select pattern page 16");

}  // namespace UI
