#!/usr/bin/env python3
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
    require("wordDigitAlreadyDispatched" in sketch and
            "digitDispatchMask" in sketch and
            "if (u >= '0' && u <= '9') continue;" not in sketch,
            "word-only Cardputer digits must dispatch while HID duplicates stay suppressed")
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
