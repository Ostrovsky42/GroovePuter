#pragma once

// PML-C: "allow replacement" of Pattern slots, session-only.
//
// Contract (docs/pml/PML_B_REUSE_CONTRACT.md):
//  * A mark is permission for a FUTURE generation to replace a slot; content stays until then.
//  * Protection beats the mark: Song / Phrase Bank references, CURRENT, Working, NEXT, the live
//    generation Undo receipt and Melody descriptors exclude reuse.
//  * A mark stores the content token and MaterialId; any difference revokes it.
//  * Generation prefers slots that are free now and reaches for marked slots only when no run
//    of the needed length exists otherwise.
//  * Nothing in this header erases anything. Reclaiming happens only inside the guarded
//    generation commit (applyPreparedPersistent).

#include <cstdint>

#include "miniacid_engine.h"
#include "phrase_generator.h"
#include "song_pattern_materializer.h"
#include "src/state/generated_phrase_undo_payload.h"
#include "src/state/reuse_marks.h"
#include "src/state/slot_content_token.h"
#include "src/state/undo_owner.h"


namespace SlotReuse {

enum Holder : uint8_t {
  kHolderSong = 1,
  kHolderPhraseBank = 2,
  kHolderCurrent = 4,
  kHolderWorking = 8,
  kHolderNext = 16,
  kHolderUndoReceipt = 32,
  kHolderMelody = 64,
};

enum class Verdict : uint8_t {
  Ok = 0,
  NotMarked,
  WrongPage,
  Edited,      // content token or MaterialId differs from the one taken at marking time
  Protected,   // at least one holder excludes reuse
};

enum class MarkResult : uint8_t {
  Marked = 0,
  NothingToReplace,  // slot is already free for generation
  Protected,
  OutOfRange,
};

// Slot range of the live generation Undo receipt, if the receipt is one.
inline bool undoReceiptRange(int& firstLocalSlot, int& bars) {
  auto& owner = GroovePuterUndo::undoOwner();
  if (owner.kind() != GroovePuterUndo::UndoKind::Generation ||
      owner.payloadSize() != sizeof(GeneratedPhraseSong::GeneratedPhraseUndoPayload)) {
    return false;
  }
  GeneratedPhraseSong::GeneratedPhraseUndoPayload payload{};
  if (!owner.read(GroovePuterUndo::UndoKind::Generation, payload) ||
      payload.tag != GeneratedPhraseSong::kGeneratedPhraseUndoTag) {
    return false;
  }
  firstLocalSlot = payload.firstLocalSlot;
  bars = payload.bars;
  return true;
}

inline uint8_t holders(const MiniAcid& engine, const Scene& scene, int slot) {
  uint8_t mask = 0;
  const int global = songPatternFromPageBankIndex(
      engine.currentPageIndex(), slot / Bank<SynthPattern>::kPatterns,
      slot % Bank<SynthPattern>::kPatterns);
  for (int t = 0; t < SongPatternMaterializer::kEditableTrackCount; ++t) {
    const SongTrack track = SongPatternMaterializer::editableTrackForIndex(t);
    const int phrase = SongPatternMaterializer::phrasePatternReferenceCount(scene, track, global);
    const int all = SongPatternMaterializer::globalPatternReferenceCount(scene, track, global);
    if (phrase > 0) mask |= kHolderPhraseBank;
    if (all - phrase > 0) mask |= kHolderSong;
  }
  const uint8_t engineHolders = engine.reuseEngineHolders(slot);
  if (engineHolders & MiniAcid::kReuseHolderCurrent) mask |= kHolderCurrent;
  if (engineHolders & MiniAcid::kReuseHolderWorking) mask |= kHolderWorking;
  if (engineHolders & MiniAcid::kReuseHolderNext) mask |= kHolderNext;
  int first = 0, bars = 0;
  if (undoReceiptRange(first, bars) && slot >= first && slot < first + bars) mask |= kHolderUndoReceipt;
  for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
    if (scene.materialSlots[voice][slot].kind == GroovePuterMaterial::MaterialKind::Melody) {
      mask |= kHolderMelody;
    }
  }
  return mask;
}

inline bool slotIsFree(const Scene& scene, int pageIndex, int slot) {
  return PhraseGenerator::localSlotIsSafeForPhrase(scene, pageIndex, slot);
}

inline uint32_t slotId(const Scene& scene, int slot) {
  return static_cast<uint32_t>(scene.materialSlots[0][slot].id.value);
}

inline Verdict verify(const MiniAcid& engine, const Scene& scene, int slot) {
  const auto& marks = engine.reuseMarks();
  if (!marks.marked(slot)) return Verdict::NotMarked;
  if (marks.page != engine.currentPageIndex()) return Verdict::WrongPage;
  if (marks.token[slot] != GroovePuterMaterial::slotContentToken(scene, slot) ||
      marks.id[slot] != slotId(scene, slot)) {
    return Verdict::Edited;
  }
  return holders(engine, scene, slot) != 0 ? Verdict::Protected : Verdict::Ok;
}

// Mark a slot. Refuses protected slots (protection beats the mark) and slots that need no mark.
inline MarkResult mark(MiniAcid& engine, int slot, uint8_t* holdersOut = nullptr) {
  if (slot < 0 || slot >= kPatternsPerPage) return MarkResult::OutOfRange;
  const Scene& scene = engine.sceneManager().currentScene();
  const uint8_t held = holders(engine, scene, slot);
  if (holdersOut) *holdersOut = held;
  if (held != 0) return MarkResult::Protected;
  if (slotIsFree(scene, engine.currentPageIndex(), slot)) return MarkResult::NothingToReplace;
  engine.reuseMarksForReuseModule().set(slot, engine.currentPageIndex(),
                                        GroovePuterMaterial::slotContentToken(scene, slot),
                                        slotId(scene, slot));
  return MarkResult::Marked;
}

inline void unmark(MiniAcid& engine, int slot) { engine.reuseMarksForReuseModule().revoke(slot); }

// Usable by a generation right now: free, or marked and every condition still holds. An edited mark
// is revoked here (the permission ends with the edit).
inline bool usable(MiniAcid& engine, const Scene& scene, int slot) {
  if (slotIsFree(scene, engine.currentPageIndex(), slot)) return true;
  const Verdict v = verify(engine, scene, slot);
  if (v == Verdict::Edited) engine.reuseMarksForReuseModule().revoke(slot);
  return v == Verdict::Ok;
}

// Run start for `bars` slots. Pass 1: slots free now. Pass 2: free or usable marked slots.
// Returns -1 when neither exists.
inline int findRun(MiniAcid& engine, const Scene& scene, int pageIndex, int bars) {
  if (pageIndex < 0 || pageIndex >= kMaxPages || !PhraseGenerator::isSupportedLength(bars) ||
      bars > kPatternsPerPage) {
    return -1;
  }
  const int plain = PhraseGenerator::findSafeContiguousEmptySlots(scene, pageIndex, bars);
  if (plain >= 0 || engine.reuseMarks().mask == 0) return plain;
  for (int start = 0; start + bars <= kPatternsPerPage; ++start) {
    bool ok = true;
    for (int o = 0; o < bars && ok; ++o) ok = usable(engine, scene, start + o);
    if (ok) return start;
  }
  return -1;
}

// Free-or-usable mask without mutating marks (for the preview).
inline uint16_t usableMask(const MiniAcid& engine, const Scene& scene, bool withMarks) {
  uint16_t mask = 0;
  for (int s = 0; s < kPatternsPerPage; ++s) {
    if (slotIsFree(scene, engine.currentPageIndex(), s) ||
        (withMarks && verify(engine, scene, s) == Verdict::Ok)) {
      mask = static_cast<uint16_t>(mask | (1u << s));
    }
  }
  return mask;
}

inline int longestRun(uint16_t mask) {
  int best = 0, run = 0;
  for (int s = 0; s < kPatternsPerPage; ++s) {
    run = (mask & (1u << s)) ? run + 1 : 0;
    if (run > best) best = run;
  }
  return best;
}

struct Preview {
  uint16_t freeNow = 0;
  uint16_t freeAfter = 0;
  int longestNow = 0;
  int longestAfter = 0;
  uint8_t verdicts[kPatternsPerPage]{};   // Verdict per slot (NotMarked for unmarked slots)
  uint8_t slotHolders[kPatternsPerPage]{};
  bool take4Now = false, take4After = false, grow8Now = false, grow8After = false;
};

inline Preview preview(const MiniAcid& engine) {
  const Scene& scene = engine.sceneManager().currentScene();
  Preview p;
  p.freeNow = usableMask(engine, scene, false);
  p.freeAfter = usableMask(engine, scene, true);
  p.longestNow = longestRun(p.freeNow);
  p.longestAfter = longestRun(p.freeAfter);
  p.take4Now = p.longestNow >= 4;
  p.take4After = p.longestAfter >= 4;
  p.grow8Now = p.longestNow >= 8;
  p.grow8After = p.longestAfter >= 8;
  for (int s = 0; s < kPatternsPerPage; ++s) {
    p.verdicts[s] = static_cast<uint8_t>(verify(engine, scene, s));
    p.slotHolders[s] = holders(engine, scene, s);
  }
  return p;
}

// Commit-time reclaim of one slot (called only inside the guarded generation commit): empties the
// three patterns and BOTH descriptors so no stale MaterialId describes the new content, and ends
// the mark.
inline void reclaim(MiniAcid& engine, Scene& scene, int slot) {
  const int bank = slot / Bank<SynthPattern>::kPatterns;
  const int idx = slot % Bank<SynthPattern>::kPatterns;
  scene.synthABanks[bank].patterns[idx] = SynthPattern{};
  scene.synthBBanks[bank].patterns[idx] = SynthPattern{};
  scene.drumBanks[bank].patterns[idx] = DrumPatternSet{};
  for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
    GroovePuterMaterial::clearResidentDescriptor(scene, voice, slot);
  }
  engine.reuseMarksForReuseModule().revoke(slot);
}

}  // namespace SlotReuse
