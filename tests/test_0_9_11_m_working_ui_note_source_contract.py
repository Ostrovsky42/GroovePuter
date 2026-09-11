#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
hub = (ROOT / "src/ui/pages/sequencer_hub_page.cpp").read_text(encoding="utf-8")
legacy = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")

failures = []


def require(condition, witness, detail):
    if condition:
        print(f"M-WORKING UI GREEN [{witness}]: {detail}")
    else:
        print(f"M-WORKING UI RED [{witness}]: {detail}", file=sys.stderr)
        failures.append(witness)


def between(text, start, end):
    begin = text.find(start)
    if begin < 0:
        return ""
    finish = text.find(end, begin + len(start))
    if finish < 0:
        finish = len(text)
    return text[begin:finish]

hub_grid = between(hub, "bool SequencerHubPage::handleGridEdit(UIEvent& e)", "\n}")
legacy_events = between(legacy, "bool PatternEditPage::handleEvent(UIEvent& ui_event)", "void PatternEditPage::tick()")

require(
    hub_grid.count("mini_acid_.adjustWorking303StepNote") >= 3
    and "mini_acid_.adjust303StepNote" not in hub_grid,
    "UI-NOTE-A",
    "Sequencer Hub semitone/add-note paths use transactional WORKING edit only",
)
require(
    legacy_events.count("mini_acid_.adjustWorking303StepNote") >= 4
    and "mini_acid_.adjust303StepNote" not in legacy_events,
    "UI-NOTE-B",
    "legacy Pattern editor semitone paths use transactional WORKING edit only",
)

hub_overview = between(hub, "inline bool hubTrackHitAt", "inline void drawHubScrollbar")
hub_te = between(hub, "void SequencerHubPage::drawTEGridStyle", "void SequencerHubPage::drawRetroClassicStyle")
hub_detail = between(hub, "void SequencerHubPage::drawDetail", "bool SequencerHubPage::handleEvent")
require(
    "currentWorking303Pattern" in hub_overview
    and "currentWorking303Pattern" in hub_te
    and "currentWorking303Pattern" in hub_detail,
    "UI-NOTE-C",
    "Sequencer Hub overview/detail render paths prefer exact-bound WORKING",
)

legacy_minimal = between(legacy, "void PatternEditPage::drawMinimalStyle", "void PatternEditPage::drawRetroClassicStyle")
legacy_retro = between(legacy, "void PatternEditPage::drawRetroClassicStyle", "void PatternEditPage::drawAmberStyle")
legacy_amber = between(legacy, "void PatternEditPage::drawAmberStyle", "void PatternEditPage::drawTEGridStyle")
require(
    "currentWorking303Pattern" in legacy_minimal,
    "UI-NOTE-D",
    "legacy MINIMAL renderer prefers exact-bound WORKING",
)
require(
    "currentWorking303Pattern" in legacy_retro
    and "currentWorking303Pattern" in legacy_amber,
    "UI-NOTE-E",
    "legacy RETRO/AMBER renderers prefer exact-bound WORKING",
)

if failures:
    print("M-WORKING UI SUMMARY: RED witnesses=" + ",".join(failures), file=sys.stderr)
    raise SystemExit(1)

print("M-WORKING UI SUMMARY: PASS")
