#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H
#define GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H

#include <cstdint>
#include <functional>

#include "src/dsp/miniacid_engine.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

// U4B9: the single owner of the PATTERN/PHRASE switch.
//
// The decision has three branches and carries an undo receipt, and it now has
// two entry points -- the SRC row on the MORE tab and ALT+R on the editor
// itself. Two copies of that would diverge, so both call here.
//
// The receipt carries the source as well as the material, so Ctrl+Z restores
// PATTERN/PHRASE truth and the buffer together rather than leaving the voice on
// one source holding the other's content.
namespace PhraseSourceToggle {

using AudioGuard = std::function<void(const std::function<void()>&)>;

inline void toggle(MiniAcid& engine, const AudioGuard& audioGuard,
                   int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return;

  const bool onPhrase = engine.currentSequencedSource(voiceIndex) ==
                        MiniAcid::SequencedSource::Phrase;

  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = static_cast<uint8_t>(voiceIndex);
  receipt.source =
      static_cast<uint8_t>(engine.currentSequencedSource(voiceIndex));
  receipt.before = engine.currentPhraseBuffer(voiceIndex);

  const auto apply = [&]() {
    (void)GroovePuterUndo::undoOwner().commitRuntimePrepared(
        GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
          if (onPhrase) {
            // Back to PATTERN. The phrase material is kept, so returning to
            // PHRASE later does not re-project over edits.
            engine.setSequencedSource(voiceIndex,
                                      MiniAcid::SequencedSource::Pattern);
          } else if (engine.currentPhraseBuffer(voiceIndex).count > 0) {
            // Material already exists: this is a source switch, not a
            // conversion, so makePhrase() must not run again.
            engine.setSequencedSource(voiceIndex,
                                      MiniAcid::SequencedSource::Phrase);
          } else {
            (void)engine.makePhrase(voiceIndex);
          }
        });
  };

  if (audioGuard) audioGuard(apply);
  else apply();
}

}  // namespace PhraseSourceToggle

#endif  // GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H
