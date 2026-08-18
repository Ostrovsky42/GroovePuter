#!/usr/bin/env python3
from pathlib import Path


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle!r}")


drum = Path("src/ui/pages/drum_sequencer_page.cpp").read_text()
synth = Path("src/ui/pages/synth_sequencer_page.cpp").read_text()
legacy_synth = Path("src/ui/pages/pattern_edit_page.cpp").read_text()
owner = Path("src/state/undo_owner.h").read_text()
receipts = Path("src/state/undo_receipts.h").read_text()

# One-cell Drum Backspace is intercepted before legacy dispatch and uses the
# same bounded PREPARE -> canonical COMMIT helper as normal manual Drum edits.
require(drum, "const bool singleCellBackspace = ui_event.key == '\\\\b' || ui_event.key == 0x7F;",
        "single-cell Backspace interception")
require(drum, "singleCellBackspace && !ui_event.alt && !page->has_selection_",
        "single-cell-only Drum scope")
require(drum, "page->commitDrumPatternMutation([&](DrumPatternSet& pattern) {",
        "canonical Drum delete commit")
require(drum, "pattern.voices[voice].steps[step].hit = false;",
        "Drum hit clear")
require(drum, "pattern.voices[voice].steps[step].accent = false;",
        "Drum accent clear")
require(receipts, "struct DrumPatternUndoPayload", "bounded Drum receipt")

# Synth NOTES G at STOP is owned by the parent before the legacy child can
# perform its direct mutation. Generation happens on a local candidate; only a
# fixed prepared pattern assignment enters the canonical Pattern COMMIT.
require(synth, "bool isSynthGenerateKey(const UIEvent& event)",
        "Synth G normalization")
require(synth, "synth_tab_ == SynthTab::Notes && isSynthGenerateKey(ui_event) &&\n      !mini_acid_.isPlaying()",
        "STOP-state Synth generation interception")
require(synth, "SynthPattern generated = before.before;", "local generated candidate")
require(synth, "mini_acid_.modeManager().generatePattern(\n        generated, mini_acid_.bpm(), genreParams, behavior, voice_index_);",
        "legacy-equivalent local generation")
require(synth, "GroovePuterUndo::undoOwner().commitPrepared(\n        GroovePuterUndo::UndoKind::Pattern, before",
        "canonical Synth Pattern commit")
require(synth, "GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);",
        "fixed prepared Synth assignment")
require(receipts, "static_assert(sizeof(SynthPatternUndoPayload) == 116",
        "116-byte Synth Pattern receipt")

# Preserve the established Reggae A/B generator split exactly.
for needle in (
    "behavior.stepMask = 0x1111;",
    "behavior.motifLength = 2;",
    "behavior.stepMask = 0xAAAA;",
    "behavior.motifLength = 4;",
):
    require(synth, needle, "Reggae generation invariant")

# The old child path is deliberately still present for PLAY/TIME behavior; the
# parent STOP interception is the new ownership boundary. Do not silently move
# this hardware follow-up onto 0.9.9 pending/quantized activation.
require(legacy_synth, "if (keyG) {", "legacy PLAY fallback")
assert "regenerateSynthWithQuantizedCommit" not in synth
require(owner, "it must not perform generation", "bounded COMMIT prohibition")

print("R9 hardware follow-up source regressions: PASS")
