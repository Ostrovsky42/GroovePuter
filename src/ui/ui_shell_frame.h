#pragma once

#include <cstddef>

namespace UI {

struct UiFooterModel {
  static constexpr std::size_t kTextBytes = 64;

  char left[kTextBytes]{};
  char right[kTextBytes]{};
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

  void clear() { footer.clear(); }
  void setFooter(const char* left, const char* right = nullptr) {
    footer.set(left, right);
  }
};

static_assert(sizeof(UiShellFrameModel) <= 136,
              "UI shell frame model must stay stack-bounded");

}  // namespace UI
