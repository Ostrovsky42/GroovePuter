#!/usr/bin/env python3
from pathlib import Path

path = Path("src/ui/pages/synth_sequencer_page.cpp")
text = path.read_text(encoding="utf-8")

old_include = '#include "../phrase_notes_selection.h"\n'
new_include = old_include + '#include "../phrase_notes_duration_edit.h"\n'
assert text.count(old_include) == 1
assert "phrase_notes_duration_edit.h" not in text
text = text.replace(old_include, new_include, 1)

old_footer = 'UI::drawStandardFooter(gfx, "L/R:CUR", "U/D:GRID");'
new_footer = 'UI::drawStandardFooter(gfx, "L/R:CUR U/D:GRID", "A+L/R:LEN");'
assert text.count(old_footer) == 1
text = text.replace(old_footer, new_footer, 1)

start = text.index("bool SynthSequencerPage::handlePhraseNotesEvent(UIEvent& ui_event) {")
end = text.index("\nvoid SynthSequencerPage::draw", start)
old_handler = text[start:end]
assert "ui_event.ctrl || ui_event.alt || ui_event.meta" in old_handler
assert "PhraseNotesDurationEdit" not in old_handler

new_handler = '''bool SynthSequencerPage::handlePhraseNotesEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN ||
      ui_event.ctrl || ui_event.meta) {
    return false;
  }

  const int nav = UIInput::navCode(ui_event);
  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);

  if (ui_event.alt) {
    if (nav != GROOVEPUTER_LEFT && nav != GROOVEPUTER_RIGHT) {
      return false;
    }

    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);
    PhraseNotesDurationEdit::Prepared prepared{};
    const int direction = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    const auto result = PhraseNotesDurationEdit::prepare(
        phrase,
        PhraseNotesCursor::tick(phrase_cursor_),
        phrase_cursor_.grid,
        direction,
        prepared);
    if (result != PhraseNotesDurationEdit::Result::Ready) {
      UI::showToast(
          result == PhraseNotesDurationEdit::Result::NoTarget
              ? "NO NOTE"
              : "LEN LIMIT",
          900);
      return true;
    }

    bool committed = false;
    const auto apply = [&]() {
      auto& live = mini_acid_.currentPhraseBuffer(voice_index_);
      committed = PhraseNotesDurationEdit::commitIfUnchanged(live, prepared);
    };
    if (audio_guard_) audio_guard_(apply);
    else apply();

    UI::showToast(
        committed
            ? (direction > 0 ? "NOTE LONGER" : "NOTE SHORTER")
            : "EDIT STALE",
        900);
    return true;
  }

  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    phrase_cursor_ = PhraseNotesCursor::move(
        phrase_cursor_, nav == GROOVEPUTER_RIGHT ? 1 : -1, phrase.lengthTicks);
    return true;
  }
  if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
    phrase_cursor_ = PhraseNotesCursor::changeGrid(
        phrase_cursor_, nav == GROOVEPUTER_UP ? 1 : -1, phrase.lengthTicks);
    return true;
  }
  return false;
}
'''.rstrip()

text = text[:start] + new_handler + text[end:]
path.write_text(text, encoding="utf-8")
