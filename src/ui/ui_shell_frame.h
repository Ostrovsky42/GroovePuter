#pragma once
#ifndef GROOVEPUTER_SRC_UI_UI_SHELL_FRAME_H
#define GROOVEPUTER_SRC_UI_UI_SHELL_FRAME_H

#include <cstddef>

namespace UI {

struct UiFooterModel {
  static constexpr std::size_t kTextBytes = 64;

  char left[64]{};
  char right[64]{};
  bool valid = false;

  void clear() {
    left[0] = '\0';
    right[0] = '\0';
    valid = false;
  }

  void set(const char* leftText, const char* rightText) {
    copyText(left, leftText);
    copyText(right, rightText);
    valid = true;
  }

 private:
  static void copyText(char (&dst)[kTextBytes], const char* src) {
    if (src == nullptr) {
      dst[0] = '\0';
      return;
    }
    std::size_t i = 0;
    for (; i + 1 < kTextBytes && src[i] != '\0'; ++i) {
      dst[i] = src[i];
    }
    dst[i] = '\0';
  }
};

struct UiShellFrameModel {
  UiFooterModel footer{};
  // The performance HUD's feel chip reports the Pattern grid. On a screen that
  // is not editing a Pattern it is not merely noise, it is wrong, so a page can
  // decline it for its own frame. Default stays on for every other page.
  bool feelOverlay = true;

  void clear() {
    footer.clear();
    feelOverlay = true;
  }
  void setFooter(const char* left, const char* right = nullptr) {
    footer.set(left, right);
  }
};

static_assert(sizeof(UiShellFrameModel) <= 136,
              "UI shell frame model must stay stack-bounded");

}  // namespace UI

#endif  // GROOVEPUTER_SRC_UI_UI_SHELL_FRAME_H
