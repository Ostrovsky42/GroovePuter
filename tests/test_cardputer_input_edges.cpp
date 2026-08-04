#include <cassert>
#include <cstdint>
#include <vector>

#include "src/input/cardputer_input_edges.h"

namespace {
struct FakeKeysState {
  bool alt = false;
  bool ctrl = false;
  bool shift = false;
  bool fn = false;
  std::vector<uint8_t> hid_keys;
  std::vector<char> word;
};
}

int main() {
  using namespace GroovePuterInput;

  FakeKeysState empty{};
  FakeKeysState tab{};
  tab.hid_keys = {0x2B};
  assert(shouldDispatchHid(tab, empty, true, 0x2B));
  assert(!shouldDispatchHid(tab, tab, true, 0x2B));

  FakeKeysState fnTab = tab;
  fnTab.fn = true;
  assert(shouldDispatchHid(fnTab, tab, true, 0x2B));
  assert(!shouldDispatchHid(tab, fnTab, true, 0x2B));
  assert(modifierReleased(tab, fnTab));

  FakeKeysState fnRight{};
  fnRight.fn = true;
  fnRight.hid_keys = {0x30};
  assert(shouldDispatchHid(fnRight, fnTab, true, 0x30));

  FakeKeysState wordA{};
  wordA.hid_keys = {0x04};
  wordA.word = {'a'};
  char wordAValue = wordA.word.front();
  assert(shouldDispatchWord(wordA, empty, true, wordAValue));
  assert(wordAValue == 'a');
  wordAValue = wordA.word.front();
  assert(!shouldDispatchWord(wordA, wordA, true, wordAValue));

  FakeKeysState wordOnlyTab{};
  wordOnlyTab.word = {'\t'};
  char wordOnlyTabValue = wordOnlyTab.word.front();
  assert(shouldDispatchWord(wordOnlyTab, empty, true, wordOnlyTabValue));
  assert(wordOnlyTabValue == GROOVEPUTER_WORD_TAB_SENTINEL);
  assert(normalizeKeyChar(wordOnlyTabValue) == '\t');

  FakeKeysState duplicateTab{};
  duplicateTab.hid_keys = {kCardputerTabHid};
  duplicateTab.word = {'\t'};
  char duplicateTabValue = duplicateTab.word.front();
  assert(!shouldDispatchWord(duplicateTab, empty, true, duplicateTabValue));
  assert(duplicateTabValue == '\t');

  char heldWordTabValue = wordOnlyTab.word.front();
  assert(!shouldDispatchWord(wordOnlyTab, wordOnlyTab, true, heldWordTabValue));
  assert(heldWordTabValue == '\t');

  UIEvent arrow{};
  arrow.scancode = GROOVEPUTER_RIGHT;
  assert(mayRepeat(arrow));
  UIEvent fnEvent{};
  fnEvent.scancode = GROOVEPUTER_RIGHT;
  fnEvent.meta = true;
  assert(!mayRepeat(fnEvent));

  arrow.meta = true;
  assert(!mayRepeat(arrow));
  arrow.meta = false;
  arrow.key = '\t';
  arrow.scancode = GROOVEPUTER_TAB;
  assert(!mayRepeat(arrow));

  UIEvent enter{};
  enter.key = '\n';
  assert(!mayRepeat(enter));

  UIEvent bracket{};
  bracket.key = ']';
  assert(!mayRepeat(bracket));

  FakeKeysState rightOnly{};
  rightOnly.hid_keys = {0x38};
  UIEvent rightEvent{};
  rightEvent.scancode = GROOVEPUTER_RIGHT;
  assert(repeatKeyStillHeld(rightOnly, 0x38, rightEvent));
  rightOnly.fn = true;
  assert(!repeatKeyStillHeld(rightOnly, 0x38, rightEvent));
  return 0;
}
