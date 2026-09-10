#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H
#define GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H

#include <cstdint>
#include <functional>

#include "src/dsp/miniacid_engine.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

// U4B9: the single owner of PATTERN/PHRASE source selection and the explicit
// MAKE PHRASE gesture.
//
// SOURCE is deliberately a pure source switch. It never projects Pattern
// material as a side effect, including when the Phrase buffer is empty.
// MAKE PHRASE is the distinct one-way materialization command. Both actions
// retain the same bounded Runtime Phrase undo ownership and AudioGuard path.
//
// The receipt carries the source as well as the material, so Ctrl+Z restores
// PATTERN/PHRASE truth and the buffer together rather than leaving the voice on
// one source holding the other's content.
namespace PhraseSourceToggle {

using AudioGuard = std::function<void(const std::function<void()>&)>;

enum class Result : uint8_t {
  Rejected = 0,
  MadePhrase,
  SwitchedToPhrase,
  SwitchedToPattern,
};

inline Result toggle(MiniAcid& engine, const AudioGuard& audioGuard,
                     int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return Result::Rejected;
  }

  const bool onPhrase = engine.currentSequencedSource(voiceIndex) ==
                        MiniAcid::SequencedSource::Phrase;

  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = static_cast<uint8_t>(voiceIndex);
  receipt.source =
      static_cast<uint8_t>(engine.currentSequencedSource(voiceIndex));
  receipt.before = engine.currentPhraseBuffer(voiceIndex);

  Result result = Result::Rejected;
  const auto apply = [&]() {
    const bool committed =
        GroovePuterUndo::undoOwner().commitRuntimePrepared(
            GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
              if (onPhrase) {
                // Back to PATTERN. Phrase material is retained untouched.
                engine.setSequencedSource(voiceIndex,
                                          MiniAcid::SequencedSource::Pattern);
                result = Result::SwitchedToPattern;
              } else {
                // SOURCE is selection only. An empty Phrase is a valid target;
                // only MAKE PHRASE is allowed to materialize Pattern content.
                engine.setSequencedSource(voiceIndex,
                                          MiniAcid::SequencedSource::Phrase);
                result = Result::SwitchedToPhrase;
              }
            });
    if (!committed) result = Result::Rejected;
  };

  if (audioGuard) audioGuard(apply);
  else apply();
  return result;
}

// Explicit one-way musical gesture from Pattern. Unlike SOURCE, this command
// owns materialization. A voice already on Phrase is left untouched so edited
// material cannot be silently re-projected.
inline bool makePhrase(MiniAcid& engine, const AudioGuard& audioGuard,
                       int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  if (engine.currentSequencedSource(voiceIndex) ==
      MiniAcid::SequencedSource::Phrase) {
    return true;
  }

  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = static_cast<uint8_t>(voiceIndex);
  receipt.source =
      static_cast<uint8_t>(engine.currentSequencedSource(voiceIndex));
  receipt.before = engine.currentPhraseBuffer(voiceIndex);

  bool made = false;
  const auto apply = [&]() {
    const bool committed =
        GroovePuterUndo::undoOwner().commitRuntimePrepared(
            GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
              made = engine.makePhrase(voiceIndex);
            });
    if (!committed) made = false;
  };

  if (audioGuard) audioGuard(apply);
  else apply();
  return made;
}

}  // namespace PhraseSourceToggle

#endif  // GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H
