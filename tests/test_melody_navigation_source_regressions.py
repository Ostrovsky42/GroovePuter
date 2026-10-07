#!/usr/bin/env python3
"""Melody editor navigation (0.9.17): [ / ] browse saved Melodies, Ctrl+Left/
Right jump bars, plain arrows keep moving the cursor."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        print(f"FAIL: {message}")
        sys.exit(1)


def main():
    page = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
    keys = (ROOT / "src/ui/docs/keys.md").read_text(encoding="utf-8")

    brackets = page.index("ui_event.key == '[' || ui_event.key == ']'")
    require("return stepMelodySlot(" in page[brackets:brackets + 300],
            "[ / ] on MELODY must browse Melodies")
    require("PhraseInstrumentControls::jumpBar(" in
            page.split("bool SynthSequencerPage::jumpPhraseBar")[1].split("\nbool ")[0],
            "the bar jump lives in jumpPhraseBar")
    ctrl = page.index("return jumpPhraseBar(")
    require(page.rfind("ui_event.ctrl && !ui_event.alt && !ui_event.meta", 0, ctrl) > 0 and
            page.index("handleMelodySlotKey(ui_event)) return true;") > ctrl,
            "Ctrl+Left/Right must reach jumpPhraseBar before other MELODY keys")
    require("MelodySlotBrowse::neighbour(" in page and
            "return switchToMelodySlot(bank, pattern);" in page,
            "Q..I and [ / ] must share one slot switch path")
    require("`Ctrl+Left/Right` on MELODY" in keys and "`[` / `]` on MELODY" in keys,
            "keys.md documents the Melody navigation keys")
    print("melody navigation source regressions: OK")


if __name__ == "__main__":
    main()
