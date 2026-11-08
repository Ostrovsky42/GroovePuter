// PML-A (read-only inventory): why "delete Song rows" can still leave NO SLOTS, who holds each pattern
// slot, what could be freed and what could not, and what the existing Undo/save mechanisms can restore.
//
// Host only. Runs in an isolated temporary project (the caller runs it from a private working
// directory). Nothing here frees, deletes or reclaims anything: the preview is computed from masks.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/song_pattern_materializer.h"
#include "src/state/generation_request_state.h"
#include "src/state/material_version.h"
#include "src/state/undo_owner.h"

SerialMock Serial;
SDMock SD;

namespace {
namespace R = GroovePuterRhythm;
constexpr float kRate = 44100.0f;
const auto kGuard = [](auto&& body) { body(); };

struct SlotRow {
  bool a = false, b = false, d = false;         // physical content per lane
  bool descA = false, descB = false;            // descriptor not canonical-free
  const char* kindA = "free";
  int songRefs = 0, phraseRefs = 0;             // references (Song rows of both slots / Phrase Bank)
  bool current = false, working = false, pending = false, melody = false;
  bool undoReceipt = false, recipe = false, origin = false, originUnedited = false, songGeneratedBit = false;
};

bool hasNotes(const SynthPattern& p) { return !PhraseGenerator::synthPatternIsEmpty(p); }

int slotGlobal(MiniAcid& e, int slot) {
  return songPatternFromPageBankIndex(e.currentPageIndex(), slot / Bank<SynthPattern>::kPatterns,
                                      slot % Bank<SynthPattern>::kPatterns);
}

SlotRow inspect(MiniAcid& e, int slot) {
  Scene& sc = e.sceneManager().currentScene();
  const int bank = slot / Bank<SynthPattern>::kPatterns, idx = slot % Bank<SynthPattern>::kPatterns;
  SlotRow r;
  r.a = hasNotes(sc.synthABanks[bank].patterns[idx]);
  r.b = hasNotes(sc.synthBBanks[bank].patterns[idx]);
  r.d = !PhraseGenerator::drumPatternSetIsEmpty(sc.drumBanks[bank].patterns[idx]);
  r.descA = !GroovePuterMaterial::residentSlotIsFree(sc, 0, slot);
  r.descB = !GroovePuterMaterial::residentSlotIsFree(sc, 1, slot);
  r.melody = sc.materialSlots[0][slot].kind == GroovePuterMaterial::MaterialKind::Melody ||
             sc.materialSlots[1][slot].kind == GroovePuterMaterial::MaterialKind::Melody;
  r.kindA = r.melody ? "melody" : (sc.materialSlots[0][slot].id.valid() ? "pattern+id" : "free");
  const int g = slotGlobal(e, slot);
  for (int t = 0; t < SongPatternMaterializer::kEditableTrackCount; ++t) {
    const SongTrack tr = SongPatternMaterializer::editableTrackForIndex(t);
    r.phraseRefs += SongPatternMaterializer::phrasePatternReferenceCount(sc, tr, g);
    r.songRefs += SongPatternMaterializer::globalPatternReferenceCount(sc, tr, g) -
                  SongPatternMaterializer::phrasePatternReferenceCount(sc, tr, g);
  }
  // CURRENT: what the selectors point at; working material bound to it; NEXT queued on it.
  for (int v = 0; v < NUM_303_VOICES; ++v) {
    if (e.current303BankIndex(v) == bank && e.display303LocalPatternIndex(v) == idx) r.current = true;
    if (!e.workingMaterial_[v].empty()) {
      GroovePuterMaterial::MaterialReference ref{};
      if (e.current303MaterialReference_(v, ref) && e.workingMaterial_[v].patternMatches(ref) &&
          e.current303BankIndex(v) == bank && e.display303LocalPatternIndex(v) == idx) r.working = true;
    }
    if (e.pendingMaterial_[v].queued && e.pendingMaterial_[v].slot == slot) r.pending = true;
  }
  if (e.sceneManager().getCurrentBankIndex(2) == bank &&
      e.sceneManager().getCurrentDrumPatternIndex() == idx) r.current = true;  // drum selector
  if (const auto* rec = e.generatedPhraseRecipe()) {
    if (slot >= rec->firstLocalSlot && slot < rec->firstLocalSlot + rec->bars) r.recipe = true;
  }
  if (const auto* origin = e.generatedSynthAOrigin()) {
    for (int i = 0; i < origin->common.barCount; ++i) {
      if (origin->bars[i].material.address.globalSlot == g) {
        r.origin = true;
        r.originUnedited = origin->bars[i].originPatternVersion ==
                           GroovePuterMaterial::versionForPattern(sc.synthABanks[bank].patterns[idx]);
      }
    }
  }
  r.songGeneratedBit = SongPatternMaterializer::slotIsSongGenerated(sc, SongTrack::SynthA, slot);
  GeneratedPhraseSong::GeneratedPhraseUndoPayload undo{};
  if (GroovePuterUndo::undoOwner().read(GroovePuterUndo::UndoKind::Generation, undo) &&
      undo.tag == GeneratedPhraseSong::kGeneratedPhraseUndoTag &&
      slot >= undo.firstLocalSlot && slot < undo.firstLocalSlot + undo.bars) r.undoReceipt = true;
  return r;
}

// Why a slot does not count as free for phrase generation (the exact predicate generate() uses).
bool safeForPhrase(MiniAcid& e, int slot) {
  return PhraseGenerator::localSlotIsSafeForPhrase(e.sceneManager().currentScene(), e.currentPageIndex(), slot);
}

const char* blockedBy(const SlotRow& r) {
  static char buf[96];
  buf[0] = 0;
  auto add = [&](const char* s) { if (buf[0]) std::strcat(buf, "+"); std::strcat(buf, s); };
  if (r.a || r.b || r.d) add("content");
  if (r.descA || r.descB) add("descriptor");
  if (r.songRefs) add("song");
  if (r.phraseRefs) add("phrasebank");
  return buf[0] ? buf : "-";
}

// Candidates if the owner allowed freeing: nothing protected by Song/Phrase Bank/CURRENT/working/NEXT/
// Melody/Undo receipt. Class tells how risky the content is.
const char* protectedBy(const SlotRow& r) {
  static char buf[96];
  buf[0] = 0;
  auto add = [&](const char* s) { if (buf[0]) std::strcat(buf, ","); std::strcat(buf, s); };
  if (r.songRefs) add("SONG");
  if (r.phraseRefs) add("PHRASE-BANK");
  if (r.current) add("CURRENT");
  if (r.working) add("WORKING");
  if (r.pending) add("NEXT");
  if (r.melody) add("MELODY");
  if (r.undoReceipt) add("UNDO-RECEIPT");
  return buf[0] ? buf : "";
}

const char* contentClass(const SlotRow& r) {
  if (!(r.a || r.b || r.d) && !(r.descA || r.descB)) return "empty";
  if (!(r.a || r.b || r.d)) return "orphan-descriptor";
  if (r.origin && r.originUnedited) return "generated-unedited";
  if (r.origin) return "generated-edited";
  if (r.songGeneratedBit) return "song-generated";
  return "unknown/manual";
}

int firstRun(const bool* free, int bars) {
  for (int s = 0; s + bars <= kPatternsPerPage; ++s) {
    bool ok = true;
    for (int o = 0; o < bars; ++o) if (!free[s + o]) { ok = false; break; }
    if (ok) return s;
  }
  return -1;
}

void table(MiniAcid& e, const char* title) {
  std::printf("\n== %s ==\n", title);
  std::printf("slot A B D  desc  content-class        refs(song/pb)  holds(protected)           blocks-phrase-by  free-for-phrase\n");
  bool freeNow[kPatternsPerPage], freeIfCandidates[kPatternsPerPage];
  int freeCount = 0, candCount = 0;
  for (int s = 0; s < kPatternsPerPage; ++s) {
    const SlotRow r = inspect(e, s);
    const bool free = safeForPhrase(e, s);
    freeNow[s] = free; if (free) ++freeCount;
    const bool prot = protectedBy(r)[0] != 0;
    const bool content = r.a || r.b || r.d || r.descA || r.descB;
    // candidate = occupies the slot only by content/descriptor, protected by nothing
    freeIfCandidates[s] = free || (content && !prot && r.songRefs == 0 && r.phraseRefs == 0);
    if (!free && freeIfCandidates[s]) ++candCount;
    std::printf("%2d   %c %c %c  %c%c    %-19s  %d/%d            %-26s %-17s %s\n", s, r.a ? 'x' : '.', r.b ? 'x' : '.',
                r.d ? 'x' : '.', r.descA ? 'A' : '.', r.descB ? 'B' : '.', contentClass(r), r.songRefs, r.phraseRefs,
                protectedBy(r), blockedBy(r), free ? "yes" : "no");
  }
  std::printf("free for phrase now: %d of %d | candidates to free (unprotected, not Song/Phrase-Bank referenced): %d\n",
              freeCount, kPatternsPerPage, candCount);
  for (int bars : {1, 2, 4, 8}) {
    std::printf("  contiguous %dB: now=%s  after-freeing-candidates=%s\n", bars,
                firstRun(freeNow, bars) >= 0 ? (std::string("start ") + std::to_string(firstRun(freeNow, bars))).c_str() : "NONE",
                firstRun(freeIfCandidates, bars) >= 0 ? (std::string("start ") + std::to_string(firstRun(freeIfCandidates, bars))).c_str() : "NONE");
  }
}

void clearSongRows(MiniAcid& e, int rows) {
  Song& song = e.sceneManager().currentScene().songs[0];
  for (int r = 0; r < rows; ++r)
    for (int t = 0; t < SongPosition::kTrackCount; ++t) song.positions[r].patterns[t] = -1;
  song.length = 1;
  song.positions[0].patterns[0] = song.positions[0].patterns[1] = song.positions[0].patterns[2] = -1;
}

const char* lifecycle(GeneratedPhraseSong::LifecycleStatus s) {
  using L = GeneratedPhraseSong::LifecycleStatus;
  switch (s) {
    case L::CommittedNow: return "CommittedNow"; case L::PendingNextBar: return "PendingNextBar";
    case L::Failed: return "Failed"; case L::Busy: return "Busy"; case L::TargetChanged: return "TargetChanged";
    default: return "?";
  }
}
const char* cycleName(GeneratedPhraseSong::CycleStatus s) {
  using S = GeneratedPhraseSong::CycleStatus;
  switch (s) {
    case S::CommittedNow: return "CommittedNow"; case S::NoSafeSlots: return "NoSafeSlots";
    case S::RowsOccupied: return "RowsOccupied"; case S::CycleAlreadyPublished: return "CycleAlreadyPublished";
    case S::NoRecipe: return "NoRecipe"; case S::NotAdmitted: return "NotAdmitted"; default: return "other";
  }
}
}  // namespace

int main() {
  SceneStorageSdl storage;
  MiniAcid engine(kRate, &storage);
  PatternPagingService::setProjectName("pml-a-inventory");
  PatternPagingService::clearProjectPages();
  engine.init();
  engine.setSongMode(false);
  Scene& sc = engine.sceneManager().currentScene();
  sc.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
  sc.genre.recipe = 0;
  sc.genre.rhythmSelectionMode = static_cast<uint8_t>(R::RhythmSelectionMode::Manual);
  sc.genre.rhythmArchetypeId = 404;
  sc.activeSongSlot = 0; sc.songs[0] = Song{}; sc.songs[1] = Song{}; sc.feel.patternBars = 1;
  for (int b = 0; b < kBankCount; ++b)
    for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
      sc.synthABanks[b].patterns[i] = SynthPattern{}; sc.synthBBanks[b].patterns[i] = SynthPattern{};
      sc.drumBanks[b].patterns[i] = DrumPatternSet{};
    }
  for (int v = 0; v < Scene::kMaterialVoices; ++v)
    for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) sc.materialSlots[v][s] = GroovePuterMaterial::MaterialSlotDescriptor{};
  engine.genreManager().setGenerativeMode(GenerativeMode::Techno);
  engine.genreManager().setRecipe(0);
  GroovePuterState::setGenerationLevel(R::RealizationLevel::P3Transformation);
  engine.setBpm(120.0f);

  std::printf("PML-A inventory. Isolated temporary project 'pml-a-inventory'; nothing is freed or deleted.\n");
  std::printf("page=%d slots=%d sizeof(SynthPattern)=%zu sizeof(DrumPatternSet)=%zu sizeof(Song)=%zu undoPayloadBytes=%zu\n",
              engine.currentPageIndex(), kPatternsPerPage, sizeof(SynthPattern), sizeof(DrumPatternSet), sizeof(Song),
              GroovePuterUndo::kUndoPayloadBytes);

  // --- Scenario: TAKE 4B, GROW, delete the Song rows, TAKE again, GROW again -----------------------------
  auto r1 = GeneratedPhraseSong::generate(engine, 4, 0, kGuard);
  std::printf("\n[1] TAKE 4B at row 1: %s\n", lifecycle(r1.status));
  auto c1 = GeneratedPhraseSong::generateCycle(engine, kGuard);
  std::printf("[2] GROW (D): %s bars=%d\n", cycleName(c1.status), c1.bars);
  table(engine, "after TAKE + GROW (A 4 + cycle up to 8)");

  clearSongRows(engine, 12);
  std::printf("\n[3] user deletes the Song rows (emulated: every row of Song 1 cleared, length=1)\n");
  table(engine, "after deleting the Song rows (content and descriptors remain)");

  auto r2 = GeneratedPhraseSong::generate(engine, 4, 0, kGuard);
  std::printf("\n[4] TAKE 4B again: %s error=%d\n", lifecycle(r2.status), static_cast<int>(r2.phrase.error));
  auto r3 = GeneratedPhraseSong::generate(engine, 4, 4, kGuard);
  std::printf("[5] TAKE 4B a third time: %s error=%d (PhraseError 5? see phrase_generator.h NoContiguousPatternSlots)\n",
              lifecycle(r3.status), static_cast<int>(r3.phrase.error));
  auto c2 = GeneratedPhraseSong::generateCycle(engine, kGuard);
  std::printf("[6] GROW after the second TAKE: %s\n", cycleName(c2.status));
  table(engine, "after the repeated TAKEs");

  std::printf("\nUndo/save boundary facts (from code, not from this run):\n");
  std::printf("  * one Undo receipt (%zu B). A Generation receipt stores the whole Song plus (pageIndex, firstLocalSlot, bars); it restores rows and clears the slots it created.\n",
              GroovePuterUndo::kUndoPayloadBytes);
  std::printf("  * restoring ONE freed slot needs %zu B of pattern content (2 synth + 1 drum set); %zu slots would fit next to a Song copy.\n",
              2 * sizeof(SynthPattern) + sizeof(DrumPatternSet),
              (GroovePuterUndo::kUndoPayloadBytes > sizeof(Song))
                  ? (GroovePuterUndo::kUndoPayloadBytes - sizeof(Song)) / (2 * sizeof(SynthPattern) + sizeof(DrumPatternSet)) : 0);
  return 0;
}
