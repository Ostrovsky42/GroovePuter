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
// SOURCE switches between existing materials and never re-projects a Melody
// that already exists, so edits made on the Melody can not be overwritten by
// the steps. When the voice has no Melody yet, SOURCE is also the entry
// gesture: it materializes the current steps through the MAKE PHRASE owner
// (validated, one-way, undoable) and lands on MELODY. Both actions retain the
// same bounded Runtime Phrase undo ownership and AudioGuard path.
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

inline Result toggle(MiniAcid& engine, const AudioGuard& audioGuard,
                     int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return Result::Rejected;
  }

  const bool onPhrase = engine.currentSequencedSource(voiceIndex) ==
                        MiniAcid::SequencedSource::Phrase;

  // No Melody yet: materialize the steps through MAKE PHRASE instead of
  // refusing. makePhrase() validates the candidate before it becomes the
  // sounding owner, so an invalid or non-existent Melody still never plays;
  // if materialization fails the voice stays on PATTERN.
  if (!onPhrase && engine.retainedWorkingMelody(voiceIndex) == nullptr) {
    return makePhrase(engine, audioGuard, voiceIndex) ? Result::MadePhrase
                                                     : Result::Rejected;
  }

  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = static_cast<uint8_t>(voiceIndex);
  receipt.source =
      static_cast<uint8_t>(engine.currentSequencedSource(voiceIndex));
  const auto* retained = engine.retainedWorkingMelody(voiceIndex);
  if (retained != nullptr) {
    receipt.before = *retained;
  }

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
                engine.setSequencedSource(voiceIndex,
                                          MiniAcid::SequencedSource::Phrase);
                if (engine.currentSequencedSource(voiceIndex) ==
                    MiniAcid::SequencedSource::Phrase) {
                  result = Result::SwitchedToPhrase;
                } else {
                  result = Result::Rejected;
                }
              }
            });
    if (!committed) result = Result::Rejected;
  };

  if (audioGuard) audioGuard(apply);
  else apply();
  return result;
}

}  // namespace PhraseSourceToggle

#endif  // GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H
