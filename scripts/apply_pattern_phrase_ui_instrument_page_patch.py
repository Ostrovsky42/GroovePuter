#!/usr/bin/env python3
from pathlib import Path

PATH = Path("src/ui/pages/synth_sequencer_page.cpp")
text = PATH.read_text(encoding="utf-8")


def replace_once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one anchor, found {count}")
    return source.replace(old, new, 1)


text = replace_once(
    text,
    '#include "../phrase_notes_join_edit.h"\n#include "../phrase_selection_state.h"',
    '#include "../phrase_notes_join_edit.h"\n#include "../phrase_instrument_controls.h"\n#include "../phrase_selection_state.h"',
    "instrument-controls include",
)

text = replace_once(
    text,
    '''  gfx.setTextColor(voiceColor);
  gfx.drawText(bounds.x + 4, bounds.y, "MELODY");
  char where[20];
  std::snprintf(where, sizeof(where), "BAR %u OF %u",
                static_cast<unsigned>(viewport.focusBar) + 1u,
                static_cast<unsigned>(viewport.totalBars));
  gfx.setTextColor(COLOR_LABEL);
  const int whereX = bounds.x + 4 + textWidth(gfx, "MELODY") + 10;
  gfx.drawText(whereX, bounds.y, where);
  gfx.drawText(whereX + textWidth(gfx, where) + 10, bounds.y, "ALT+R SRC");
''',
    '''  gfx.setTextColor(voiceColor);
  gfx.drawText(bounds.x + 4, bounds.y, "PHRASE");
  char where[32];
  std::snprintf(where, sizeof(where), "PLAY:PHR BAR %u/%u",
                static_cast<unsigned>(viewport.focusBar) + 1u,
                static_cast<unsigned>(viewport.totalBars));
  gfx.setTextColor(COLOR_LABEL);
  const int whereX = bounds.x + 4 + textWidth(gfx, "PHRASE") + 8;
  gfx.drawText(whereX, bounds.y, where);
  const char* sourceHint = "A+R SRC";
  gfx.drawText(bounds.x + bounds.w - 4 - textWidth(gfx, sourceHint),
               bounds.y, sourceHint);
''',
    "roll identity header",
)

text = replace_once(
    text,
    '''  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4, bounds.y + 84,
               "ENTER ADD  J JOIN  BS DEL  ^Z UNDO");
''',
    '''  char controls[48];
  std::snprintf(controls, sizeof(controls), "ENT/J/BS  G GRID %s  L LEN %u  []BAR",
                PhraseNotesCursor::gridLabel(phrase_cursor_.grid),
                static_cast<unsigned>(
                    PhraseInstrumentControls::lengthBars(phrase.lengthTicks)));
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4, bounds.y + 84, controls);
''',
    "roll control strip",
)

text = replace_once(
    text,
    '''  phrase_selection_ = PhraseSelectionState::resolve(phrase, phrase_selection_);
  if (!phrase_selection_.active) {
    phrase_selection_ = PhraseSelectionState::first(phrase);
  }
  const IGfxColor voiceColor = synthTabColor(voice_index_);

  gfx.setTextColor(voiceColor);
  gfx.drawText(bounds.x + 4, bounds.y, "SOUNDS");
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4 + textWidth(gfx, "SOUNDS") + 10, bounds.y,
               "V ROLL   ALT+R SRC");
''',
    '''  phrase_cursor_ = PhraseNotesCursor::clamp(phrase_cursor_, phrase.lengthTicks);
  const PhraseNotesViewport::Window viewport = PhraseNotesViewport::resolve(
      phrase.lengthTicks, PhraseNotesCursor::focusBar(phrase_cursor_));
  phrase_selection_ = PhraseSelectionState::resolve(phrase, phrase_selection_);
  if (!phrase_selection_.active) {
    phrase_selection_ = PhraseSelectionState::first(phrase);
  }
  const IGfxColor voiceColor = synthTabColor(voice_index_);

  gfx.setTextColor(voiceColor);
  gfx.drawText(bounds.x + 4, bounds.y, "PHRASE");
  char where[32];
  std::snprintf(where, sizeof(where), "PLAY:PHR BAR %u/%u",
                static_cast<unsigned>(viewport.focusBar) + 1u,
                static_cast<unsigned>(viewport.totalBars));
  gfx.setTextColor(COLOR_LABEL);
  const int whereX = bounds.x + 4 + textWidth(gfx, "PHRASE") + 8;
  gfx.drawText(whereX, bounds.y, where);
  const char* sourceHint = "A+R SRC";
  gfx.drawText(bounds.x + bounds.w - 4 - textWidth(gfx, sourceHint),
               bounds.y, sourceHint);
''',
    "list identity header",
)

text = replace_once(
    text,
    '''  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4, bounds.y + 82, "ENTER ADD  J JOIN  BS DEL");
''',
    '''  char controls[48];
  std::snprintf(controls, sizeof(controls), "ENT/J/BS  G GRID %s  L LEN %u  []BAR",
                PhraseNotesCursor::gridLabel(phrase_cursor_.grid),
                static_cast<unsigned>(
                    PhraseInstrumentControls::lengthBars(phrase.lengthTicks)));
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4, bounds.y + 82, controls);
''',
    "list control strip",
)

text = replace_once(
    text,
    '''  const int nav = UIInput::navCode(ui_event);
  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);
  const bool isBackspace = ui_event.key == '\\b' || ui_event.key == 0x7F;

  // Enter adds a sound where the cursor stands. Until now the editor could
''',
    '''  const int nav = UIInput::navCode(ui_event);
  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);
  const bool isBackspace = ui_event.key == '\\b' || ui_event.key == 0x7F;
  const char plainKey = ui_event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(ui_event.key)))
      : 0;

  phrase_selection_ = PhraseSelectionState::resolve(phrase, phrase_selection_);
  if (!phrase_selection_.active) {
    phrase_selection_ = PhraseSelectionState::first(phrase);
  }
  const uint16_t selectedEventIndex = phrase_selection_.active
      ? phrase_selection_.eventIndex
      : phrase.count;

  // LENGTH is a domain command, not UI-owned material. L grows through
  // 1/2/4/8 bars; ALT+L walks the same set backwards. A contraction that would
  // cut existing events is rejected by RuntimePhraseEdit before the domain
  // command is committed, so the gesture cannot leave a malformed Phrase.
  if (plainKey == 'l') {
    const auto before = phrase;
    uint8_t requestedBars =
        PhraseInstrumentControls::lengthBars(before.lengthTicks);
    bool domainChanged = false;
    const bool changed = PhraseInstrumentControls::applyLengthChange(
        before.lengthTicks, ui_event.alt ? -1 : 1,
        [&](uint8_t bars) {
          requestedBars = bars;
          auto candidate = before;
          if (RuntimePhraseEdit::setLengthBars(candidate, bars) !=
              RuntimePhraseEdit::LengthEditResult::Changed) {
            return false;
          }

          const auto apply = [&]() {
            auto& live = mini_acid_.currentPhraseBuffer(voice_index_);
            if (!RuntimePhraseEdit::same(live, before)) return;

            GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
            receipt.voiceIndex = static_cast<uint8_t>(voice_index_);
            receipt.source = static_cast<uint8_t>(
                mini_acid_.currentSequencedSource(voice_index_));
            receipt.before = before;

            bool setApplied = false;
            const bool undoCommitted =
                GroovePuterUndo::undoOwner().commitRuntimePrepared(
                    GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
                      setApplied =
                          mini_acid_.setPhraseLength(voice_index_, bars);
                    });
            domainChanged = undoCommitted && setApplied;
          };
          if (audio_guard_) audio_guard_(apply);
          else apply();
          return domainChanged;
        });

    if (changed) {
      const auto& current = mini_acid_.currentPhraseBuffer(voice_index_);
      phrase_cursor_ = PhraseNotesCursor::clamp(phrase_cursor_, current.lengthTicks);
      phrase_selection_ = PhraseSelectionState::resolve(current, phrase_selection_);
      char toast[32];
      std::snprintf(toast, sizeof(toast), "PHRASE LEN %u BAR%s",
                    static_cast<unsigned>(requestedBars),
                    requestedBars == 1 ? "" : "S");
      UI::showToast(toast, 1000);
    } else {
      UI::showToast("LENGTH BLOCKED", 1000);
    }
    return true;
  }

  // BAR navigation is presentation continuity only: it moves the insertion
  // cursor to the same within-bar position and never mutates Phrase material.
  if (!ui_event.alt &&
      (ui_event.key == '[' || ui_event.key == '{' ||
       ui_event.key == ']' || ui_event.key == '}')) {
    const int direction =
        (ui_event.key == ']' || ui_event.key == '}') ? 1 : -1;
    phrase_cursor_ = PhraseInstrumentControls::jumpBar(
        phrase_cursor_, direction, phrase.lengthTicks);
    phrase_selection_ = PhraseSelectionState::withInsertTick(
        phrase_selection_, PhraseNotesCursor::tick(phrase_cursor_));
    const PhraseNotesViewport::Window viewport = PhraseNotesViewport::resolve(
        phrase.lengthTicks, PhraseNotesCursor::focusBar(phrase_cursor_));
    char toast[32];
    std::snprintf(toast, sizeof(toast), "PHRASE BAR %u/%u",
                  static_cast<unsigned>(viewport.focusBar) + 1u,
                  static_cast<unsigned>(viewport.totalBars));
    UI::showToast(toast, 900);
    return true;
  }

  // Enter adds a sound where the cursor stands. Until now the editor could
''',
    "handler controls and selection owner",
)

text = replace_once(
    text,
    '''    PhraseNotesDeleteEdit::Prepared prepared{};
    const auto result = PhraseNotesDeleteEdit::prepare(
        phrase, PhraseNotesCursor::tick(phrase_cursor_), prepared);
''',
    '''    PhraseNotesDeleteEdit::Prepared prepared{};
    const auto result = PhraseNotesDeleteEdit::prepareSelected(
        phrase, selectedEventIndex, prepared);
''',
    "delete selected target",
)

text = replace_once(
    text,
    '''    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);

    UI::showToast(committed ? "NOTE DELETED" : "EDIT STALE", 900);
    return true;
  }

  if (ui_event.alt) {
''',
    '''    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);
    if (committed) {
      phrase_selection_ = PhraseSelectionState::resolve(
          mini_acid_.currentPhraseBuffer(voice_index_), phrase_selection_);
    }

    UI::showToast(committed ? "NOTE DELETED" : "EDIT STALE", 900);
    return true;
  }

  if (ui_event.alt) {
''',
    "delete selection repair",
)

text = replace_once(
    text,
    '''    const PhraseNotesSelection::Selection before =
        PhraseNotesSelection::deriveInCell(
            phrase, PhraseNotesCursor::tick(phrase_cursor_),
            PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
    const uint32_t audibleBefore =
        before.active ? audibleEndTick(phrase, before.eventIndex) : 0;
    const auto result = PhraseNotesDurationEdit::prepare(
        phrase,
        PhraseNotesCursor::tick(phrase_cursor_),
        phrase_cursor_.grid,
        direction,
        prepared);
''',
    '''    const uint32_t audibleBefore = selectedEventIndex < phrase.count
        ? audibleEndTick(phrase, selectedEventIndex)
        : 0;
    const auto result = PhraseNotesDurationEdit::prepareSelected(
        phrase, selectedEventIndex, phrase_cursor_.grid, direction, prepared);
''',
    "duration selected target",
)

text = replace_once(
    text,
    '''    const auto& after = mini_acid_.currentPhraseBuffer(voice_index_);
    const PhraseNotesSelection::Selection nowSelected =
        PhraseNotesSelection::deriveInCell(
            after, PhraseNotesCursor::tick(phrase_cursor_),
            PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
    const bool audibleChanged =
        !nowSelected.active ||
        audibleEndTick(after, nowSelected.eventIndex) != audibleBefore;
''',
    '''    const auto& after = mini_acid_.currentPhraseBuffer(voice_index_);
    phrase_selection_ = PhraseSelectionState::resolve(after, phrase_selection_);
    const bool audibleChanged =
        selectedEventIndex >= after.count ||
        audibleEndTick(after, selectedEventIndex) != audibleBefore;
''',
    "duration selection repair",
)

text = replace_once(
    text,
    '''    phrase_cursor_ = PhraseNotesCursor::move(
        phrase_cursor_, nav == GROOVEPUTER_RIGHT ? 1 : -1, phrase.lengthTicks);
    return true;
''',
    '''    phrase_cursor_ = PhraseNotesCursor::move(
        phrase_cursor_, nav == GROOVEPUTER_RIGHT ? 1 : -1, phrase.lengthTicks);
    const uint16_t insertTick = PhraseNotesCursor::tick(phrase_cursor_);
    phrase_selection_ = PhraseSelectionState::withInsertTick(
        phrase_selection_, insertTick);
    const PhraseNotesSelection::Selection underCursor =
        PhraseNotesSelection::deriveInCell(
            phrase, insertTick,
            PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
    if (underCursor.active) {
      const uint16_t retainedInsertTick = phrase_selection_.insertTick;
      phrase_selection_ =
          PhraseSelectionState::at(phrase, underCursor.eventIndex);
      phrase_selection_.insertTick = retainedInsertTick;
    }
    return true;
''',
    "roll cursor selects occupied cell",
)

text = replace_once(
    text,
    '''    PhraseNotesJoinEdit::Prepared prepared{};
    const auto result = PhraseNotesJoinEdit::prepare(
        phrase, PhraseNotesCursor::tick(phrase_cursor_),
        phrase_cursor_.grid, prepared);
''',
    '''    PhraseNotesJoinEdit::Prepared prepared{};
    const auto result = PhraseNotesJoinEdit::prepareSelected(
        phrase, selectedEventIndex, prepared);
''',
    "join selected target",
)

text = replace_once(
    text,
    '''    const auto& joined = mini_acid_.currentPhraseBuffer(voice_index_);
    const PhraseNotesSelection::Selection stillSelected =
        PhraseNotesSelection::deriveInCell(
            joined, PhraseNotesCursor::tick(phrase_cursor_),
            PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
    bool stillCut = false;
    if (stillSelected.active) {
      const auto& event = joined.events[stillSelected.eventIndex];
      const uint32_t stored = event.startTick +
          event.durationSubticks / PhraseRuntime::kSubticksPerTick;
      stillCut = audibleEndTick(joined, stillSelected.eventIndex) < stored;
    }
''',
    '''    const auto& joined = mini_acid_.currentPhraseBuffer(voice_index_);
    phrase_selection_ = PhraseSelectionState::resolve(joined, phrase_selection_);
    bool stillCut = false;
    if (phrase_selection_.active) {
      const auto& event = joined.events[phrase_selection_.eventIndex];
      const uint32_t stored = event.startTick +
          event.durationSubticks / PhraseRuntime::kSubticksPerTick;
      stillCut =
          audibleEndTick(joined, phrase_selection_.eventIndex) < stored;
    }
''',
    "join selection repair",
)

text = replace_once(
    text,
    '''    char toast[24];
    std::snprintf(toast, sizeof(toast), "STEP %s",
                  PhraseNotesCursor::gridLabel(phrase_cursor_.grid));
    UI::showToast(toast, 900);
''',
    '''    phrase_selection_ = PhraseSelectionState::withInsertTick(
        phrase_selection_, PhraseNotesCursor::tick(phrase_cursor_));
    char toast[24];
    std::snprintf(toast, sizeof(toast), "GRID %s",
                  PhraseNotesCursor::gridLabel(phrase_cursor_.grid));
    UI::showToast(toast, 900);
''',
    "grid causal label",
)

text = replace_once(
    text,
    '''    const auto result = PhraseNotesPitchEdit::prepare(
        phrase, PhraseNotesCursor::tick(phrase_cursor_), direction, prepared);
''',
    '''    const auto result = PhraseNotesPitchEdit::prepareSelected(
        phrase, selectedEventIndex, direction, prepared);
''',
    "pitch selected target",
)

text = replace_once(
    text,
    '''    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);

    UI::showToast(
        committed
            ? (direction > 0 ? "NOTE HIGHER" : "NOTE LOWER")
            : "EDIT STALE",
        900);
''',
    '''    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);
    if (committed) {
      const auto& after = mini_acid_.currentPhraseBuffer(voice_index_);
      if (selectedEventIndex < after.count) {
        const uint16_t retainedInsertTick = phrase_selection_.insertTick;
        phrase_selection_ = PhraseSelectionState::at(after, selectedEventIndex);
        phrase_selection_.insertTick = retainedInsertTick;
      }
    }

    UI::showToast(
        committed
            ? (direction > 0 ? "NOTE HIGHER" : "NOTE LOWER")
            : "EDIT STALE",
        900);
''',
    "pitch selection repair",
)

text = replace_once(
    text,
    '''  // The source switch belongs to both views, so it sits above the PHRASE-only
  // handler: from PATTERN there is otherwise no way back except three
  // keypresses on the MORE tab.
  if (synth_tab_ == SynthTab::Notes && isSourceToggleKey(ui_event)) {
    PhraseSourceToggle::toggle(mini_acid_, audio_guard_, voice_index_);
    UI::showToast(mini_acid_.currentSequencedSource(voice_index_) ==
                          MiniAcid::SequencedSource::Phrase
                      ? "SOURCE: MELODY"
                      : "SOURCE: PATTERN",
                  1000);
    return true;
  }
''',
    '''  // From Pattern, ALT+R is the explicit one-way MAKE PHRASE gesture.
  // From Phrase, the same physical key returns to Pattern source. Both paths
  // reuse PhraseSourceToggle, so the UI never owns a second material/source flag.
  if (synth_tab_ == SynthTab::Notes && isSourceToggleKey(ui_event)) {
    if (mini_acid_.currentSequencedSource(voice_index_) ==
        MiniAcid::SequencedSource::Pattern) {
      const bool made =
          PhraseSourceToggle::makePhrase(mini_acid_, audio_guard_, voice_index_);
      UI::showToast(made ? "MAKE PHRASE" : "MAKE PHRASE FAILED", 1000);
    } else {
      PhraseSourceToggle::toggle(mini_acid_, audio_guard_, voice_index_);
      UI::showToast("SOURCE: PATTERN", 1000);
    }
    return true;
  }
''',
    "explicit make phrase gesture",
)

PATH.write_text(text, encoding="utf-8")
print("Pattern/Phrase SynthSequencerPage patch applied")
