#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
files = {
    "hub": ROOT / "src/ui/pages/sequencer_hub_page.cpp",
    "pattern": ROOT / "src/ui/pages/pattern_edit_page_legacy.h",
    "pattern_modern": ROOT / "src/ui/pages/pattern_edit_page.cpp",
    "display": ROOT / "src/ui/miniacid_display.cpp",
    "song": ROOT / "src/ui/pages/song_page.cpp",
    "drum": ROOT / "src/ui/pages/drum_sequencer_page_legacy.h",
}
text = {name: path.read_text(encoding="utf-8") for name, path in files.items()}
failures = []


def require(condition, witness, detail):
    if condition:
        print(f"M-WORKING MW-L UI GREEN [{witness}]: {detail}")
    else:
        print(f"M-WORKING MW-L UI RED [{witness}]: {detail}", file=sys.stderr)
        failures.append(witness)


def between(source, start, end):
    a = source.find(start)
    if a < 0:
        return ""
    b = source.find(end, a + len(start))
    if b < 0:
        b = len(source)
    return source[a:b]

hub_quick = between(text["hub"], "bool SequencerHubPage::handleQuickKeys(UIEvent& e)",
                    "bool SequencerHubPage::handleAppEvent")
require(
    "tryManual303TargetSwitch" in hub_quick
    and "set303PatternIndex" not in hub_quick
    and "set303BankIndex" not in hub_quick,
    "MW-L-UI-A",
    "Sequencer Hub manual pattern/bank retarget uses the Working guard",
)

require(
    "tryManual303TargetSwitch" in text["pattern"]
    and "mini_acid_.set303PatternIndex" not in text["pattern"]
    and "mini_acid_.set303BankIndex" not in text["pattern"],
    "MW-L-UI-B",
    "legacy Pattern editor cannot bypass the manual target guard",
)

require(
    "tryManual303TargetSwitch" in text["pattern_modern"]
    and "mini_acid_.set303PatternIndex" not in text["pattern_modern"],
    "MW-L-UI-C",
    "modern Pattern wrapper manual retarget cannot bypass the Working guard",
)

for name in ("display", "song", "pattern", "drum"):
    require(
        "requestPageSwitch(" not in text[name]
        and "tryManualPageSwitch(" in text[name],
        f"MW-L-PAGE-{name.upper()}",
        f"{name} manual page navigation uses the global Working guard",
    )

engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
require(
    "void MiniAcid::requestPageSwitch(int pageIndex)" in engine,
    "MW-L-LOWLEVEL-PAGE",
    "low-level page switch remains available to Song/runtime authority",
)
require(
    "void MiniAcid::set303PatternIndex" in engine
    and "void MiniAcid::set303BankIndex" in engine,
    "MW-L-LOWLEVEL-303",
    "low-level 303 setters remain available below the manual guard",
)

if failures:
    print("M-WORKING MW-L UI SUMMARY: RED witnesses=" + ",".join(failures), file=sys.stderr)
    raise SystemExit(1)

print("M-WORKING MW-L UI SUMMARY: PASS")
