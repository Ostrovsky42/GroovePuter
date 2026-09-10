#!/usr/bin/env python3
from pathlib import Path

SRC = Path("src/ui/pages/synth_sequencer_page.cpp")
text = SRC.read_text(encoding="utf-8")

start = text.index("void SynthSequencerPage::drawTabIndicator")
end = text.index("void SynthSequencerPage::drawPhraseNotes", start)
body = text[start:end]

for label in ('"[N]KM"', '"N[K]M"', '"NK[M]"'):
    assert label in body, f"missing synth tab indicator label {label}"

assert "UI::currentStyle" not in body, (
    "active synth tab observability must not depend on theme/style"
)
assert "VisualStyle::" not in body, (
    "drawTabIndicator must not suppress mandatory information per theme"
)

phrase_draw = text.index("drawPhraseNotes(gfx);")
phrase_indicator = text.index("drawTabIndicator(gfx);", phrase_draw)
normal_draw = text.index("MultiPage::draw(gfx);")
normal_indicator = text.index("drawTabIndicator(gfx);", normal_draw)
assert phrase_indicator > phrase_draw
assert normal_indicator > normal_draw

print("PASS: synth tab information is theme-independent and rendered for both NOTES paths")
