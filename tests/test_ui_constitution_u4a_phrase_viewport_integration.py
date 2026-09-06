#!/usr/bin/env python3
from pathlib import Path

CPP = Path("src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
HDR = Path("src/ui/pages/synth_sequencer_page.h").read_text(encoding="utf-8")
CONT = Path("src/ui/ui_view_continuity.h").read_text(encoding="utf-8")

assert '#include "../phrase_notes_viewport.h"' in CPP or '#include "../phrase_notes_viewport.h"' in HDR or 'phrase_notes_viewport.h' in CPP + HDR
assert "PhraseNotesViewport::resolve" in CPP
assert "PhraseNotesViewport::moveFocus" in CPP
assert "phraseFocusBar[2]" in CONT
assert "phrase_focus_bar_" in HDR

old = "std::min<uint16_t>(\n      phrase.lengthTicks, 2 * PhraseRuntime::kTicksPerBar)"
assert old not in CPP, "Phrase draw must not stay hard-wired to the first two bars"

assert "windowStartSubtick" in CPP
assert "windowEndSubtick" in CPP
assert "UI::drawStandardFooter" in CPP or "UI::publishShellFooter" in CPP
assert "L/R:BAR" in CPP

handler_start = CPP.index("bool SynthSequencerPage::handlePhraseNotesEvent")
handler_end = CPP.index("void SynthSequencerPage::draw", handler_start)
handler = CPP[handler_start:handler_end]
assert "GROOVEPUTER_LEFT" in handler and "GROOVEPUTER_RIGHT" in handler
assert "RuntimePhraseEdit::commit" not in handler
assert "commitPreparedPhrase" not in handler

print("PASS: U4A Phrase viewport is bounded, continuity-backed, and navigation-only")
