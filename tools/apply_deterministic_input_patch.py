#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"anchor mismatch in {path}: count={count} anchor={old[:120]!r}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8")


def replace_range(path: str, start_anchor: str, end_anchor: str, replacement: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    start = text.index(start_anchor)
    end = text.index(end_anchor, start)
    target.write_text(text[:start] + replacement + text[end:], encoding="utf-8")


replace_once(
    "GroovePuter.ino",
    '#include "src/input/performance_keyboard.h"\n',
    '#include "src/input/performance_keyboard.h"\n#include "src/input/cardputer_input_edges.h"\n',
)

replace_once(
    "GroovePuter.ino",
    '''  static constexpr unsigned long KEY_REPEAT_DELAY_MS = 350;
  static constexpr unsigned long KEY_REPEAT_INTERVAL_MS = 80;
  static Keyboard_Class::KeysState lastKeysState{};
  static bool hasLastKeys = false;
  static unsigned long nextRepeatAt = 0;

  auto handleWithFallback = [&](UIEvent evt) {
    Serial.printf("[DIAG] handleWithFallback: key=0x%02X (%c), scancode=%d, app_event=%d\\n",
      (uint8_t)evt.key, evt.key >= 32 && evt.key < 127 ? evt.key : '.', evt.scancode, evt.app_event_type);
''',
    '''  static constexpr unsigned long KEY_REPEAT_DELAY_MS = 350;
  static constexpr unsigned long KEY_REPEAT_INTERVAL_MS = 80;
  static Keyboard_Class::KeysState previousKeysState{};
  static bool hasPreviousKeysState = false;
  static UIEvent repeatEvent{};
  static uint8_t repeatHid = 0;
  static bool repeatActive = false;
  static uint32_t repeatPressId = 0;
  static uint32_t nextPressId = 1;
  static unsigned long nextRepeatAt = 0;

  auto handleWithFallback = [&](UIEvent evt,
                                const char* source,
                                uint32_t pressId,
                                bool repeat) {
    Serial.printf("[KEY] press=%u src=%s repeat=%d fn=%d alt=%d ctrl=%d shift=%d key=0x%02X sc=%d\\n",
      (unsigned)pressId, source, repeat ? 1 : 0,
      evt.meta ? 1 : 0, evt.alt ? 1 : 0, evt.ctrl ? 1 : 0,
      evt.shift ? 1 : 0, (uint8_t)evt.key, evt.scancode);
''',
)

new_input_block = r'''  auto processKeyEdges = [&](const Keyboard_Class::KeysState& ks,
                             const Keyboard_Class::KeysState& previous,
                             bool hadPrevious,
                             uint32_t pressId) -> bool {
    bool dispatched = false;
    bool sawEdge = false;
    bool armedRepeat = false;

    for (auto hid : ks.hid_keys) {
      if (!GroovePuterInput::shouldDispatchHid(
              ks, previous, hadPrevious, static_cast<uint8_t>(hid))) {
        continue;
      }
      sawEdge = true;

      UIEvent evt{};
      evt.alt = ks.alt;
      evt.ctrl = ks.ctrl;
      evt.shift = ks.shift;
      evt.meta = ks.fn;
      bool shouldSend = false;
      auto mapFKey = [&](uint8_t h, KeyScanCode& sc) -> bool {
        if (h >= 0x3A && h <= 0x41) {
            sc = static_cast<KeyScanCode>(GROOVEPUTER_F1 + (h - 0x3A));
            return true;
        }
        return false;
      };

      if (mapFKey(hid, evt.scancode)) {
        shouldSend = true;
      } else if (hid == 0x33) {
        evt.scancode = GROOVEPUTER_UP;
        shouldSend = true;
      } else if (hid == 0x37) {
        evt.scancode = GROOVEPUTER_DOWN;
        shouldSend = true;
      } else if (hid == 0x36) {
        evt.scancode = GROOVEPUTER_LEFT;
        shouldSend = true;
      } else if (hid == 0x38) {
        evt.scancode = GROOVEPUTER_RIGHT;
        shouldSend = true;
      } else if (hid == 0x28 || hid == 0x58) {
        evt.key = '\n';
        shouldSend = true;
      } else if (hid == KEY_BACKSPACE) {
        evt.key = '\b';
        shouldSend = true;
      } else if (hid == KEY_TAB || hid == 0x2B) {
        evt.key = '\t';
        evt.scancode = GROOVEPUTER_TAB;
        shouldSend = true;
      } else if (hid >= 0x1E && hid <= 0x27) {
        evt.key = hid == 0x27 ? '0' : static_cast<char>('1' + (hid - 0x1E));
        shouldSend = true;
      } else if (applyCtrlLetter(ks, hid, evt)) {
        mapHidLetterScancode(hid, evt.scancode);
        shouldSend = true;
      } else if (applyAltLetter(ks, hid, evt)) {
        mapHidLetterScancode(hid, evt.scancode);
        shouldSend = true;
      }

      if (!shouldSend) continue;
      handleWithFallback(evt, "HID", pressId, false);
      dispatched = true;

      if (GroovePuterInput::mayRepeat(evt) &&
          ks.hid_keys.size() == 1 && ks.word.empty()) {
        repeatEvent = evt;
        repeatHid = static_cast<uint8_t>(hid);
        repeatPressId = pressId;
        nextRepeatAt = millis() + KEY_REPEAT_DELAY_MS;
        armedRepeat = true;
      }
    }

    const bool suppressWordAfterModifierRelease =
        hadPrevious && GroovePuterInput::modifierReleased(ks, previous);
    if (!suppressWordAfterModifierRelease) {
      for (auto inputChar : ks.word) {
        if (!GroovePuterInput::shouldDispatchWord(
                ks, previous, hadPrevious, inputChar)) {
          continue;
        }
        sawEdge = true;

        if (inputChar != 0) {
          const unsigned char u = static_cast<unsigned char>(inputChar);
          if (u >= '0' && u <= '9') continue;
          if (u == '\n' || u == '\r' || u == '\b' || u == '\t') continue;

          if (ks.ctrl || ks.alt) {
            const bool isLetter =
                (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
            const bool isCtrlChar = u >= 1 && u <= 26;
            if (isLetter || isCtrlChar) continue;
          }
        }

        UIEvent evt{};
        evt.alt = ks.alt;
        evt.ctrl = ks.ctrl;
        evt.shift = ks.shift;
        evt.meta = ks.fn;
        evt.key = normalizeKeyChar(inputChar);
        evt.scancode = mapAsciiLetterScancode(evt.key);
        if (evt.key == '`' || evt.key == '~') {
          evt.scancode = GROOVEPUTER_ESCAPE;
        }
        handleWithFallback(evt, "WORD", pressId, false);
        dispatched = true;
      }
    }

    if (sawEdge) {
      repeatActive = armedRepeat;
      if (!repeatActive) {
        repeatHid = 0;
        repeatPressId = 0;
      }
    }
    return dispatched;
  };

  const Keyboard_Class::KeysState currentKeysState =
      M5Cardputer.Keyboard.keysState();
  reconcilePerformanceKeys(currentKeysState);

  const uint32_t candidatePressId = nextPressId;
  const bool dispatched = processKeyEdges(
      currentKeysState,
      previousKeysState,
      hasPreviousKeysState,
      candidatePressId);
  if (dispatched) {
    ++nextPressId;
    if (g_miniDisplay) g_miniDisplay->dismissSplash();
  }

  if (repeatActive &&
      !GroovePuterInput::repeatKeyStillHeld(
          currentKeysState, repeatHid, repeatEvent)) {
    Serial.printf("[KEY] press=%u src=REPEAT blocked=1 hid=0x%02X\n",
                  (unsigned)repeatPressId, (unsigned)repeatHid);
    repeatActive = false;
  }

  const unsigned long nowMs = millis();
  if (repeatActive &&
      static_cast<int32_t>(nowMs - nextRepeatAt) >= 0) {
    handleWithFallback(repeatEvent, "REPEAT", repeatPressId, true);
    nextRepeatAt = nowMs + KEY_REPEAT_INTERVAL_MS;
  }

  previousKeysState = currentKeysState;
  hasPreviousKeysState = true;

'''

replace_range(
    "GroovePuter.ino",
    "  auto processKeys = [&](const Keyboard_Class::KeysState& ks) {\n",
    "  static unsigned long lastUIUpdate = 0;\n",
    new_input_block,
)

helper = r'''#pragma once

#include <cstdint>

#include "../ui/ui_core.h"

namespace GroovePuterInput {

template <typename KeysState>
inline bool sameModifiers(const KeysState& a, const KeysState& b) {
  return a.alt == b.alt &&
         a.ctrl == b.ctrl &&
         a.shift == b.shift &&
         a.fn == b.fn;
}

template <typename KeysState>
inline bool modifierActivated(const KeysState& current,
                              const KeysState& previous) {
  return (current.alt && !previous.alt) ||
         (current.ctrl && !previous.ctrl) ||
         (current.shift && !previous.shift) ||
         (current.fn && !previous.fn);
}

template <typename KeysState>
inline bool modifierReleased(const KeysState& current,
                             const KeysState& previous) {
  return (!current.alt && previous.alt) ||
         (!current.ctrl && previous.ctrl) ||
         (!current.shift && previous.shift) ||
         (!current.fn && previous.fn);
}

template <typename KeysState>
inline bool containsHid(const KeysState& state, uint8_t hid) {
  for (const auto value : state.hid_keys) {
    if (static_cast<uint8_t>(value) == hid) return true;
  }
  return false;
}

template <typename KeysState, typename WordChar>
inline bool containsWord(const KeysState& state, WordChar value) {
  for (const auto current : state.word) {
    if (current == value) return true;
  }
  return false;
}

template <typename KeysState>
inline bool shouldDispatchHid(const KeysState& current,
                              const KeysState& previous,
                              bool hadPrevious,
                              uint8_t hid) {
  if (!hadPrevious) return true;
  if (!containsHid(previous, hid)) return true;
  return modifierActivated(current, previous);
}

template <typename KeysState, typename WordChar>
inline bool shouldDispatchWord(const KeysState& current,
                               const KeysState& previous,
                               bool hadPrevious,
                               WordChar value) {
  (void)current;
  return !hadPrevious || !containsWord(previous, value);
}

inline bool mayRepeat(const UIEvent& event) {
  if (event.alt || event.ctrl || event.shift || event.meta) return false;
  return event.scancode == GROOVEPUTER_UP ||
         event.scancode == GROOVEPUTER_DOWN ||
         event.scancode == GROOVEPUTER_LEFT ||
         event.scancode == GROOVEPUTER_RIGHT;
}

template <typename KeysState>
inline bool repeatKeyStillHeld(const KeysState& state,
                               uint8_t hid,
                               const UIEvent& event) {
  if (state.hid_keys.size() != 1) return false;
  if (!containsHid(state, hid)) return false;
  return state.alt == event.alt &&
         state.ctrl == event.ctrl &&
         state.shift == event.shift &&
         state.fn == event.meta;
}

}  // namespace GroovePuterInput
'''
(ROOT / "src/input/cardputer_input_edges.h").write_text(helper, encoding="utf-8")

host_test = r'''#include <cassert>
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
  assert(shouldDispatchWord(wordA, empty, true, 'a'));
  assert(!shouldDispatchWord(wordA, wordA, true, 'a'));

  UIEvent arrow{};
  arrow.scancode = GROOVEPUTER_RIGHT;
  assert(mayRepeat(arrow));
  assert(repeatKeyStillHeld(fnRight, 0x30, UIEvent{0, GROOVEPUTER_NO_SCANCODE, false, false, false, true} ) == false);

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
'''
(ROOT / "tests/test_cardputer_input_edges.cpp").write_text(host_test, encoding="utf-8")

source_test = r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    helper = (ROOT / "src/input/cardputer_input_edges.h").read_text(encoding="utf-8")

    require("Keyboard.isChange()" not in sketch,
            "Cardputer input must not depend on key-count-only isChange()")
    require("Keyboard.isPressed()" not in sketch,
            "input edges must be derived from complete snapshots")
    require("processKeys(lastKeysState)" not in sketch,
            "repeat must never replay a complete stale KeysState")
    require("processKeyEdges" in sketch and "previousKeysState" in sketch,
            "sketch must compare current and previous physical key snapshots")
    require('handleWithFallback(repeatEvent, "REPEAT"' in sketch,
            "repeat must dispatch one stored eligible event")
    require("shouldDispatchHid" in sketch and "shouldDispatchWord" in sketch,
            "HID and word paths need explicit edge filtering")
    require("[KEY] press=%u src=%s" in sketch,
            "runtime diagnostics must expose press ID and source")
    require("event.alt || event.ctrl || event.shift || event.meta" in helper,
            "modified shortcuts must be excluded from repeat")
    for arrow in ("GROOVEPUTER_UP", "GROOVEPUTER_DOWN",
                  "GROOVEPUTER_LEFT", "GROOVEPUTER_RIGHT"):
        require(arrow in helper, f"repeat whitelist missing {arrow}")
    require("modifierActivated" in helper and "modifierReleased" in helper,
            "modifier edges must be detected independently of key count")
    print("deterministic Cardputer input source regressions: OK")


if __name__ == "__main__":
    main()
'''
(ROOT / "tests/test_cardputer_input_source_regressions.py").write_text(source_test, encoding="utf-8")

run_tests = ROOT / "tests/run_host_tests.sh"
run_text = run_tests.read_text(encoding="utf-8")
run_text = run_text.replace(
    'python3 "${ROOT_DIR}/tests/test_source_regressions.py"\n',
    'python3 "${ROOT_DIR}/tests/test_source_regressions.py"\n'
    'python3 "${ROOT_DIR}/tests/test_cardputer_input_source_regressions.py"\n',
    1,
)
run_text += r'''

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_cardputer_input_edges.cpp" \
  -o "${BUILD_DIR}/test_cardputer_input_edges"

"${BUILD_DIR}/test_cardputer_input_edges"
'''
run_tests.write_text(run_text, encoding="utf-8")

doc = r'''# Deterministic Cardputer Input — Hardware Stage

## Purpose

Make one physical Cardputer key press produce at most one UI navigation event.
This stage fixes input edge detection and repeat policy only. It does not add
NVS persistence, project autosave, new workflow pages, MIDI routing or DSP
changes.

## Hardware list

- M5Stack Cardputer ADV
- USB-C data/power cable
- optional serial monitor at the repository's configured baud rate

## Wiring

No external wiring is required.

PORT.A remains unused by this test. If existing I2C hardware stays connected,
preserve the Cardputer ADV bus on GPIO2 SDA / GPIO1 SCL and keep shared devices
on `Wire`.

## Build / Flash

```bash
git switch agent/deterministic-cardputer-input
git pull
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

The pinned dependency install remains:

```bash
bash scripts/install_arduino_deps.sh
```

This is important because the diagnosis assumes the repository-pinned
M5Cardputer version rather than an arbitrary local Arduino library.

## Expected behavior

- the application polls the complete `KeysState` every loop;
- changing from one same-size key combination to another is detected;
- newly pressed HID/word entries are emitted once;
- Fn, Alt and Ctrl combinations never auto-repeat;
- Tab, Enter and brackets never auto-repeat;
- only one unmodified arrow may auto-repeat;
- performance-note releases continue to follow the live HID set;
- logs identify `HID`, `WORD` and `REPEAT` sources with one press ID.

Example:

```text
[KEY] press=41 src=HID repeat=0 fn=1 alt=0 ctrl=0 shift=0 key=0x09 sc=...
```

Holding Fn+Tab for one second must not produce a `src=REPEAT` line.

## Troubleshooting

### A held Fn shortcut still repeats

Confirm `src/input/cardputer_input_edges.h` rejects any event with `meta`,
`alt`, `ctrl` or `shift` and that the flashed build is from this branch.

### A same-size key replacement is ignored

Confirm the sketch no longer calls `Keyboard.isChange()` and that Serial logs a
new press ID when replacing one held key with another.

### Notes remain stuck

Check that `reconcilePerformanceKeys(currentKeysState)` runs every loop, not only
when an edge is emitted.

### Local build uses another M5Cardputer version

Run `scripts/install_arduino_deps.sh`, then rebuild with verbose Arduino output
and confirm the selected library path.

## Acceptance checklist

```text
BUILD
[ ] host source regression passes
[ ] host edge/repeat unit test passes
[ ] Cardputer ADV build passes with --warnings all

ONE PRESS / ONE EVENT
[ ] tap Fn+Tab 20 times: exactly 20 workflow transitions
[ ] hold Fn+Tab for 1 second: exactly one transition
[ ] hold Fn+[ for 1 second: exactly one transition
[ ] hold Fn+] for 1 second: exactly one transition
[ ] hold Fn+M for 1 second: launcher changes state once
[ ] hold Enter for 1 second: confirmation occurs once

EDGE DETECTION
[ ] switch Fn+Tab directly to Fn+] without a long release: new key is detected
[ ] switch Fn+] directly to Fn+[: new key is detected
[ ] modifier release does not resend the held ordinary key

ALLOWED REPEAT
[ ] unmodified UP repeats after the delay
[ ] unmodified DOWN repeats after the delay
[ ] adding Fn/Alt/Ctrl/Shift stops arrow repeat immediately

REALTIME
[ ] no stuck performance notes
[ ] no new audio underruns
[ ] no watchdog reset
```
'''
(ROOT / "docs/stages/DETERMINISTIC_CARDPUTER_INPUT_STAGE.md").write_text(doc, encoding="utf-8")
