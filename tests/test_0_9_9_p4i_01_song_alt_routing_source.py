#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

ino = (ROOT / "GroovePuter.ino").read_text()
edges = (ROOT / "src/input/cardputer_input_edges.h").read_text()
normalize = (ROOT / "src/ui/key_normalize.h").read_text()
display = (ROOT / "src/ui/miniacid_display.cpp").read_text()
song_header = (ROOT / "src/ui/pages/song_page.h").read_text()
song_picker = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text()
lease_owner = (ROOT / "src/phrase/pattern_lease_owner.h").read_text()

# Hardware scan still enters the one existing central UI dispatcher. P4I-01
# must not add a second scanner or route directly into SongPage.
assert "M5Cardputer.update();" in ino
assert "M5Cardputer.Keyboard.keysState()" in ino
assert "handled = g_miniDisplay ? g_miniDisplay->handleEvent(evt) : false;" in ino
assert "SongPage" not in edges
for forbidden_scanner in (".Keyboard", "keysState()", ".update()"):
    assert forbidden_scanner not in edges

# The existing raw word filter is intentionally unchanged. The closure happens
# before it by tunnelling only word-only Alt-letter/Enter representations.
for expected in (
    "if (u == '\\n' || u == '\\r' || u == '\\b' || u == '\\t') continue;",
    "if (ks.ctrl || ks.alt)",
    "if (isLetter || isCtrlChar) continue;",
):
    assert expected in ino

assert "const bool altOnly = current.alt && !current.ctrl;" in edges
assert "const bool altWordFallback = altOnly && (letterHid != 0 || enterWord);" in edges
assert "enterHidDown(current)" in edges
assert "containsHid(current, letterHid)" in edges
assert "current.alt && !previous.alt" in edges
assert "GROOVEPUTER_WORD_ENTER_SENTINEL" in edges
assert "stageWordAltLetterFallback(rawChar)" in edges

assert "GROOVEPUTER_WORD_ALT_LETTER_SENTINEL" in normalize
assert "GROOVEPUTER_WORD_ENTER_SENTINEL" in normalize
assert "GROOVEPUTER_WORD_TAB_SENTINEL" in normalize
assert "if (state.pending)" in normalize
assert "return '\\n';" in normalize
assert "return '\\t';" in normalize

# Alt+H is a global UI action before page dispatch. Therefore a canonical
# logical event cannot be consumed by SongPage before help is toggled.
display_handle_start = display.index("bool MiniAcidDisplay::handleEvent(UIEvent event)")
help_idx = display.index(
    "if (event.alt && (event.key == 'h' || event.key == 'H'))",
    display_handle_start,
)
page_idx = display.index(
    "IPage* currentPage = getPage_(page_index_);", display_handle_start
)
assert help_idx < page_idx
help_body = display[help_idx:page_idx]
assert "global_help_overlay_.setPageContext(page_index_)" in help_body
assert "global_help_overlay_.toggle()" in help_body
assert "return true;" in help_body

# Alt+Enter remains the existing Song-only Pattern Picker gesture and invokes
# the existing picker implementation. No new picker/allocator path is added.
assert "songPatternPickerOpenGesture" in song_picker
assert "event.alt && !event.ctrl" in song_picker
assert "event.key == '\\n' || event.key == '\\r'" in song_picker
handle_start = song_picker.index("bool SongPage::handleEvent(UIEvent& ui_event)")
active_idx = song_picker.index("if (pattern_picker_.active)", handle_start)
open_idx = song_picker.index("if (songPatternPickerOpenGesture(ui_event))", handle_start)
app_idx = song_picker.index(
    "if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT)", handle_start
)
assert active_idx < open_idx < app_idx
open_branch = song_picker[open_idx:open_idx + 160]
assert "return openPatternPicker();" in open_branch
assert "bool SongPage::openPatternPicker()" in song_picker

# PatternLease ownership is exactly the pre-existing P4I owner. Input routing
# must neither allocate nor free pattern material.
assert "PhrasePatternLease::PatternLease" in song_header
assert "PhrasePatternLease::patternLeaseOwner()" in song_picker
assert "PatternLeaseOwner" in lease_owner
for forbidden in (
    "patternLeaseOwner",
    "PatternLeaseOwner",
    "preparePersistentTransfer",
    "completePersistentTransfer",
    "SongPatternMaterializer",
):
    assert forbidden not in edges
    assert forbidden not in normalize

# No closure-specific permanent debug channel remains in production headers.
assert "P4I-01" not in edges
assert "P4I-01" not in normalize
assert "Serial." not in edges

print("0.9.9-P4I-01 Song Alt routing source contracts passed")
