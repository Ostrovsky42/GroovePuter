#pragma once

namespace PatternAddressUiContext {

inline int& bankIndexStorage() {
  static int value = 0;
  return value;
}

inline void setBankIndex(int bankIndex) {
  if (bankIndex < 0) bankIndex = 0;
  if (bankIndex > 1) bankIndex = 1;
  bankIndexStorage() = bankIndex;
}

inline int bankIndex() {
  return bankIndexStorage();
}

}  // namespace PatternAddressUiContext
