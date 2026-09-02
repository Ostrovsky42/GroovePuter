#include "../src/pattern/pattern_address.h"
#include "../src/ui/pattern_matrix_navigation.h"

#include <cassert>
#include <cstring>

int main() {
  char label[12];

  const PatternAddress first = patternAddressFromParts(0, 0, 0);
  assert(first.valid());
  assert(patternAddressToGlobal(first) == 0);
  formatPatternAddress(label, sizeof(label), first);
  assert(std::strcmp(label, "1A1") == 0);

  const PatternAddress address = patternAddressFromParts(2, 1, 6);
  assert(address.valid());
  const int global = patternAddressToGlobal(address);
  assert(global == songPatternFromPageBankIndex(2, 1, 6));

  const PatternAddress decoded = patternAddressFromGlobal(global);
  assert(decoded.page == 2);
  assert(decoded.bank == 1);
  assert(decoded.slot == 6);
  formatGlobalPatternAddress(label, sizeof(label), global);
  assert(std::strcmp(label, "3B7") == 0);

  formatGlobalPatternAddress(label, sizeof(label), -1);
  assert(std::strcmp(label, "---") == 0);
  formatPatternAddressParts(label, sizeof(label), kMaxPages, 0, 0);
  assert(std::strcmp(label, "---") == 0);

  assert(UI::songPatternPageShortcut('1', true, false, false) == 0);
  assert(UI::songPatternPageShortcut('8', true, false, false) == 7);
  assert(UI::songPatternPageShortcut('1', true, true, false) == 8);
  assert(UI::songPatternPageShortcut('8', true, true, false) == 15);
  assert(UI::songPatternPageShortcut('1', false, true, false) == -1);
  assert(UI::songPatternPageShortcut('1', true, true, true) == -1);
  assert(UI::songPatternPageShortcut('9', true, false, false) == -1);

  return 0;
}
