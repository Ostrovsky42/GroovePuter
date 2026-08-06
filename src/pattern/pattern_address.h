#pragma once

#include "../../scenes.h"

#include <cstddef>
#include <cstdio>

struct PatternAddress {
  int page = -1;
  int bank = -1;
  int slot = -1;  // Zero-based slot inside one bank.

  bool valid() const {
    return page >= 0 && page < kMaxPages &&
           bank >= 0 && bank < kBankCount &&
           slot >= 0 && slot < Bank<SynthPattern>::kPatterns;
  }
};

inline PatternAddress patternAddressFromParts(int page, int bank, int slot) {
  PatternAddress address{page, bank, slot};
  return address.valid() ? address : PatternAddress{};
}

inline PatternAddress patternAddressFromGlobal(int globalIndex) {
  if (globalIndex < 0 || globalIndex >= kMaxGlobalPatterns) {
    return PatternAddress{};
  }
  return PatternAddress{
      songPatternPage(globalIndex),
      songPatternBank(globalIndex),
      songPatternIndexInBank(globalIndex),
  };
}

inline int patternAddressToGlobal(const PatternAddress& address) {
  if (!address.valid()) return -1;
  return songPatternFromPageBankIndex(address.page, address.bank, address.slot);
}

inline void formatPatternAddress(char* out, std::size_t outSize,
                                 const PatternAddress& address) {
  if (!out || outSize == 0) return;
  if (!address.valid()) {
    std::snprintf(out, outSize, "---");
    return;
  }
  std::snprintf(out, outSize, "%d%c%d",
                address.page + 1,
                static_cast<char>('A' + address.bank),
                address.slot + 1);
}

inline void formatPatternAddressParts(char* out, std::size_t outSize,
                                      int page, int bank, int slot) {
  formatPatternAddress(out, outSize,
                       patternAddressFromParts(page, bank, slot));
}

inline void formatGlobalPatternAddress(char* out, std::size_t outSize,
                                       int globalIndex) {
  formatPatternAddress(out, outSize,
                       patternAddressFromGlobal(globalIndex));
}
