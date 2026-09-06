#!/usr/bin/env python3
from pathlib import Path

path = Path("src/ui/pages/synth_sequencer_page.cpp")
text = path.read_text(encoding="utf-8")

replacements = [
    (
'''bool isSynthGenerateKey(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  return key == 'g' || event.scancode == GROOVEPUTER_G;
}
}  // namespace
''',
'''bool isSynthGenerateKey(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  return key == 'g' || event.scancode == GROOVEPUTER_G;
}

bool commitRuntimePhraseEditWithUndo(
    MiniAcid& miniAcid,
    const AudioGuard& audioGuard,
    int voiceIndex,
    const PhraseRuntime::RuntimeSynthEventBuffer& beforeBuffer,
    const PhraseRuntime::RuntimeSynthEventBuffer& afterBuffer) {
  if (voiceIndex < 0 || voiceIndex >= 2 ||
      !RuntimePhraseEdit::validate(beforeBuffer) ||
      !RuntimePhraseEdit::validate(afterBuffer)) {
    return false;
  }

  bool committed = false;
  const auto apply = [&]() {
    auto& live = miniAcid.currentPhraseBuffer(voiceIndex);
    if (!RuntimePhraseEdit::same(live, beforeBuffer)) return;

    GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
    receipt.voiceIndex = static_cast<uint8_t>(voiceIndex);
    receipt.source = static_cast<uint8_t>(
        miniAcid.currentSequencedSource(voiceIndex));
    receipt.before = beforeBuffer;

    committed = GroovePuterUndo::undoOwner().commitRuntimePrepared(
        GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
          (void)RuntimePhraseEdit::commit(live, afterBuffer);
        });
  };
  if (audioGuard) audioGuard(apply);
  else apply();
  return committed;
}
}  // namespace
'''
    ),
    (
'''    bool committed = false;
    const auto apply = [&]() {
      auto& live = mini_acid_.currentPhraseBuffer(voice_index_);
      committed = PhraseNotesDeleteEdit::commitIfUnchanged(live, prepared);
    };
    if (audio_guard_) audio_guard_(apply);
    else apply();

    UI::showToast(committed ? "NOTE DELETED" : "EDIT STALE", 900);
''',
'''    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);

    UI::showToast(committed ? "NOTE DELETED" : "EDIT STALE", 900);
'''
    ),
    (
'''    bool committed = false;
    const auto apply = [&]() {
      auto& live = mini_acid_.currentPhraseBuffer(voice_index_);
      committed = PhraseNotesDurationEdit::commitIfUnchanged(live, prepared);
    };
    if (audio_guard_) audio_guard_(apply);
    else apply();

    UI::showToast(
''',
'''    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);

    UI::showToast(
'''
    ),
    (
'''  const bool phraseNotes =
      synth_tab_ == SynthTab::Notes &&
      mini_acid_.currentSequencedSource(voice_index_) ==
          MiniAcid::SequencedSource::Phrase;
  if (phraseNotes && handlePhraseNotesEvent(ui_event)) return true;

  if (!phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event) &&
''',
'''  const bool phraseNotes =
      synth_tab_ == SynthTab::Notes &&
      mini_acid_.currentSequencedSource(voice_index_) ==
          MiniAcid::SequencedSource::Phrase;

  if (phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event)) {
    auto& owner = GroovePuterUndo::undoOwner();
    if (owner.hasUndo() &&
        owner.kind() == GroovePuterUndo::UndoKind::RuntimePhrase) {
      const bool redo = owner.nextIsRedo();
      const auto result =
          owner.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
              GroovePuterUndo::UndoKind::RuntimePhrase,
              [&](const GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
                return GroovePuterUndo::validRuntimePhraseUndoPayload(retained) &&
                       retained.voiceIndex == static_cast<uint8_t>(voice_index_);
              },
              [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
                const auto exchange = [&]() {
                  auto& live = mini_acid_.currentPhraseBuffer(voice_index_);
                  GroovePuterUndo::exchangeFixedValue(live, retained.before);
                  const auto currentSource =
                      mini_acid_.currentSequencedSource(voice_index_);
                  mini_acid_.setSequencedSource(
                      voice_index_,
                      static_cast<MiniAcid::SequencedSource>(retained.source));
                  retained.source = static_cast<uint8_t>(currentSource);
                };
                if (audio_guard_) audio_guard_(exchange);
                else exchange();
              });

      if (result == GroovePuterUndo::UndoResult::Restored) {
        UI::showToast(redo ? "REDO: PHRASE" : "UNDO: PHRASE", 900);
      } else if (result == GroovePuterUndo::UndoResult::Expired) {
        UI::showToast(redo ? "REDO: EXPIRED" : "UNDO: EXPIRED", 900);
      } else {
        UI::showToast(GroovePuterUndoUx::fallbackToast(owner.hasUndo()), 900);
      }
      return true;
    }
  }

  if (phraseNotes && handlePhraseNotesEvent(ui_event)) return true;

  if (!phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event) &&
'''
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"guard failed: expected one anchor, found {count}: {old[:100]!r}")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")
