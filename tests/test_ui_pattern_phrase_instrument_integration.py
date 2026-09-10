#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SYNTH = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text()
TOGGLE = (ROOT / "src/ui/phrase_source_toggle.h").read_text()


def require(needle: str, message: str) -> None:
    if needle not in SYNTH:
        raise AssertionError(message)


# Domain/runtime truth must still select the renderer. No UI-owned source flag.
require(
    "mini_acid_.currentSequencedSource(voice_index_)",
    "Synth page stopped projecting authoritative runtime source",
)

# PHRASE must identify edit object, playing source, bar extent and edit resolution
# at the same time. GRID must not survive only as a transient toast.
require('gfx.drawText(bounds.x + 4, bounds.y, "PHRASE")',
        "Phrase editor does not explicitly identify its edited object")
require("PLAY:PHR", "Phrase screen does not explicitly identify the playing source")
require("PhraseNotesCursor::gridLabel(phrase_cursor_.grid)",
        "Phrase screen does not project its edit GRID")
require("BAR %u/%u", "Phrase screen does not project current/total bars compactly")

# Length is a musical-domain command: UI requests one of 1/2/4/8 and then reads
# the resulting runtime buffer again. It must not assign lengthTicks directly.
require("PhraseInstrumentControls::applyLengthChange",
        "Phrase length gesture is not routed through the causal control adapter")
require("mini_acid_.setPhraseLength(voice_index_, bars)",
        "Phrase length gesture does not call the runtime/domain command")
length_handler = SYNTH.split("if (!ui_event.alt && lower == 'l')", 1)[1].split(
    "if (!ui_event.alt && (ui_event.key == '['", 1
)[0]
require_in_length = {
    "const auto apply =": "Phrase length gesture has no guarded mutation closure",
    "if (audio_guard_) audio_guard_(apply)":
        "Phrase length mutation bypasses the existing AudioGuard",
    "else apply()": "Phrase length mutation has no unguarded host-test fallback",
}
for needle, message in require_in_length.items():
    if needle not in length_handler:
        raise AssertionError(message)
if "currentPhraseBuffer(voice_index_).lengthTicks =" in SYNTH:
    raise AssertionError("UI directly owns Phrase lengthTicks")

# Bar navigation is UI continuity only. The handler moves the cursor through the
# adapter; it does not call any musical mutation for the gesture.
require("PhraseInstrumentControls::jumpBar",
        "Phrase bar navigation is not explicit")
require("PHRASE BAR", "bar navigation gives no immediate causal feedback")
require("lower == 'l'", "plain L is not owned by the Phrase editor")
require("ui_event.key == '[' || ui_event.key == ']'",
        "Phrase editor does not consume bracket bar navigation")
require("UI::showToast(toast, 900);\n    return true;",
        "Phrase bracket controls can fall through to workspace navigation")

# GRID changes editing resolution only and names itself as GRID, not STEP/LENGTH.
require('"GRID %s"', "GRID action is still presented as an ambiguous STEP control")

# MAKE PHRASE remains explicit to the musician, but conversion/switching must be
# decided by PhraseSourceToggle, never by SynthSequencerPage. The page consumes
# the owner's causal result only to choose feedback.
require("PhraseSourceToggle::toggle",
        "Pattern/Phrase gesture does not use the shared source owner")
require("PhraseSourceToggle::Result::MadePhrase",
        "Pattern source conversion gives no explicit MAKE PHRASE feedback")
require('"MAKE PHRASE"',
        "Pattern source conversion is not named as a musical action")
if "makePhrase(" in SYNTH:
    raise AssertionError("Synth page still decides conversion for itself")
if "RuntimeSynthEventBuffer candidate" in SYNTH:
    raise AssertionError("UI introduced a shadow Phrase/material buffer")
if "currentPhraseBuffer(voice_index_) =" in SYNTH:
    raise AssertionError("UI copies Phrase material instead of invoking domain/runtime")

if "enum class Result" not in TOGGLE or "MadePhrase" not in TOGGLE:
    raise AssertionError("source owner does not expose causal action result")

print("Pattern/Phrase instrument integration: PASS")
