#!/usr/bin/env python3
"""Source-level gates for the Phrase Core UI follow-up.

These checks intentionally cover UI/workspace ownership. Phrase domain, Scene
persistence and revision behavior remain covered by the compiled foundation tests.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/ui/pages/phrase_page.h").read_text(encoding="utf-8")
WORKSPACE = (ROOT / "src/phrase/phrase_workspace.h").read_text(encoding="utf-8")
INSERT = (ROOT / "src/phrase/phrase_song_insert.h").read_text(encoding="utf-8")
DISPLAY = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
CARDPUTER = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
HELP = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
DOC = (ROOT / "docs/stages/PHRASE_CORE_INSERT_WORKFLOW.md").read_text(encoding="utf-8")
GEN_DOC = (ROOT / "docs/tests/GENERATED_PHRASE_TO_SONG.md").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# Fixed-capacity preview cache: no vector/heap or raw event ownership in UI.
require(HEADER, "std::array<PhraseCore::BarPreview, PhraseCore::kMaxBars>",
        "Phrase page must cache the bounded bar previews in fixed storage")
require(HEADER, "std::array<bool, PhraseCore::kMaxBars>",
        "Phrase page must track fixed-capacity preview validity")
if "std::vector" in HEADER or "new " in CPP or "malloc(" in CPP:
    raise AssertionError("Phrase UI must not allocate preview/event storage")

# The page renders the complete Phrase shape and a selected per-bar preview.
require(CPP, "drawPhraseShape", "Phrase UI must render energy-by-bar")
require(CPP, "PhraseCore::kMaxBars", "Phrase shape must remain bounded to 8 bars")
require(CPP, "bar_preview_valid_[preview_bar_]",
        "Left/right bar navigation must use the cached selected preview")
require(CPP, '"SA"', "Synth A preview label is missing")
require(CPP, '"SB"', "Synth B preview label is missing")
require(CPP, '"DR"', "Drums preview label is missing")

# Honest reference semantics and separate next-capture controls.
require(CPP, '"REF MUT"', "Mutable reference storage badge is missing")
require(CPP, '"REF LINKED"', "Linked reference warning is missing")
require(CPP, '"NEXT %uB %s  P:%s"',
        "Next capture settings must be visibly separate from saved metadata")
if any(term in CPP for term in ('"COPIED"', '"RECORDED"', '"EXTRACTED"')):
    raise AssertionError("Phrase UI must not claim independent event ownership")

# Theme-aware rendering for all selectable visual styles.
require(CPP, "paletteForStyle", "Phrase UI must use a semantic palette")
require(CPP, "VisualStyle::RETRO_CLASSIC", "CYBER palette is missing")
require(CPP, "VisualStyle::AMBER", "AMBER palette is missing")
require(CPP, "VisualStyle::MINIMAL", "CARBON palette is missing")

# MiniAcidDisplay already paints the full skin before page draw. A second page
# clear is redundant and increases SPI work.
if "LayoutManager::clearContent" in CPP:
    raise AssertionError("Phrase page must not clear the full content twice")

# Existing command contract remains present, with one visible destination owner.
for needle in (
    "PhraseWorkspace::capture",
    "PhraseWorkspace::derive",
    "PhraseWorkspace::writeToSong",
    "PhraseWorkspace::clear",
    "case 'r': cycleRole",
    "case 'p': cycleParent",
    "case 'd': return deriveFromParent();",
    "case 'w': return writeToCurrentRow(ui_event.alt);",
    "return clearCurrentSlot();",
    '"1-4:SLOT  L/R:BAR  U/D:LEN"',
    '"G:GEN C+LR:TO C+UD:8 ENT/D/W"',
):
    require(CPP, needle, f"Phrase UI command/legend regression: {needle}")

# Generated Phrase recovery uses plain G, current 1/2/4/8B length and the same
# explicit TO destination that W/Alt+W already expose. It stays STOP-only.
require(HEADER, "bool generatePhraseToSong();",
        "Phrase page must expose the generated Phrase action")
require(CPP, '#include "src/dsp/generated_phrase_song.h"',
        "Phrase page must use the current generated-Phrase adapter")
require(CPP, "bool PhrasePage::generatePhraseToSong()",
        "generated Phrase action implementation is missing")
require(CPP, "const int songStart = static_cast<int>(destination_row_);",
        "Phrase G must start at the visible TO destination")
require(CPP, "GeneratedPhraseSong::generate(",
        "Phrase G must route through the current generated-Phrase adapter")
require(CPP, "mini_acid_.isPlaying()",
        "multi-row Phrase generation must guard moving Song ownership")
require(CPP, '"STOP PLAYBACK FOR PHRASE"',
        "PLAY rejection must be visible to the user")
require(CPP, "!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lower == 'g'",
        "only plain G may own generated Phrase materialization")
require(CPP, "static_cast<int>(songStart) + result.bars",
        "successful generated Phrase must advance TO by generated length")
if "mini_acid_.stop()" in CPP or "mini_acid_.start()" in CPP:
    raise AssertionError("Phrase G must not hide stop/generate/restart transport ownership")

# Destination must be explicit and independent of hidden engine playhead state.
require(HEADER, "uint8_t destination_row_ = 0;",
        "Phrase page must own an explicit bounded Song destination")
require(CPP, "void PhrasePage::cycleDestinationRow(int delta)",
        "Phrase destination row must be keyboard-adjustable")
require(CPP, "request.startRow = destination_row_;",
        "saved Phrase writes must use the visible destination row")
require(CPP, "ui_event.ctrl && !ui_event.alt && !ui_event.meta",
        "Phrase destination navigation must be modifier-gated")
require(CPP, "if (nav == GROOVEPUTER_LEFT)",
        "Ctrl+Left must move Phrase destination backward")
require(CPP, "cycleDestinationRow(-1)",
        "Ctrl+Left must move Phrase destination by one row")
require(CPP, "if (nav == GROOVEPUTER_RIGHT)",
        "Ctrl+Right must move Phrase destination forward")
require(CPP, "cycleDestinationRow(1)",
        "Ctrl+Right must move Phrase destination by one row")
require(CPP, "if (nav == GROOVEPUTER_UP)",
        "Ctrl+Up must coarse-move Phrase destination backward")
require(CPP, "cycleDestinationRow(-8)",
        "Ctrl+Up must move Phrase destination by eight rows")
require(CPP, "if (nav == GROOVEPUTER_DOWN)",
        "Ctrl+Down must coarse-move Phrase destination forward")
require(CPP, "cycleDestinationRow(8)",
        "Ctrl+Down must move Phrase destination by eight rows")

# Cardputer's physical punctuation positions are canonical arrow HID keys. Do
# not regress to raw comma/period handlers: those characters are not a stable
# independent control surface on hardware.
require(CARDPUTER, "hid == 0x36", "Cardputer comma-position HID mapping changed")
require(CARDPUTER, "evt.scancode = GROOVEPUTER_LEFT",
        "Cardputer comma-position key must remain canonical LEFT")
require(CARDPUTER, "hid == 0x37", "Cardputer period-position HID mapping changed")
require(CARDPUTER, "evt.scancode = GROOVEPUTER_DOWN",
        "Cardputer period-position key must remain canonical DOWN")
if "key == ',' || key == '<'" in CPP or "key == '.' || key == '>'" in CPP:
    raise AssertionError("Phrase destination must not depend on raw comma/period chars")

require(CPP, "static_cast<int>(request.startRow) + static_cast<int>(request.lengthBars)",
        "successful capture must seed destination immediately after the captured region")
require(CPP, '"ID:%u P:%u %s  TO:%c%d"',
        "saved Phrase view must show the explicit Song destination")
require(CPP, '"FROM:%c%d DERIVE:%s  TO:%c%d"',
        "empty Phrase view must show capture source and insertion destination")
require(CPP, '"INSERTED"',
        "normal W success toast must identify insertion")
require(CPP, '"REPLACED"',
        "Alt+W success toast must identify replacement")

# Normal W means true insertion. Alt+W keeps explicit replacement semantics.
require(CPP, "writeToCurrentRow(ui_event.alt)",
        "W/Alt+W must keep one explicit UI dispatch point")
require(WORKSPACE, "if (request.overwrite)",
        "workspace must separate REPLACE from normal INSERT")
require(WORKSPACE, "PhraseCore::insertIntoSong(",
        "normal W must route to the insertion primitive")
require(WORKSPACE, "PhraseCore::writeToSong(",
        "Alt+W must retain the bounded replacement primitive")
require(INSERT, "destination.positions[row + phraseBars] = destination.positions[row]",
        "insert must shift complete SongPosition rows")
require(INSERT, "logicalSongLengthForInsert",
        "insert must handle the one-row empty Song placeholder")
require(INSERT, "const bool shiftsRows = insertRow < logicalLength",
        "insert must distinguish in-Song shift from sparse destination materialization")
require(INSERT, "for (int row = logicalLength; row < insertRow; ++row)",
        "sparse destination must clear the explicit gap")
require(INSERT, "finalLength > Song::kMaxPositions",
        "insert must preflight the true 128-row capacity boundary")

# Global Alt+W remains the waveform shortcut everywhere except PHRASE, where
# the page must receive it as explicit REPLACE.
require(DISPLAY, "page_index_ != WorkflowPages::kPhrase",
        "global Alt+W must exclude PHRASE so REPLACE is reachable")
waveform_start = DISPLAY.index("if (event.alt && (event.key == 'w' || event.key == 'W')")
waveform_block = DISPLAY[waveform_start:waveform_start + 260]
require(waveform_block, "page_index_ != WorkflowPages::kPhrase",
        "Phrase exclusion must belong to the global waveform Alt+W condition")
require(waveform_block, "UI::waveformOverlay.enabled = !UI::waveformOverlay.enabled",
        "non-Phrase Alt+W must still toggle the waveform overlay")

require(HELP, '"Ctrl+L/R    Move TO row +/-1"',
        "on-device Phrase help must expose fine destination movement")
require(HELP, '"Ctrl+U/D    Move TO row +/-8"',
        "on-device Phrase help must expose coarse destination movement")
require(HELP, '"W           INSERT before TO row"',
        "on-device Phrase help must explain W insertion")
require(HELP, '"Alt+W       REPLACE at TO row"',
        "on-device Phrase help must explain Alt+W replacement")
require(DOC, "`Ctrl+Left` / `Ctrl+Right`",
        "hardware doc must describe fine destination movement")
require(DOC, "`Ctrl+Up` / `Ctrl+Down`",
        "hardware doc must describe coarse destination movement")
require(GEN_DOC, "`G` generates a fresh connected phrase",
        "generated Phrase doc must define G against the visible TO destination")
require(GEN_DOC, "`W` inserts the selected saved Phrase",
        "generated Phrase doc must preserve W insertion semantics")
require(GEN_DOC, "`Alt+W` replaces saved-Phrase lanes",
        "generated Phrase doc must preserve Alt+W replacement semantics")

print("Phrase UI source regressions: PASS")
