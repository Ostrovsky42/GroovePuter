#!/usr/bin/env python3
from pathlib import Path

path = Path("src/ui/pages/synth_sequencer_page.cpp")
text = path.read_text(encoding="utf-8")

replacements = [
    (
        '#include "../phrase_notes_selection.h"\n#include "../phrase_notes_duration_edit.h"',
        '#include "../phrase_notes_selection.h"\n#include "../phrase_notes_delete_edit.h"\n#include "../phrase_notes_duration_edit.h"',
    ),
    (
        '  UI::drawStandardFooter(gfx, "L/R:CUR U/D:GRID", "A+L/R:LEN");',
        '  UI::drawStandardFooter(gfx, "L/R:CUR U/D:GRID", "BS:DEL A+L/R:LEN");',
    ),
    (
        '  const int nav = UIInput::navCode(ui_event);\n'
        '  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);\n\n'
        '  if (ui_event.alt) {',
        '  const int nav = UIInput::navCode(ui_event);\n'
        '  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);\n'
        '  const bool isBackspace = ui_event.key == \'\\b\' || ui_event.key == 0x7F;\n\n'
        '  if (isBackspace && !ui_event.alt) {\n'
        '    phrase_cursor_ = PhraseNotesCursor::clamp(\n'
        '        phrase_cursor_, phrase.lengthTicks);\n'
        '    PhraseNotesDeleteEdit::Prepared prepared{};\n'
        '    const auto result = PhraseNotesDeleteEdit::prepare(\n'
        '        phrase, PhraseNotesCursor::tick(phrase_cursor_), prepared);\n'
        '    if (result != PhraseNotesDeleteEdit::Result::Ready) {\n'
        '      UI::showToast(\n'
        '          result == PhraseNotesDeleteEdit::Result::NoTarget\n'
        '              ? "NO NOTE"\n'
        '              : "DELETE FAILED",\n'
        '          900);\n'
        '      return true;\n'
        '    }\n\n'
        '    bool committed = false;\n'
        '    const auto apply = [&]() {\n'
        '      auto& live = mini_acid_.currentPhraseBuffer(voice_index_);\n'
        '      committed = PhraseNotesDeleteEdit::commitIfUnchanged(live, prepared);\n'
        '    };\n'
        '    if (audio_guard_) audio_guard_(apply);\n'
        '    else apply();\n\n'
        '    UI::showToast(committed ? "NOTE DELETED" : "EDIT STALE", 900);\n'
        '    return true;\n'
        '  }\n\n'
        '  if (ui_event.alt) {',
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"guard failed: expected one anchor, found {count}: {old[:80]!r}")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")
