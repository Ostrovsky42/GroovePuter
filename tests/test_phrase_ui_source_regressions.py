#!/usr/bin/env python3
"""Source-level gates for Phrase Core and experimental Arranger UI.

These checks cover UI ownership only. Phrase domain, Scene persistence and
revision behavior remain covered by the focused backend tests.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/ui/pages/phrase_page.h").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# Fixed-capacity preview and arranger state: no vector/heap or event ownership.
require(HEADER, "std::array<PhraseCore::BarPreview, PhraseCore::kMaxBars>",
        "Phrase page must cache the bounded bar previews in fixed storage")
require(HEADER, "std::array<bool, PhraseCore::kMaxBars>",
        "Phrase page must track fixed-capacity preview validity")
require(HEADER, "enum class View : uint8_t",
        "Phrase page must expose bounded Core/Arrange views")
require(HEADER, "uint8_t arrangement_cursor_ = 0;",
        "Phrase arranger cursor must stay fixed-size")
if "std::vector" in HEADER or "new " in CPP or "malloc(" in CPP:
    raise AssertionError("Phrase UI must not allocate preview/arrangement storage")

# The Core view renders the complete Phrase shape and selected-bar preview.
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

# Stage 2 remains a bounded view over the persisted fixed arrangement.
for needle in (
    "void PhrasePage::drawArrangement",
    '"PHRASE ARRANGE"',
    '"CHAIN %u/%u  TOTAL %uB"',
    "PhraseCore::kArrangementCapacity",
    "PhraseWorkspace::assignArrangementStep",
    "PhraseWorkspace::removeArrangementStep",
    "PhraseWorkspace::clearArrangement",
    "PhraseWorkspace::writeArrangementToSong",
):
    require(CPP, needle, f"Phrase Arranger UI regression: {needle}")

# Theme-aware rendering for all selectable visual styles.
require(CPP, "paletteForStyle", "Phrase UI must use a semantic palette")
require(CPP, "VisualStyle::RETRO_CLASSIC", "CYBER palette is missing")
require(CPP, "VisualStyle::AMBER", "AMBER palette is missing")
require(CPP, "VisualStyle::MINIMAL", "CARBON palette is missing")

# MiniAcidDisplay already paints the full skin before page draw. A second page
# clear is redundant and increases SPI work.
if "LayoutManager::clearContent" in CPP:
    raise AssertionError("Phrase page must not clear the full content twice")

# Core and arranger commands/legends must match the actual Stage 2 page.
for needle in (
    "PhraseWorkspace::capture",
    "PhraseWorkspace::derive",
    "PhraseWorkspace::writeToSong",
    "PhraseWorkspace::clear",
    '"TAB:ARR 1-4:SLOT L/R:BAR U/D:LEN"',
    '"ENT:CAP D:DERIVE W:WRITE SH+W:OVER"',
    '"TAB:CORE L/R:POS U/D:+8 1-4:SET"',
    '"W:WRITE SH+W:OVER DEL:RM SH+DEL:CLEAR"',
    "if (key == '\\t')",
):
    require(CPP, needle, f"Phrase UI command/legend regression: {needle}")

print("Phrase UI source regressions: PASS")
