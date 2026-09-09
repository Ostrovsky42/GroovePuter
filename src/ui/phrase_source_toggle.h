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
// SELECT SOURCE and MAKE PHRASE are distinct musical actions. Source selection
// only changes which already-authoritative buffer is heard/edited; it must not
// project Pattern material into an empty Phrase as a side effect. MAKE PHRASE
// owns the one-way Pattern -> Phrase materialization gesture.
//
// Both operations carry the same runtime Phrase undo receipt so Ctrl+Z restores
// PATTERN/PHRASE truth and the Phrase buffer atomically rather than leaving the
// voice on one source holding the other's content.
namespace PhraseSourceToggle {

using AudioGuard = std::function<void(const std::function<void()>&)>;

enum class Result : uint8_t {
  Rejected = 0,
  MadePhrase,
  SwitchedToPhrase,
  SwitchedToPattern,
};

inline GroovePuterUndo::RuntimePhraseUndoPayload makeReceipt(
    MiniAcid& engine, int voiceIndex) {
  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = static_cast<uint8_t>(voiceIndex);
  receipt.source =
      static_cast<uint8_t>(engine.currentSequencedSource(voiceIndex));
  receipt.before = engine.currentPhraseBuffer(voiceIndex);
  return receipt;
}

inline Result toggle(MiniAcid& engine, const AudioGuard& audioGuard,
                     int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return Result::Rejected;
  }

  const bool onPhrase = engine.currentSequencedSource(voiceIndex) ==
                        MiniAcid::SequencedSource::Phrase;
  const GroovePuterUndo::RuntimePhraseUndoPayload receipt =
      makeReceipt(engine, voiceIndex);

  Result result = Result::Rejected;
  const auto apply = [&]() {
    const bool committed =
        GroovePuterUndo::undoOwner().commitRuntimePrepared(
            GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
              if (onPhrase) {
                engine.setSequencedSource(voiceIndex,
                                          MiniAcid::SequencedSource::Pattern);
                result = Result::SwitchedToPattern;
              } else {
                // Pure source selection. An empty Phrase remains empty; only
                // explicit MAKE PHRASE is allowed to materialize Pattern data.
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

// Explicit one-way Pattern -> Phrase materialization. This is deliberately not
// implemented by toggle(): SOURCE is selection-only, while MAKE PHRASE is the
// sole command allowed to project the Pattern into Phrase material. Repeating
// MAKE PHRASE on an already-active Phrase is a no-op so edits are never lost.
inline bool makePhrase(MiniAcid& engine, const AudioGuard& audioGuard,
                       int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  if (engine.currentSequencedSource(voiceIndex) ==
      MiniAcid::SequencedSource::Phrase) {
    return true;
  }

  const GroovePuterUndo::RuntimePhraseUndoPayload receipt =
      makeReceipt(engine, voiceIndex);
  bool materialized = false;
  const auto apply = [&]() {
    const bool committed =
        GroovePuterUndo::undoOwner().commitRuntimePrepared(
            GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
              materialized = engine.makePhrase(voiceIndex);
            });
    if (!committed) materialized = false;
  };

  if (audioGuard) audioGuard(apply);
  else apply();
  return materialized;
}

}  // namespace PhraseSourceToggle

#endif  // GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H
