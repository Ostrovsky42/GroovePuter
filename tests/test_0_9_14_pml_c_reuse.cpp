// 0.9.14 PML-C: session-only "allow replacement" of Pattern slots.
// Real engine, real GeneratedPhraseSong transaction, real Undo owner, real page save/load.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/slot_reuse.h"
#include "src/state/generation_request_state.h"
#include "src/phrase/phrase_core.h"
#include "src/state/slot_content_token.h"
#include "src/state/undo_receipts.h"

SerialMock Serial;
SDMock SD;

namespace {

namespace R = GroovePuterRhythm;
using GeneratedPhraseSong::CycleStatus;
using GeneratedPhraseSong::LifecycleStatus;
using GroovePuterMaterial::slotContentToken;
using SlotReuse::Holder;
using SlotReuse::MarkResult;
using SlotReuse::Verdict;

const auto kGuard = [](auto&& body) { body(); };

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__,  \
                   #cond);                                                   \
      std::abort();                                                          \
    }                                                                        \
  } while (0)

struct Fixture {
  SceneStorageSdl storage;
  MiniAcid engine{44100.0f, &storage};
  explicit Fixture(const char* project) {
    CHECK(PatternPagingService::setProjectName(project));
    CHECK(PatternPagingService::clearProjectPages());
    engine.init();
    engine.setSongMode(false);
    Scene& scene = engine.sceneManager().currentScene();
    scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
    scene.genre.recipe = 0;
    scene.genre.rhythmSelectionMode = static_cast<uint8_t>(R::RhythmSelectionMode::Manual);
    scene.genre.rhythmArchetypeId = 404;
    scene.activeSongSlot = 0;
    scene.songs[0] = Song{};
    scene.songs[1] = Song{};
    scene.feel.patternBars = 1;
    for (int b = 0; b < kBankCount; ++b) {
      for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
        scene.synthABanks[b].patterns[i] = SynthPattern{};
        scene.synthBBanks[b].patterns[i] = SynthPattern{};
        scene.drumBanks[b].patterns[i] = DrumPatternSet{};
      }
    }
    for (int v = 0; v < Scene::kMaterialVoices; ++v)
      for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s)
        scene.materialSlots[v][s] = GroovePuterMaterial::MaterialSlotDescriptor{};
    engine.genreManager().setGenerativeMode(GenerativeMode::Techno);
    engine.genreManager().setRecipe(0);
    engine.setBpm(120.0f);
    GroovePuterState::setGenerationLevel(R::RealizationLevel::P3Transformation);
    // selectors away from the slots under test: CURRENT is a holder and must not hide the others
    engine.sceneManager().setCurrentBankIndex(0, 1);
    engine.sceneManager().setCurrentBankIndex(1, 1);
    engine.sceneManager().setCurrentBankIndex(2, 1);
  }
  ~Fixture() { GroovePuterState::setGenerationLevel(R::RealizationLevel::P2Variation); }
  Scene& scene() { return engine.sceneManager().currentScene(); }
  Song& song() { return scene().songs[0]; }
  bool take(int bars, int row = 0) {
    return GeneratedPhraseSong::generate(engine, static_cast<uint8_t>(bars), row, kGuard).status ==
           LifecycleStatus::CommittedNow;
  }
  void clearSongRows() {
    for (int r = 0; r < Song::kMaxPositions; ++r)
      for (int t = 0; t < SongPosition::kTrackCount; ++t) song().positions[r].patterns[t] = -1;
    song().length = 1;
  }
  uint64_t token(int slot) { return slotContentToken(scene(), slot); }
  bool empty(int slot) { return PhraseGenerator::localSlotIsEmpty(scene(), slot); }
};

// Four 4B TAKEs fill the page (slots 0-15), the Song rows are deleted: slots 0-11 are unreferenced
// orphans, slots 12-15 belong to the live generation receipt. Deterministic (no cycle: its length
// depends on the identity).
void buildOrphans(Fixture& f) {
  for (int i = 0; i < 4; ++i) {
    CHECK(f.take(4, i * 4));
    if (i < 3) f.clearSongRows();
  }
  f.clearSongRows();
  CHECK(f.scene().songs[0].length == 1);
  for (int s = 0; s < 16; ++s) CHECK(!f.empty(s));
}

void testMarkProtectionAndRefusals() {
  Fixture f("pmlc-mark");
  CHECK(f.take(4, 0));
  // slot 0 is referenced by the Song: protected, cannot be marked
  uint8_t held = 0;
  CHECK(SlotReuse::mark(f.engine, 0, &held) == MarkResult::Protected);
  CHECK(held & SlotReuse::kHolderSong);
  CHECK(f.engine.reuseMarks().count() == 0);
  // an empty slot needs no mark
  CHECK(SlotReuse::mark(f.engine, 9) == MarkResult::NothingToReplace);
  CHECK(SlotReuse::mark(f.engine, 99) == MarkResult::OutOfRange);

  f.clearSongRows();
  // CURRENT selects slot 0: protected by CURRENT even though nothing references it
  f.engine.sceneManager().setCurrentBankIndex(0, 0);
  f.engine.sceneManager().setCurrentBankIndex(1, 0);
  f.engine.sceneManager().setCurrentBankIndex(2, 0);
  f.engine.sceneManager().setCurrentSynthPatternIndex(0, 0);
  f.engine.sceneManager().setCurrentSynthPatternIndex(1, 0);
  f.engine.sceneManager().setCurrentDrumPatternIndex(0);
  held = 0;
  CHECK(SlotReuse::mark(f.engine, 0, &held) == MarkResult::Protected);
  CHECK(held & SlotReuse::kHolderCurrent);
  f.engine.sceneManager().setCurrentBankIndex(0, 1);
  f.engine.sceneManager().setCurrentBankIndex(1, 1);
  f.engine.sceneManager().setCurrentBankIndex(2, 1);

  // still inside the live generation receipt (slots 0-3): protected although nothing references it
  held = 0;
  CHECK(SlotReuse::mark(f.engine, 1, &held) == MarkResult::Protected);
  CHECK(held & SlotReuse::kHolderUndoReceipt);
  // a second TAKE moves the receipt to 4-7; slot 1 is then an unreferenced, unprotected orphan
  CHECK(f.take(4, 4));
  f.clearSongRows();
  CHECK(SlotReuse::mark(f.engine, 1) == MarkResult::Marked);
  CHECK(f.engine.reuseMarks().marked(1));
  CHECK(SlotReuse::verify(f.engine, f.scene(), 1) == Verdict::Ok);
  SlotReuse::unmark(f.engine, 1);
  CHECK(!f.engine.reuseMarks().marked(1));
  CHECK(SlotReuse::verify(f.engine, f.scene(), 1) == Verdict::NotMarked);
  std::printf("PML-C: mark refuses protected/free slots, unmark works; ReuseMarks = %zu B\n",
              sizeof(GroovePuterMaterial::ReuseMarks));
}

void testEveryHolderProtects() {
  Fixture f("pmlc-holders");
  buildOrphans(f);
  const int slot = 1;                        // an orphan: content, no Song reference
  CHECK(SlotReuse::mark(f.engine, slot) == MarkResult::Marked);
  CHECK(SlotReuse::verify(f.engine, f.scene(), slot) == Verdict::Ok);
  const int global = songPatternFromPageBankIndex(0, 0, slot);

  // Song reference (either song) revokes nothing but excludes reuse
  f.song().positions[5].patterns[0] = static_cast<int16_t>(global);
  CHECK(SlotReuse::verify(f.engine, f.scene(), slot) == Verdict::Protected);
  CHECK(SlotReuse::holders(f.engine, f.scene(), slot) & SlotReuse::kHolderSong);
  f.song().positions[5].patterns[0] = -1;
  f.scene().songs[1].positions[2].patterns[1] = static_cast<int16_t>(global);
  CHECK(SlotReuse::holders(f.engine, f.scene(), slot) & SlotReuse::kHolderSong);
  f.scene().songs[1].positions[2].patterns[1] = -1;
  CHECK(SlotReuse::verify(f.engine, f.scene(), slot) == Verdict::Ok);

  // Phrase Bank reference view
  f.song().positions[0].patterns[0] = f.song().positions[0].patterns[1] =
      f.song().positions[0].patterns[2] = static_cast<int16_t>(global);
  f.song().length = 1;
  CHECK(PhraseCore::captureSongRegion(f.scene().phraseBank, PhraseCore::SlotId::A, f.song(), 0, 0, 1,
                                      PhraseCore::Role::Main, PhraseCore::Source::InternalPattern).error ==
        PhraseCore::Error::None);
  f.clearSongRows();
  CHECK(SlotReuse::holders(f.engine, f.scene(), slot) & SlotReuse::kHolderPhraseBank);
  CHECK(SlotReuse::verify(f.engine, f.scene(), slot) == Verdict::Protected);
  PhraseCore::reset(f.scene().phraseBank);
  CHECK(SlotReuse::verify(f.engine, f.scene(), slot) == Verdict::Ok);

  // CURRENT
  f.engine.sceneManager().setCurrentBankIndex(2, 0);
  f.engine.sceneManager().setCurrentDrumPatternIndex(slot);
  CHECK(SlotReuse::holders(f.engine, f.scene(), slot) & SlotReuse::kHolderCurrent);
  f.engine.sceneManager().setCurrentBankIndex(2, 1);
  CHECK(SlotReuse::verify(f.engine, f.scene(), slot) == Verdict::Ok);

  // NEXT queued on the slot
  f.engine.pendingMaterial_[0].queued = true;
  f.engine.pendingMaterial_[0].slot = static_cast<uint16_t>(global);
  CHECK(SlotReuse::holders(f.engine, f.scene(), slot) & SlotReuse::kHolderNext);
  f.engine.pendingMaterial_[0].queued = false;

  // Melody descriptor
  f.scene().materialSlots[1][slot].kind = GroovePuterMaterial::MaterialKind::Melody;
  CHECK(SlotReuse::holders(f.engine, f.scene(), slot) & SlotReuse::kHolderMelody);
  CHECK(SlotReuse::verify(f.engine, f.scene(), slot) == Verdict::Protected);
  f.scene().materialSlots[1][slot] = GroovePuterMaterial::MaterialSlotDescriptor{};
  CHECK(SlotReuse::verify(f.engine, f.scene(), slot) == Verdict::Ok);

  // the live generation receipt belongs to the last TAKE (slots 12-15)
  int first = 0, bars = 0;
  CHECK(SlotReuse::undoReceiptRange(first, bars) && bars == 4 && first == 12);
  for (int s = first; s < first + bars; ++s) {
    CHECK(SlotReuse::holders(f.engine, f.scene(), s) & SlotReuse::kHolderUndoReceipt);
  }
  std::puts("PML-C: Song, Phrase Bank, CURRENT, NEXT, Melody and the Undo receipt each exclude reuse: PASS");
}

void testEditRevokes() {
  Fixture f("pmlc-edit");
  buildOrphans(f);
  CHECK(SlotReuse::mark(f.engine, 2) == MarkResult::Marked);
  const uint64_t before = f.token(2);
  f.scene().synthABanks[0].patterns[2].steps[5].velocity ^= 0x11;     // manual edit
  CHECK(f.token(2) != before);
  CHECK(SlotReuse::verify(f.engine, f.scene(), 2) == Verdict::Edited);
  const uint64_t editedToken = f.token(2);
  // an edited slot is not usable, the mark ends, content is untouched
  CHECK(!SlotReuse::usable(f.engine, f.scene(), 2));
  CHECK(!f.engine.reuseMarks().marked(2));
  CHECK(f.token(2) == editedToken);
  // revert does not resurrect a revoked mark
  f.scene().synthABanks[0].patterns[2].steps[5].velocity ^= 0x11;
  CHECK(SlotReuse::verify(f.engine, f.scene(), 2) == Verdict::NotMarked);
  std::puts("PML-C: a manual edit revokes the mark; content untouched; no resurrection: PASS");
}

void testPreferenceAndReplacement() {
  Fixture f("pmlc-replace");
  buildOrphans(f);                           // 12-15 hold the latest TAKE (rows deleted), 0-11 orphans
  // nothing is free now: a TAKE fails and changes nothing
  uint64_t old[16];
  for (int s = 0; s < 16; ++s) old[s] = f.token(s);
  CHECK(!f.take(4, 0));
  for (int s = 0; s < 16; ++s) CHECK(f.token(s) == old[s]);
  CHECK(!SlotReuse::preview(f.engine).take4Now);

  // mark 0-3 (a B descriptor is planted to prove both descriptors are reclaimed)
  f.scene().materialSlots[1][2].id = GroovePuterMaterial::MaterialId{777};
  for (int s = 0; s < 4; ++s) CHECK(SlotReuse::mark(f.engine, s) == MarkResult::Marked);
  const auto pv = SlotReuse::preview(f.engine);
  CHECK(!pv.take4Now && pv.take4After);          // "TAKE 4B: not now, after the permitted replacement: yes"
  CHECK(!pv.grow8Now && !pv.grow8After);         // GROW needs 8 in a row: not available
  CHECK(pv.longestNow == 0 && pv.longestAfter == 4);

  CHECK(f.take(4, 0));                           // goes to the marked run
  const auto* recipe = f.engine.generatedPhraseRecipe();
  CHECK(recipe != nullptr && recipe->firstLocalSlot == 0);
  for (int s = 0; s < 4; ++s) {
    CHECK(f.token(s) != old[s]);                 // replaced
    CHECK(!f.engine.reuseMarks().marked(s));     // mark consumed
    CHECK(f.scene().materialSlots[0][s].id.valid());   // new canonical id
    CHECK(!f.scene().materialSlots[1][s].id.valid());  // no stale Synth B id
  }
  for (int s = 4; s < 16; ++s) CHECK(f.token(s) == old[s]);   // nothing else touched
  std::puts("PML-C: marked run used only when nothing is free; both descriptors reclaimed; mark consumed; rest untouched: PASS");

  // free-now slots are preferred over marked ones
  Fixture g("pmlc-prefer");
  buildOrphans(g);
  CHECK(g.take(4, 0) == false);
  // free a run the ordinary way: clear slots 12-15 completely (user deleted the material)
  for (int s = 12; s < 16; ++s) SlotReuse::reclaim(g.engine, g.scene(), s);
  CHECK(SlotReuse::mark(g.engine, 0) == MarkResult::Marked);
  CHECK(SlotReuse::mark(g.engine, 1) == MarkResult::Marked);
  CHECK(SlotReuse::mark(g.engine, 2) == MarkResult::Marked);
  CHECK(SlotReuse::mark(g.engine, 3) == MarkResult::Marked);
  CHECK(g.take(4, 0));
  CHECK(g.engine.generatedPhraseRecipe()->firstLocalSlot == 12);   // the free run, not the marked one
  for (int s = 0; s < 4; ++s) CHECK(g.engine.reuseMarks().marked(s));  // marks untouched
  std::puts("PML-C: a run that is free now is preferred; marks stay until needed: PASS");
}

void testRefusalsDoNotMutate() {
  Fixture f("pmlc-refuse");
  buildOrphans(f);
  for (int s = 0; s < 4; ++s) CHECK(SlotReuse::mark(f.engine, s) == MarkResult::Marked);
  f.scene().synthABanks[0].patterns[1].steps[0].velocity ^= 0x05;   // slot 1 edited: run 0-3 is broken
  uint64_t snap[16];
  for (int s = 0; s < 16; ++s) snap[s] = f.token(s);
  CHECK(!f.take(4, 0));                                             // no run of 4 usable
  for (int s = 0; s < 16; ++s) CHECK(f.token(s) == snap[s]);
  CHECK(f.engine.generatedPhraseRecipe() != nullptr);               // nothing published
  CHECK(!f.engine.reuseMarks().marked(1));                          // the edited mark was revoked
  CHECK(f.engine.reuseMarks().marked(0));                           // the others are untouched
  CHECK(!GeneratedPhraseSong::generate(f.engine, 3, 0, kGuard));    // unsupported length
  for (int s = 0; s < 16; ++s) CHECK(f.token(s) == snap[s]);
  std::puts("PML-C: refusals (edited mark, no run, bad length) change no slot: PASS");
}

// The Undo gate: replacing marked slots must never let Undo bring back rows that point at empty
// or foreign content, in either Song or the Phrase Bank.
int danglingReferences(Fixture& f) {
  int dangling = 0;
  for (int songSlot = 0; songSlot < 2; ++songSlot)
    for (int r = 0; r < Song::kMaxPositions; ++r)
      for (int t = 0; t < SongPosition::kTrackCount; ++t) {
        const int g = f.scene().songs[songSlot].positions[r].patterns[t];
        if (g < 0 || songPatternPage(g) != 0) continue;
        const int local = songPatternBank(g) * Bank<SynthPattern>::kPatterns + songPatternIndexInBank(g);
        if (f.empty(local)) ++dangling;
      }
  for (int slot = 0; slot < PhraseCore::kSlotCount; ++slot) {
    const auto& phrase = f.scene().phraseBank.slots[slot];
    if ((phrase.metadata.flags & PhraseCore::kFlagValid) == 0) continue;
    for (int bar = 0; bar < phrase.metadata.lengthBars; ++bar)
      for (int t = 0; t < PhraseCore::kTrackCount; ++t) {
        const int g = phrase.patternRefs[bar][t];
        if (g < 0 || songPatternPage(g) != 0) continue;
        const int local = songPatternBank(g) * Bank<SynthPattern>::kPatterns + songPatternIndexInBank(g);
        if (f.empty(local)) ++dangling;
      }
  }
  return dangling;
}

void testUndoGate() {
  Fixture f("pmlc-undo");
  buildOrphans(f);

  // Put a real Song Undo receipt in the owner, exactly as a "delete rows" edit would leave it: its
  // snapshot references the orphan slots 0-3.
  GroovePuterUndo::SongUndoPayload songReceipt{};
  songReceipt.pageIndex = 0;
  songReceipt.songSlot = 0;
  songReceipt.before = Song{};
  for (int r = 0; r < 4; ++r)
    for (int t = 0; t < 3; ++t) songReceipt.before.positions[r].patterns[t] = static_cast<int16_t>(r);
  songReceipt.before.length = 4;
  CHECK(GroovePuterUndo::undoOwner().commitPrepared(GroovePuterUndo::UndoKind::Song, songReceipt, [] {}));
  CHECK(GroovePuterUndo::undoOwner().kind() == GroovePuterUndo::UndoKind::Song);

  const int danglingBefore = danglingReferences(f);
  for (int s = 0; s < 4; ++s) CHECK(SlotReuse::mark(f.engine, s) == MarkResult::Marked);
  CHECK(f.take(4, 0));                                        // replaces slots 0-3

  // The single Undo slot now belongs to the replacing generation: the Song snapshot is gone, so
  // Undo can no longer restore rows 0-3 onto the replaced slots.
  CHECK(GroovePuterUndo::undoOwner().kind() == GroovePuterUndo::UndoKind::Generation);
  GroovePuterUndo::SongUndoPayload probe{};
  CHECK(!GroovePuterUndo::undoOwner().read(GroovePuterUndo::UndoKind::Song, probe));
  CHECK(danglingReferences(f) == danglingBefore);
  const uint64_t replaced[4] = {f.token(0), f.token(1), f.token(2), f.token(3)};

  // Undo of the replacing generation: rows come back as they were at that moment, slots end empty.
  CHECK(GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard) ==
        GroovePuterUndo::UndoResult::Restored);
  for (int s = 0; s < 4; ++s) CHECK(f.empty(s) && f.token(s) != replaced[s]);
  CHECK(danglingReferences(f) == danglingBefore);
  CHECK(f.scene().songs[0].length == 1);
  std::puts("PML-C Undo gate: the replacing generation owns the only receipt; Undo adds no row that points at empty or foreign content: PASS");
}

void testSaveLoadAfterReplacement() {
  Fixture f("pmlc-save");
  buildOrphans(f);
  const uint64_t oldTokens[4] = {f.token(0), f.token(1), f.token(2), f.token(3)};
  for (int s = 0; s < 4; ++s) CHECK(SlotReuse::mark(f.engine, s) == MarkResult::Marked);
  CHECK(f.take(4, 0));
  CHECK(PatternPagingService::savePage(0, f.scene()));
  auto fresh = std::make_unique<Scene>();
  CHECK(PatternPagingService::loadPage(0, *fresh));
  for (int s = 0; s < kPatternsPerPage; ++s) {
    CHECK(slotContentToken(*fresh, s) == f.token(s));
    for (int o = 0; o < 4; ++o) CHECK(slotContentToken(*fresh, s) != oldTokens[o]);   // old content is gone
  }
  CHECK(fresh->materialSlots[0][0].id == f.scene().materialSlots[0][0].id);
  std::puts("PML-C: after replacement, save/load reproduces the new content and no old content remains: PASS");
}

void testSessionBoundaries() {
  Fixture f("pmlc-session");
  buildOrphans(f);
  CHECK(SlotReuse::mark(f.engine, 1) == MarkResult::Marked);
  f.engine.setCurrentPage(static_cast<int8_t>(1));
  CHECK(f.engine.reuseMarks().count() == 0);                  // a page change drops the marks
  f.engine.setCurrentPage(static_cast<int8_t>(0));
  CHECK(SlotReuse::mark(f.engine, 1) == MarkResult::Marked);
  CHECK(f.engine.createNewSceneWithName("pmlc-session-new"));
  CHECK(f.engine.reuseMarks().count() == 0);                  // a new scene drops the marks
  // a wipe that does not go through the engine leaves the mark inert: token and id no longer match
  Fixture g("pmlc-session2");
  buildOrphans(g);
  CHECK(SlotReuse::mark(g.engine, 1) == MarkResult::Marked);
  g.engine.sceneManager().wipeToZero();
  CHECK(SlotReuse::verify(g.engine, g.scene(), 1) == Verdict::Edited);
  std::puts("PML-C: page change / new scene drop marks; an engine-bypassing wipe leaves an inert, revoked mark: PASS");
}

}  // namespace

int main() {
  testMarkProtectionAndRefusals();
  testEveryHolderProtects();
  testEditRevokes();
  testPreferenceAndReplacement();
  testRefusalsDoNotMutate();
  testUndoGate();
  testSaveLoadAfterReplacement();
  testSessionBoundaries();
  std::puts("0.9.14 PML-C reuse: PASS");
  return 0;
}
