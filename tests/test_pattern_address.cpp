#include "../src/pattern/pattern_address.h"

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

  return 0;
}
