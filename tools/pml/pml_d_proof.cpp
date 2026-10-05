// PML-D proof (host, no production change): which user scenarios does PML-C solve, and what blocks
// "GROW again"? Prototype steps (clearing the Undo receipt, resetting the cycle flag) are stand-ins
// used only to show what WOULD be needed; they are not production code.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/slot_reuse.h"
#include "src/state/generation_request_state.h"
#include "src/state/slot_content_token.h"

SerialMock Serial;
SDMock SD;

namespace {
namespace R = GroovePuterRhythm;
using GeneratedPhraseSong::CycleStatus;
using GeneratedPhraseSong::LifecycleStatus;
const auto kGuard = [](auto&& body) { body(); };
int g_fail = 0;
#define PROOF(cond, what)                                              \
  do {                                                                 \
    const bool ok_ = (cond);                                           \
    std::printf("  [%s] %s\n", ok_ ? "PASS" : "FAIL", what);           \
    if (!ok_) ++g_fail;                                                \
  } while (0)

struct Session {
  SceneStorageSdl storage;
  MiniAcid engine{44100.0f, &storage};
  Session(const char* project, GenerativeMode mode, uint16_t archetype) {
    PatternPagingService::setProjectName(project);
    PatternPagingService::clearProjectPages();
    engine.init();
    engine.setSongMode(false);
    Scene& sc = engine.sceneManager().currentScene();
    sc.genre.generativeMode = static_cast<uint8_t>(mode);
    sc.genre.recipe = 0;
    sc.genre.rhythmSelectionMode = static_cast<uint8_t>(archetype ? R::RhythmSelectionMode::Manual : R::RhythmSelectionMode::Auto);
    sc.genre.rhythmArchetypeId = archetype;
    sc.activeSongSlot = 0; sc.songs[0] = Song{}; sc.songs[1] = Song{}; sc.feel.patternBars = 1;
    for (int b = 0; b < kBankCount; ++b)
      for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
        sc.synthABanks[b].patterns[i] = SynthPattern{}; sc.synthBBanks[b].patterns[i] = SynthPattern{};
        sc.drumBanks[b].patterns[i] = DrumPatternSet{};
      }
    for (int v = 0; v < Scene::kMaterialVoices; ++v)
      for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) sc.materialSlots[v][s] = GroovePuterMaterial::MaterialSlotDescriptor{};
    engine.genreManager().setGenerativeMode(mode);
    engine.genreManager().setRecipe(0);
    engine.setBpm(124.0f);
    GroovePuterState::setGenerationLevel(R::RealizationLevel::P3Transformation);
    for (int i = 0; i < 3; ++i) engine.sceneManager().setCurrentBankIndex(i, 1);   // selectors away from slots 0-7
  }
  ~Session() { GroovePuterState::setGenerationLevel(R::RealizationLevel::P2Variation); }
  Scene& scene() { return engine.sceneManager().currentScene(); }
  Song& song() { return scene().songs[0]; }
  bool take(int bars, int row) {
    return GeneratedPhraseSong::generate(engine, static_cast<uint8_t>(bars), row, kGuard).status == LifecycleStatus::CommittedNow;
  }
  void clearRows(int from, int to) {
    for (int r = from; r < to; ++r)
      for (int t = 0; t < SongPosition::kTrackCount; ++t) song().positions[r].patterns[t] = -1;
    int len = 0;
    for (int r = 0; r < Song::kMaxPositions; ++r)
      for (int t = 0; t < SongPosition::kTrackCount; ++t) if (song().positions[r].patterns[t] >= 0) len = r + 1;
    song().length = len ? len : 1;
  }
  uint64_t token(int s) { return GroovePuterMaterial::slotContentToken(scene(), s); }
};

const char* cycleName(CycleStatus s) {
  switch (s) {
    case CycleStatus::CommittedNow: return "CommittedNow"; case CycleStatus::PendingNextBar: return "PendingNextBar";
    case CycleStatus::NoRecipe: return "NoRecipe"; case CycleStatus::CycleAlreadyPublished: return "CycleAlreadyPublished";
    case CycleStatus::NotAdmitted: return "NotAdmitted"; case CycleStatus::EditedSinceGeneration: return "EditedSinceGeneration";
    case CycleStatus::NoSafeSlots: return "NoSafeSlots"; case CycleStatus::RowsOccupied: return "RowsOccupied";
    case CycleStatus::NothingToAdd: return "NothingToAdd"; case CycleStatus::ContextChanged: return "ContextChanged";
    default: return "other";
  }
}

// A fresh session whose kept phrase A has a full 8-bar cycle (the cycle length depends on the identity).
bool makeFullCycle(std::unique_ptr<Session>& s, int attempt) {
  char name[48]; std::snprintf(name, sizeof(name), "pml-d-%d", attempt);
  s = std::make_unique<Session>(name, GenerativeMode::Techno, 404);
  if (!s->take(4, 0)) return false;
  const auto c = GeneratedPhraseSong::generateCycle(s->engine, kGuard);
  return c.status == CycleStatus::CommittedNow && c.bars == 8;
}
}  // namespace

int main() {
  std::printf("PML-D proof (isolated temporary projects; prototype steps are marked and are not production code)\n");
  std::unique_ptr<Session> s;
  int attempt = 0;
  while (!makeFullCycle(s, attempt) && ++attempt < 32) {}
  if (attempt >= 32) { std::puts("could not build a full 8-bar cycle"); return 2; }

  // ---------------------------------------------------------------- S1: regrow for the SAME A
  std::printf("\nS1: A kept in the Song, the cycle rows 4-11 deleted, user presses GROW again\n");
  uint64_t oldCycle[8];
  for (int i = 0; i < 8; ++i) oldCycle[i] = s->token(4 + i);
  s->clearRows(4, 12);
  auto c1 = GeneratedPhraseSong::generateCycle(s->engine, kGuard);
  std::printf("  GROW with the cycle rows gone: %s\n", cycleName(c1.status));
  PROOF(c1.status == CycleStatus::CycleAlreadyPublished, "blocker 1: the recipe still says a cycle was published (independent of slots)");
  s->engine.setGeneratedPhraseCycleStart(-1);                     // PROTOTYPE: forget the published cycle
  auto c2 = GeneratedPhraseSong::generateCycle(s->engine, kGuard);
  std::printf("  GROW after forgetting the cycle flag (prototype): %s\n", cycleName(c2.status));
  // F1 (found by the first run of this tool at e7f8d66f, fixed afterwards): the origin sidecar used to make a
  // repeated GROW report EditedSinceGeneration after a cycle. With the fix the blocker is the slots.
  PROOF(c2.status == CycleStatus::NoSafeSlots, "blocker 2: slots 4-11 still hold the old cycle and its Undo receipt (no false 'edited' after the F1 fix)");
  const auto pvS1 = SlotReuse::preview(s->engine);
  std::printf("  preview: longest run now=%d, after marks=%d; holders of slot 4: 0x%02x (receipt=0x%02x)\n", pvS1.longestNow, pvS1.longestAfter, pvS1.slotHolders[4], SlotReuse::kHolderUndoReceipt);
  PROOF(pvS1.slotHolders[4] & SlotReuse::kHolderUndoReceipt, "the slots of the deleted cycle are held only by the live Undo receipt");
  uint8_t held = 0;
  PROOF(SlotReuse::mark(s->engine, 4, &held) == SlotReuse::MarkResult::Protected, "a mark alone is refused for them (protection beats the mark)");
  GroovePuterUndo::undoOwner().clear();                           // PROTOTYPE: end the Undo of this cycle
  // FINDING: after GROW the CURRENT selector sits on the first cycle slot and protects it too.
  PROOF(SlotReuse::holders(s->engine, s->scene(), 4) & SlotReuse::kHolderCurrent, "finding: the first cycle slot is also CURRENT, so ending the receipt alone is not enough");
  for (int i = 0; i < 3; ++i) s->engine.sceneManager().setCurrentBankIndex(i, 1);
  for (int i = 0; i < 2; ++i) s->engine.sceneManager().setCurrentSynthPatternIndex(i, 7);
  s->engine.sceneManager().setCurrentDrumPatternIndex(7);        // the musician moves CURRENT to slot 15
  bool allMarked = true;
  for (int i = 4; i < 12; ++i) allMarked = allMarked && SlotReuse::mark(s->engine, i) == SlotReuse::MarkResult::Marked;
  PROOF(allMarked, "after the (prototype) end of that Undo, slots 4-11 can be marked");
  const auto pvS1b = SlotReuse::preview(s->engine);
  std::printf("  preview after marks: longest run after=%d  GROW 8B possible after=%s\n", pvS1b.longestAfter, pvS1b.grow8After ? "yes" : "no");
  PROOF(pvS1b.grow8After, "an 8-slot run appears once the receipt protection is ended and the slots are marked");
  auto c3 = GeneratedPhraseSong::generateCycle(s->engine, kGuard);
  std::printf("  GROW into the replaced slots: %s bars=%d\n", cycleName(c3.status), c3.bars);
  PROOF(c3.status == CycleStatus::CommittedNow && c3.bars == 8, "GROW succeeds");
  bool identical = true;
  for (int i = 0; i < 8; ++i) identical = identical && s->token(4 + i) == oldCycle[i];
  std::printf("  the regrown cycle is bit-identical to the one that was deleted: %s\n", identical ? "YES" : "no");
  PROOF(identical, "regrowing the same A yields the SAME cycle (pure function of the recipe): slots alone do not give the musician another one");

  // ---------------------------------------------------------------- S2: new TAKE, then GROW
  std::printf("\nS2: old A + cycle exist, user deletes every row, takes a NEW A, then presses GROW\n");
  std::unique_ptr<Session> t;
  attempt = 100;
  while (!makeFullCycle(t, attempt) && ++attempt < 140) {}
  t->clearRows(0, 12);
  PROOF(t->take(4, 0), "TAKE 4B (a new A) is committed on the only free run (slots 12-15)");
  const int newA = t->engine.generatedPhraseRecipe() ? t->engine.generatedPhraseRecipe()->firstLocalSlot : -1;
  std::printf("  new A occupies slots %d-%d\n", newA, newA + 3);
  auto g0 = GeneratedPhraseSong::generateCycle(t->engine, kGuard);
  std::printf("  GROW now: %s\n", cycleName(g0.status));
  PROOF(g0.status == CycleStatus::NoSafeSlots, "GROW is refused for lack of slots");
  const auto pv0 = SlotReuse::preview(t->engine);
  std::printf("  preview now: TAKE4 %s / GROW8 %s; after marks (none yet): TAKE4 %s / GROW8 %s\n",
              pv0.take4Now ? "yes" : "no", pv0.grow8Now ? "yes" : "no", pv0.take4After ? "yes" : "no", pv0.grow8After ? "yes" : "no");
  int marked = 0;
  for (int i = 0; i < 12; ++i) marked += SlotReuse::mark(t->engine, i) == SlotReuse::MarkResult::Marked ? 1 : 0;
  std::printf("  the musician allows replacement of the unused material: %d of slots 0-11 marked\n", marked);
  PROOF(marked == 12, "all twelve orphan slots (no Song reference, not CURRENT, not in the receipt) can be marked");
  const auto pv1 = SlotReuse::preview(t->engine);
  std::printf("  preview after marks: longest now=%d after=%d  GROW 8B now=%s after=%s\n", pv1.longestNow, pv1.longestAfter, pv1.grow8Now ? "yes" : "no", pv1.grow8After ? "yes" : "no");
  PROOF(!pv1.grow8Now && pv1.grow8After, "the preview separates 'permission set' (after) from 'generation possible now'");
  auto g1 = GeneratedPhraseSong::generateCycle(t->engine, kGuard);
  std::printf("  GROW: %s bars=%d\n", cycleName(g1.status), g1.bars);
  PROOF(g1.status == CycleStatus::CommittedNow, "PML-C alone makes GROW possible for a new A (no receipt action needed)");
  PROOF(GeneratedPhraseSong::undoLastGeneratedPhrase(t->engine, kGuard) == GroovePuterUndo::UndoResult::Restored, "Undo of that GROW reports Restored");
  PROOF(t->song().positions[0].patterns[0] >= 0, "the new A is still in the Song after that Undo");

  // ---------------------------------------------------------------- S3: House path
  std::printf("\nS3: House (auto style), four 4B TAKEs fill the page, rows deleted, a fifth TAKE, then GROW\n");
  Session h("pml-d-house", GenerativeMode::House, 0);
  bool fourOk = true;
  for (int i = 0; i < 4; ++i) { fourOk = fourOk && h.take(4, i * 4); if (i < 3) h.clearRows(0, 16); }
  PROOF(fourOk, "four House TAKEs committed");
  h.clearRows(0, 16);
  const bool fifth = h.take(4, 0);
  PROOF(!fifth, "a fifth TAKE is refused with an empty Song (no consecutive empty slots)");
  int hm = 0;
  for (int i = 0; i < 12; ++i) hm += SlotReuse::mark(h.engine, i) == SlotReuse::MarkResult::Marked ? 1 : 0;
  PROOF(hm == 12, "twelve orphan slots can be marked");
  PROOF(h.take(4, 0), "after allowing replacement the fifth TAKE is committed");
  auto hg = GeneratedPhraseSong::generateCycle(h.engine, kGuard);
  std::printf("  GROW on a House TAKE: %s\n", cycleName(hg.status));
  PROOF(hg.status == CycleStatus::NotAdmitted, "House cannot GROW at all: slots are not the question for this style");

  std::printf("\nS3b: Techno (admitted), manual edit of the kept phrase, then GROW\n");
  std::unique_ptr<Session> e;
  attempt = 200;
  while (!makeFullCycle(e, attempt) && ++attempt < 240) {}
  e->clearRows(0, 12);
  e->engine.clearGeneratedPhraseRecipe();
  PROOF(e->take(4, 0), "a fresh TAKE");
  e->scene().synthABanks[e->engine.generatedPhraseRecipe()->firstLocalSlot / 8].patterns[e->engine.generatedPhraseRecipe()->firstLocalSlot % 8].steps[2].velocity ^= 0x21;
  auto eg = GeneratedPhraseSong::generateCycle(e->engine, kGuard);
  std::printf("  GROW after a manual edit: %s\n", cycleName(eg.status));
  PROOF(eg.status == CycleStatus::EditedSinceGeneration, "an edited kept phrase cannot GROW (R0 boundary), whatever the slots");

  std::printf("\nRESULT: %s (%d failing checks)\n", g_fail ? "FAIL" : "ALL PROOFS HOLD", g_fail);
  return g_fail ? 1 : 0;
}
