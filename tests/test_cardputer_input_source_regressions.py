#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    helper = (ROOT / "src/input/cardputer_input_edges.h").read_text(encoding="utf-8")
    normalize = (ROOT / "src/ui/key_normalize.h").read_text(encoding="utf-8")
    perform = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
    smf_wrapper = (
        ROOT / "src/ui/pages/smf_player_page_structural.cpp"
    ).read_text(encoding="utf-8")

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

    require("kCardputerTabHid = 0x2B" in helper,
            "Cardputer Tab HID code must remain explicit")
    require("containsHid(current, kCardputerTabHid)" in helper,
            "word Tab must be suppressed when HID Tab is already present")
    require("GROOVEPUTER_WORD_TAB_SENTINEL" in helper,
            "word-only Tab must survive the raw control-character filter")
    require("GROOVEPUTER_WORD_TAB_SENTINEL = '\\x1F'" in normalize,
            "Tab sentinel value must remain stable")
    require("if (c == GROOVEPUTER_WORD_TAB_SENTINEL) return '\\t';" in normalize,
            "word-only Tab must normalize back to a real Tab event")
    require("event.key == '\\t' || event.scancode == GROOVEPUTER_TAB" in perform,
            "PERFORM must accept normalized Tab from either representation")

    require("dispatchedDigitMask" in sketch and
            "wordDigitAlreadyDispatched" in sketch,
            "Cardputer digit HID/word copies must be deduplicated")
    require("digitDispatchMask" in helper and
            "wordDigitAlreadyDispatched" in helper,
            "digit deduplication helpers must remain centralized")

    require("K is intentionally not a MIDI mute command" in smf_wrapper,
            "public MIDI Player wrapper must document the K no-op boundary")
    require(
        "if (!browserVisible_ && (event.key == 'k' || event.key == 'K')) return true;"
        in smf_wrapper,
        "MIDI Player must consume K before the legacy mute implementation",
    )

    print("deterministic Cardputer input source regressions: OK")


if __name__ == "__main__":
    main()
