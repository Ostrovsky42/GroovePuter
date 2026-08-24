#include "src/input/cardputer_input_edges.h"

#include <cassert>
#include <cstdio>
#include <optional>
#include <vector>

namespace {

struct FakeKeysState {
  std::vector<uint8_t> hid_keys;
  std::vector<char> word;
  bool alt = false;
  bool ctrl = false;
  bool shift = false;
  bool fn = false;
};

bool rawWordFilterWouldDrop(const FakeKeysState& state, char value) {
  if (value == 0) return false;

  const unsigned char u = static_cast<unsigned char>(value);
  if (u == '\n' || u == '\r' || u == '\b' || u == '\t') return true;

  if (state.ctrl || state.alt) {
    const bool isLetter =
        (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
    const bool isCtrlChar = u >= 1 && u <= 26;
    if (isLetter || isCtrlChar) return true;
  }
  return false;
}

std::optional<UIEvent> wordEvent(const FakeKeysState& current,
                                 const FakeKeysState& previous,
                                 bool hadPrevious,
                                 char rawValue) {
  char value = rawValue;
  if (!GroovePuterInput::shouldDispatchWord(
          current, previous, hadPrevious, value)) {
    return std::nullopt;
  }
  if (rawWordFilterWouldDrop(current, value)) return std::nullopt;

  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.alt = current.alt;
  event.ctrl = current.ctrl;
  event.shift = current.shift;
  event.meta = current.fn;
  event.key = normalizeKeyChar(value);
  return event;
}

enum class RoutedAction {
  None,
  GlobalHelp,
  SongPatternPicker,
  SongLegacyHotkey,
  UnrelatedPageCaptured,
};

// Mirrors the already-existing production sink predicates. The source-contract
// test below pins these predicates to MiniAcidDisplay and SongPage so this host
// test can concentrate on the Cardputer raw->logical event boundary.
RoutedAction routeLogicalEvent(bool songPage, const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return RoutedAction::None;
  if (event.alt && (event.key == 'h' || event.key == 'H')) {
    return RoutedAction::GlobalHelp;
  }
  if (songPage && event.alt && !event.ctrl &&
      (event.key == '\n' || event.key == '\r')) {
    return RoutedAction::SongPatternPicker;
  }
  if (songPage && !event.alt && !event.ctrl && event.key == 'g') {
    return RoutedAction::SongLegacyHotkey;
  }
  if (!songPage && event.alt && !event.ctrl &&
      (event.key == '\n' || event.key == '\r')) {
    return RoutedAction::UnrelatedPageCaptured;
  }
  return RoutedAction::None;
}

void testWordOnlyAltHReachesGlobalHelp() {
  FakeKeysState previous{};
  FakeKeysState current{};
  current.alt = true;
  current.word = {'h'};

  const auto event = wordEvent(current, previous, true, 'h');
  assert(event.has_value());
  assert(event->alt);
  assert(!event->ctrl);
  assert(event->key == 'h');
  assert(routeLogicalEvent(true, *event) == RoutedAction::GlobalHelp);
}

void testWordOnlyAltEnterReachesSongPicker() {
  FakeKeysState previous{};
  FakeKeysState current{};
  current.alt = true;
  current.word = {'\n'};

  const auto event = wordEvent(current, previous, true, '\n');
  assert(event.has_value());
  assert(event->alt);
  assert(!event->ctrl);
  assert(event->key == '\n');
  assert(routeLogicalEvent(true, *event) == RoutedAction::SongPatternPicker);
}

void testModifierActivationRedispatchesHeldWordOnlyKey() {
  FakeKeysState previous{};
  previous.word = {'h'};

  FakeKeysState current = previous;
  current.alt = true;

  const auto event = wordEvent(current, previous, true, 'h');
  assert(event.has_value());
  assert(event->alt);
  assert(event->key == 'h');
  assert(routeLogicalEvent(true, *event) == RoutedAction::GlobalHelp);
}

void testHidMirrorRemainsCanonicalAndWordDuplicateIsSuppressed() {
  FakeKeysState previous{};

  FakeKeysState altH{};
  altH.alt = true;
  altH.hid_keys = {GroovePuterInput::asciiLetterHid('h')};
  altH.word = {'h'};
  assert(GroovePuterInput::shouldDispatchHid(
      altH, previous, true, GroovePuterInput::asciiLetterHid('h')));
  char h = 'h';
  assert(!GroovePuterInput::shouldDispatchWord(altH, previous, true, h));

  FakeKeysState altEnter{};
  altEnter.alt = true;
  altEnter.hid_keys = {GroovePuterInput::kCardputerEnterHid};
  altEnter.word = {'\n'};
  assert(GroovePuterInput::shouldDispatchHid(
      altEnter, previous, true, GroovePuterInput::kCardputerEnterHid));
  char enter = '\n';
  assert(!GroovePuterInput::shouldDispatchWord(
      altEnter, previous, true, enter));
}

void testPlainKeysDoNotBecomeAltActions() {
  FakeKeysState previous{};

  FakeKeysState plainH{};
  plainH.word = {'h'};
  const auto hEvent = wordEvent(plainH, previous, true, 'h');
  assert(hEvent.has_value());
  assert(routeLogicalEvent(true, *hEvent) == RoutedAction::None);

  // Preserve the existing raw adapter behavior: a word-only plain Enter is
  // still filtered rather than being promoted to the Alt-only fallback path.
  FakeKeysState plainEnter{};
  plainEnter.word = {'\n'};
  const auto enterEvent = wordEvent(plainEnter, previous, true, '\n');
  assert(!enterEvent.has_value());

  UIEvent injectedPlainEnter{};
  injectedPlainEnter.event_type = GROOVEPUTER_KEY_DOWN;
  injectedPlainEnter.key = '\n';
  assert(routeLogicalEvent(true, injectedPlainEnter) == RoutedAction::None);
}

void testUnrelatedPageHidAltEnterRemainsSingleCanonicalEvent() {
  FakeKeysState previous{};
  FakeKeysState current{};
  current.alt = true;
  current.hid_keys = {GroovePuterInput::kCardputerEnterHid};
  current.word = {'\n'};

  assert(GroovePuterInput::shouldDispatchHid(
      current, previous, true, GroovePuterInput::kCardputerEnterHid));

  UIEvent hidEvent{};
  hidEvent.event_type = GROOVEPUTER_KEY_DOWN;
  hidEvent.alt = true;
  hidEvent.key = '\n';
  assert(routeLogicalEvent(false, hidEvent) ==
         RoutedAction::UnrelatedPageCaptured);

  char wordCopy = '\n';
  assert(!GroovePuterInput::shouldDispatchWord(
      current, previous, true, wordCopy));
}

void testOtherSongHotkeyAndTabCompatibilityAreUnchanged() {
  FakeKeysState previous{};

  FakeKeysState songG{};
  songG.word = {'g'};
  const auto gEvent = wordEvent(songG, previous, true, 'g');
  assert(gEvent.has_value());
  assert(routeLogicalEvent(true, *gEvent) == RoutedAction::SongLegacyHotkey);

  FakeKeysState tab{};
  tab.word = {'\t'};
  char tabValue = '\t';
  assert(GroovePuterInput::shouldDispatchWord(
      tab, previous, true, tabValue));
  assert(tabValue == GROOVEPUTER_WORD_TAB_SENTINEL);
  assert(normalizeKeyChar(tabValue) == '\t');

  FakeKeysState mirroredTab = tab;
  mirroredTab.hid_keys = {GroovePuterInput::kCardputerTabHid};
  tabValue = '\t';
  assert(!GroovePuterInput::shouldDispatchWord(
      mirroredTab, previous, true, tabValue));
}

}  // namespace

int main() {
  testWordOnlyAltHReachesGlobalHelp();
  testWordOnlyAltEnterReachesSongPicker();
  testModifierActivationRedispatchesHeldWordOnlyKey();
  testHidMirrorRemainsCanonicalAndWordDuplicateIsSuppressed();
  testPlainKeysDoNotBecomeAltActions();
  testUnrelatedPageHidAltEnterRemainsSingleCanonicalEvent();
  testOtherSongHotkeyAndTabCompatibilityAreUnchanged();

  std::puts("0.9.9-P4I-01 Song Alt routing host tests passed");
  return 0;
}
