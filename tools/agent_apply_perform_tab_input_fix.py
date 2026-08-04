#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


sketch_path = ROOT / "GroovePuter.ino"
sketch = sketch_path.read_text(encoding="utf-8")

sketch = replace_once(
    sketch,
    "    bool armedRepeat = false;\n",
    "    bool armedRepeat = false;\n"
    "    bool dispatchedHidTab = false;\n",
    "HID Tab dedup state",
)

sketch = replace_once(
    sketch,
    "      handleWithFallback(evt, \"HID\", pressId, false);\n"
    "      dispatched = true;\n\n"
    "      if (GroovePuterInput::mayRepeat(evt) &&\n",
    "      handleWithFallback(evt, \"HID\", pressId, false);\n"
    "      dispatched = true;\n"
    "      if (evt.key == '\\t' || evt.scancode == GROOVEPUTER_TAB) {\n"
    "        dispatchedHidTab = true;\n"
    "      }\n\n"
    "      if (GroovePuterInput::mayRepeat(evt) &&\n",
    "record HID Tab dispatch",
)

sketch = replace_once(
    sketch,
    "          if (u == '\\n' || u == '\\r' || u == '\\b' || u == '\\t') continue;\n",
    "          if (u == '\\n' || u == '\\r' || u == '\\b') continue;\n"
    "          // M5Cardputer may expose the dedicated Tab key only through\n"
    "          // KeysState::word. Keep that path, but suppress it when the\n"
    "          // same physical press was already emitted as HID 0x2B.\n"
    "          if (u == '\\t' && dispatchedHidTab) continue;\n",
    "allow word-only Tab",
)

sketch = replace_once(
    sketch,
    "        evt.key = normalizeKeyChar(inputChar);\n"
    "        evt.scancode = mapAsciiLetterScancode(evt.key);\n"
    "        if (evt.key == '`' || evt.key == '~') {\n",
    "        evt.key = normalizeKeyChar(inputChar);\n"
    "        evt.scancode = mapAsciiLetterScancode(evt.key);\n"
    "        if (evt.key == '\\t') {\n"
    "          evt.scancode = GROOVEPUTER_TAB;\n"
    "        }\n"
    "        if (evt.key == '`' || evt.key == '~') {\n",
    "normalize word Tab scancode",
)

sketch_path.write_text(sketch, encoding="utf-8")

source_test_path = ROOT / "tests/test_cardputer_input_source_regressions.py"
source_test = source_test_path.read_text(encoding="utf-8")
source_test = replace_once(
    source_test,
    "    require(\"modifierActivated\" in helper and \"modifierReleased\" in helper,\n"
    "            \"modifier edges must be detected independently of key count\")\n"
    "    print(\"deterministic Cardputer input source regressions: OK\")\n",
    "    require(\"modifierActivated\" in helper and \"modifierReleased\" in helper,\n"
    "            \"modifier edges must be detected independently of key count\")\n"
    "    require(\"bool dispatchedHidTab = false;\" in sketch,\n"
    "            \"Tab input must deduplicate HID and word representations\")\n"
    "    require(\"if (u == '\\\\t' && dispatchedHidTab) continue;\" in sketch,\n"
    "            \"word-only Tab must not be discarded unconditionally\")\n"
    "    require(\"if (u == '\\\\n' || u == '\\\\r' || u == '\\\\b') continue;\" in sketch,\n"
    "            \"only non-Tab control words should remain suppressed\")\n"
    "    require(\"evt.scancode = GROOVEPUTER_TAB;\" in sketch,\n"
    "            \"word-only Tab must reach pages with the canonical scancode\")\n"
    "    perform = (ROOT / \"src/ui/pages/perform_page.cpp\").read_text(encoding=\"utf-8\")\n"
    "    require(\"event.key == '\\\\t' || event.scancode == GROOVEPUTER_TAB\" in perform,\n"
    "            \"PERFORM must accept normalized Tab from either input representation\")\n"
    "    print(\"deterministic Cardputer input source regressions: OK\")\n",
    "Tab source regressions",
)
source_test_path.write_text(source_test, encoding="utf-8")

doc_path = ROOT / "docs/stages/PERFORM_TAB_INPUT_FIX_STAGE.md"
doc_path.write_text(
    """# PERFORM Tab input fix

## Purpose

Make the dedicated Cardputer Tab key reliably open and close the local
`PERFORMANCE TOOLS` layer on the PERFORM page.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable for flashing and Serial Monitor

## Wiring

No external wiring is required. The built-in keyboard is used.

## Build and flash

```bash
./scripts/build_cardputer_adv.sh
```

Flash the generated Cardputer ADV firmware with the existing project workflow.

## Expected behavior

1. Open the PERFORM page.
2. Press the dedicated Tab key once.
3. `PERFORMANCE TOOLS` appears and the toast says `PERFORMANCE TOOLS: 1-8`.
4. Press Tab again; the layer closes.
5. `Fn+Tab` still changes workflow and does not toggle the local layer.
6. One physical Tab press toggles the layer exactly once, even when the keyboard
   library reports both HID and word representations.

Serial should show one dispatched event with `key=0x09`; the source may be `HID`
or `WORD` depending on the M5Cardputer library representation.

## Troubleshooting

- If Tab changes workflow, confirm Fn is not physically held or stuck.
- If no `[KEY]` line appears, verify the firmware was built from this PR head.
- If two toasts appear from one press, capture the `[KEY]` lines; HID/word
  deduplication has regressed.

## Acceptance checklist

- [ ] Plain Tab opens `PERFORMANCE TOOLS`.
- [ ] Plain Tab closes it on the next press.
- [ ] Exactly one toggle occurs per physical press.
- [ ] `Fn+Tab` continues to switch workflows.
- [ ] Number keys `1..8` operate tools while the layer is visible.
- [ ] Host, SDL, and Cardputer ADV CI jobs pass.
""",
    encoding="utf-8",
)
