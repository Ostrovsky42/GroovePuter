#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H
#define GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <new>

#include "src/dsp/miniacid_engine.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

// U4B9: the single owner of PATTERN/PHRASE source selection and the explicit
// MAKE PHRASE gesture.
//
// SOURCE switches between existing materials and never re-projects a Melody
// that already exists, so edits made on the Melody can not be overwritten by
// the steps. With no Melody the synth/params switch (Alt+R, Opt) copies the
// steps into a new Melody through the MAKE PHRASE owner (validated, one-way,
// undoable; the callers toast "MELODY <- STEPS"); Ctrl+Z removes it again.
// Alt+N builds an EMPTY Melody instead (newEmptyMelody). The SONG page does not
// use this switch: Opt there refuses a cell with no Melody. All actions retain
// the same bounded Runtime Phrase undo ownership and AudioGuard path.
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
                       int voiceIndex, bool emptyMelody = false) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  if (engine.currentSequencedSource(voiceIndex) ==
      MiniAcid::SequencedSource::Phrase) {
    return true;
  }

  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = static_cast<uint8_t>(voiceIndex);
  receipt.source =
      static_cast<uint8_t>(engine.currentSequencedSource(voiceIndex));
  // With no Melody held, currentPhraseBuffer() aliases other storage (it can
  // read as the steps). Undo's "before" for that state is a clean empty buffer.
  if (engine.retainedWorkingMelody(voiceIndex) == nullptr) {
    receipt.before = PhraseRuntime::RuntimeSynthEventBuffer{};
  } else {
    receipt.before = engine.currentPhraseBuffer(voiceIndex);
  }

  bool made = false;
  const auto apply = [&]() {
    const bool committed =
        GroovePuterUndo::undoOwner().commitRuntimePrepared(
            GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
              made = engine.makePhrase(voiceIndex, emptyMelody);
            });
    if (!committed) made = false;
  };

  if (audioGuard) audioGuard(apply);
  else apply();
  return made;
}

// Alt+N entry from STEPS with no Melody: builds the EMPTY Melody directly. The
// Undo receipt's "before" is the state with no Melody, so Ctrl+Z gives back
// STEPS and never a Melody copied from the steps.
inline bool newEmptyMelody(MiniAcid& engine, const AudioGuard& audioGuard,
                           int voiceIndex) {
  return makePhrase(engine, audioGuard, voiceIndex, true);
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
    // A slot that already holds an accepted Melody plays that Melody; only a
    // slot without one is materialized from its own steps.
    const int bank = engine.current303BankIndex(voiceIndex);
    const int pattern = engine.display303LocalPatternIndex(voiceIndex);
    if (engine.isMelodySlot(voiceIndex, bank, pattern)) {
      std::unique_ptr<PhraseRuntime::RuntimeSynthEventBuffer> saved(
          new (std::nothrow) PhraseRuntime::RuntimeSynthEventBuffer());
      if (!saved || !engine.loadCurrentSlotMelody(voiceIndex, *saved)) {
        return Result::Rejected;
      }
      bool activated = false;
      const auto activate = [&]() {
        activated = engine.activateMelodySlot(voiceIndex, bank, pattern, *saved);
      };
      if (audioGuard) audioGuard(activate);
      else activate();
      return activated ? Result::SwitchedToPhrase : Result::Rejected;
    }
    return makePhrase(engine, audioGuard, voiceIndex) ? Result::MadePhrase
                                                     : Result::Rejected;
  }

  // Switching what sounds (STEPS <-> MELODY) changes no stored material, so it
  // writes no Undo entry: the single Undo slot keeps the last real edit.
  Result result = Result::Rejected;
  const auto apply = [&]() {
    if (onPhrase) {
      // Back to PATTERN. Phrase material is retained untouched.
      engine.setSequencedSource(voiceIndex, MiniAcid::SequencedSource::Pattern);
      result = Result::SwitchedToPattern;
    } else {
      engine.setSequencedSource(voiceIndex, MiniAcid::SequencedSource::Phrase);
      result = engine.currentSequencedSource(voiceIndex) ==
                       MiniAcid::SequencedSource::Phrase
                   ? Result::SwitchedToPhrase
                   : Result::Rejected;
    }
  };
  if (audioGuard) audioGuard(apply);
  else apply();
  return result;
}

// User-facing STEPS <-> MELODY switch (Alt+R, Opt on the synth and params
// pages). Existing material is only selected, never re-projected. With no
// Melody for the voice the steps are copied into a new one (MadePhrase); that
// goes through makePhrase(), whose Undo receipt has the no-Melody state as
// "before", so Ctrl+Z returns to STEPS with no Melody. SONG has its own refusal.
inline Result switchSource(MiniAcid& engine, const AudioGuard& audioGuard,
                           int voiceIndex) {
  return toggle(engine, audioGuard, voiceIndex);
}

}  // namespace PhraseSourceToggle

#endif  // GROOVEPUTER_SRC_UI_PHRASE_SOURCE_TOGGLE_H
