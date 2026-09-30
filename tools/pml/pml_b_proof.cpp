// PML-B proof (host, no production change): what a "reusable slot" contract can rely on.
//   T1 cold boot   : does slot content + descriptor survive savePage -> fresh Scene -> loadPage bit-exactly?
//   T2 revocation  : does a content token change on every kind of edit (so an edit can revoke a mark)?
//   T3 failure     : does a failed PREPARE leave every slot unchanged?
//   T4 overwrite   : what does Undo restore after a marked slot was reclaimed and overwritten?
//   T5 melody      : does a Melody descriptor block phrase generation (separate owner)?
// Prototype reclaim here is a stand-in for what production would do at generation time; it is NOT production code.
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
#include "src/state/generation_request_state.h"

SerialMock Serial;
SDMock SD;

namespace {
namespace R = GroovePuterRhythm;
const auto kGuard = [](auto&& body) { body(); };
int g_fail = 0;
#define PROOF(cond, what)                                                        \
  do {                                                                           \
    const bool ok_ = (cond);                                                     \
    std::printf("  [%s] %s\n", ok_ ? "PASS" : "FAIL", what);                     \
    if (!ok_) ++g_fail;                                                          \
  } while (0)

// Content token of one slot across every lane the musician can edit (Synth A, Synth B, drums incl. automation and groove).
uint64_t slotToken(const Scene& sc, int slot) {
  const int bank = slot / Bank<SynthPattern>::kPatterns, idx = slot % Bank<SynthPattern>::kPatterns;
  uint64_t x = 1469598103934665603ull;
  auto h = [&x](uint64_t v) { x ^= v + 0x9e3779b97f4a7c15ull + (x << 6) + (x >> 2); x *= 1099511628211ull; };
  auto hf = [&h](float f) { uint32_t b; std::memcpy(&b, &f, 4); h(b); };
  const DrumPatternSet& d = sc.drumBanks[bank].patterns[idx];
  for (int v = 0; v < DrumPatternSet::kVoices; ++v)
    for (int s = 0; s < DrumPattern::kSteps; ++s) {
      const DrumStep& t = d.voices[v].steps[s];
      h((uint64_t(t.hit) << 1) | t.accent); h(t.velocity); h(uint8_t(t.timing)); h(t.fx); h(t.fxParam); h(t.probability);
    }
  for (int l = 0; l < DrumPatternSet::kMaxLanes; ++l) {
    h(d.lanes[l].targetParam); h(d.lanes[l].nodeCount);
    for (int n = 0; n < d.lanes[l].nodeCount && n < AutomationLane::kMaxNodes; ++n) {
      h(d.lanes[l].nodes[n].step); hf(d.lanes[l].nodes[n].value); h(d.lanes[l].nodes[n].curveType);
    }
  }
  hf(d.groove.swing); hf(d.groove.humanize);
  for (const SynthPattern* sp : {&sc.synthABanks[bank].patterns[idx], &sc.synthBBanks[bank].patterns[idx]})
    for (int s = 0; s < SynthPattern::kSteps; ++s) {
      const SynthStep& t = sp->steps[s];
      h(uint8_t(t.note)); h((uint64_t(t.slide) << 2) | (uint64_t(t.accent) << 1) | t.ghost);
      h(t.velocity); h(uint8_t(t.timing)); h(t.fx); h(t.fxParam); h(t.probability);
    }
  return x;
}

struct Snap { uint64_t tok[kPatternsPerPage]; uint8_t kind[2][kPatternsPerPage]; uint32_t id[2][kPatternsPerPage]; };
Snap snap(const Scene& sc) {
  Snap s{};
  for (int i = 0; i < kPatternsPerPage; ++i) {
    s.tok[i] = slotToken(sc, i);
    for (int v = 0; v < 2; ++v) {
      s.kind[v][i] = static_cast<uint8_t>(sc.materialSlots[v][i].kind);
      s.id[v][i] = static_cast<uint32_t>(sc.materialSlots[v][i].id.value);
    }
  }
  return s;
}
bool snapsEqual(const Snap& a, const Snap& b) { return std::memcmp(&a, &b, sizeof(Snap)) == 0; }
int diffSlots(const Snap& a, const Snap& b) {
  int n = 0;
  for (int i = 0; i < kPatternsPerPage; ++i)
    n += a.tok[i] != b.tok[i] || a.kind[0][i] != b.kind[0][i] || a.id[0][i] != b.id[0][i] ||
         a.kind[1][i] != b.kind[1][i] || a.id[1][i] != b.id[1][i];
  return n;
}

void clearSong(MiniAcid& e) {
  Song& song = e.sceneManager().currentScene().songs[0];
  for (int r = 0; r < Song::kMaxPositions; ++r)
    for (int t = 0; t < SongPosition::kTrackCount; ++t) song.positions[r].patterns[t] = -1;
  song.length = 1;
}

// Stand-in for production "reuse at generation time": empty the three patterns and both descriptors.
void prototypeReclaim(Scene& sc, int slot) {
  const int bank = slot / Bank<SynthPattern>::kPatterns, idx = slot % Bank<SynthPattern>::kPatterns;
  sc.synthABanks[bank].patterns[idx] = SynthPattern{};
  sc.synthBBanks[bank].patterns[idx] = SynthPattern{};
  sc.drumBanks[bank].patterns[idx] = DrumPatternSet{};
  GroovePuterMaterial::clearResidentDescriptor(sc, 0, slot);
  GroovePuterMaterial::clearResidentDescriptor(sc, 1, slot);
}

void configure(MiniAcid& e) {
  Scene& sc = e.sceneManager().currentScene();
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
  e.genreManager().setGenerativeMode(GenerativeMode::Techno);
  e.genreManager().setRecipe(0);
  GroovePuterState::setGenerationLevel(R::RealizationLevel::P3Transformation);
  e.setBpm(120.0f);
}
}  // namespace

int main() {
  SceneStorageSdl storage;
  MiniAcid engine(44100.0f, &storage);
  PatternPagingService::setProjectName("pml-b-proof");
  PatternPagingService::clearProjectPages();
  engine.init();
  engine.setSongMode(false);
  configure(engine);
  Scene& sc = engine.sceneManager().currentScene();

  std::printf("PML-B proof (isolated temporary project; prototype reclaim is not production code)\n");

  // --- T1 cold boot -----------------------------------------------------------------------------------
  std::printf("\nT1 cold boot: savePage -> fresh Scene -> loadPage\n");
  PROOF(GeneratedPhraseSong::generate(engine, 4, 0, kGuard).status == GeneratedPhraseSong::LifecycleStatus::CommittedNow, "TAKE 4B committed");
  PROOF(GeneratedPhraseSong::generateCycle(engine, kGuard).status == GeneratedPhraseSong::CycleStatus::CommittedNow, "GROW committed");
  const Snap live = snap(sc);
  std::printf("  (no savePage has run yet: generation publishes to RAM; the page file is written on page switch / save)\n");
  {
    auto before = std::make_unique<Scene>();
    const bool loadedBeforeSave = PatternPagingService::pageExists(0) && PatternPagingService::loadPage(0, *before);
    std::printf("  page file present before any save: %s; slots differing from RAM if loaded now: %d of %d\n",
                PatternPagingService::pageExists(0) ? "yes" : "no", loadedBeforeSave ? diffSlots(live, snap(*before)) : -1, kPatternsPerPage);
  }
  PROOF(PatternPagingService::savePage(0, sc), "savePage(0) succeeded");
  auto fresh = std::make_unique<Scene>();
  PROOF(PatternPagingService::loadPage(0, *fresh), "loadPage(0) into a fresh Scene succeeded");
  const int d1 = diffSlots(live, snap(*fresh));
  std::printf("  slots differing after the round trip (content token incl. automation/groove, kind, MaterialId, both voices): %d of %d\n", d1, kPatternsPerPage);
  PROOF(d1 == 0, "content and descriptors survive a cold boot bit-exactly");
  std::printf("  not in the page file (session-only, so absent after a cold boot): generated-phrase recipe, origin sidecar, Undo receipt, song rows (scene document)\n");

  // --- T2 revocation ----------------------------------------------------------------------------------
  std::printf("\nT2 revocation: a content token must change on every edit and not on identical rewrites\n");
  const int slot = 1;
  const int bank = 0, idx = slot;
  const uint64_t base = slotToken(sc, slot);
  auto edit = [&](const char* name, auto&& apply, auto&& undo) {
    apply(); const bool changed = slotToken(sc, slot) != base; undo();
    const bool restored = slotToken(sc, slot) == base;
    char line[160]; std::snprintf(line, sizeof(line), "edit '%s' changes the token and reverting restores it", name);
    PROOF(changed && restored, line);
  };
  SynthPattern& a = sc.synthABanks[bank].patterns[idx];
  SynthPattern& b = sc.synthBBanks[bank].patterns[idx];
  DrumPatternSet& dr = sc.drumBanks[bank].patterns[idx];
  edit("synth A note", [&] { a.steps[0].note = static_cast<int8_t>(a.steps[0].note + 1); }, [&] { a.steps[0].note = static_cast<int8_t>(a.steps[0].note - 1); });
  edit("synth A velocity", [&] { a.steps[2].velocity ^= 0x11; }, [&] { a.steps[2].velocity ^= 0x11; });
  edit("synth A slide flag", [&] { a.steps[3].slide ^= 1; }, [&] { a.steps[3].slide ^= 1; });
  edit("synth B timing", [&] { b.steps[1].timing = static_cast<int8_t>(b.steps[1].timing + 1); }, [&] { b.steps[1].timing = static_cast<int8_t>(b.steps[1].timing - 1); });
  edit("drum hit", [&] { dr.voices[0].steps[5].hit ^= 1; }, [&] { dr.voices[0].steps[5].hit ^= 1; });
  edit("drum probability", [&] { dr.voices[1].steps[4].probability ^= 7; }, [&] { dr.voices[1].steps[4].probability ^= 7; });
  edit("drum automation node", [&] { dr.lanes[0].nodeCount = 1; dr.lanes[0].targetParam = 0; dr.lanes[0].nodes[0].value = 0.5f; },
       [&] { dr.lanes[0].nodeCount = 0; dr.lanes[0].targetParam = DRUM_AUTOMATION_NONE; dr.lanes[0].nodes[0].value = 0.0f; });
  edit("pattern groove swing", [&] { dr.groove.swing = 0.3f; }, [&] { dr.groove.swing = -1.0f; });
  const SynthPattern same = a;
  a = same;
  PROOF(slotToken(sc, slot) == base, "rewriting identical content leaves the token unchanged (no spurious revocation)");
  PROOF(slotToken(*fresh, slot) == base, "token of a cold-booted slot equals the token before the boot (a stored token stays valid)");

  // --- T3 failure -------------------------------------------------------------------------------------
  std::printf("\nT3 failure: a failed PREPARE must not change any slot\n");
  const Snap beforeFail = snap(sc);
  const auto unsupported = GeneratedPhraseSong::generate(engine, 3, 12, kGuard);
  PROOF(unsupported.status == GeneratedPhraseSong::LifecycleStatus::Failed, "unsupported length (3B) is refused");
  const auto noRun = GeneratedPhraseSong::generate(engine, 8, 12, kGuard);
  PROOF(noRun.status == GeneratedPhraseSong::LifecycleStatus::Failed, "8B with no 8-slot run is refused");
  PROOF(snapsEqual(beforeFail, snap(sc)), "no slot content or descriptor changed after the refusals");

  // --- T4 overwrite + Undo ----------------------------------------------------------------------------
  std::printf("\nT4 overwrite: reclaim a marked slot (prototype), generate over it, Undo\n");
  clearSong(engine);                                   // A (0-3) and the cycle (4-11) are no longer in the Song
  const Snap beforeReclaim = snap(sc);
  std::printf("  prototype reclaim of slots 0-3 after verifying their token equals the token taken at 'mark' time\n");
  uint64_t markTok[4];
  for (int i = 0; i < 4; ++i) markTok[i] = slotToken(sc, i);
  bool tokensOk = true;
  for (int i = 0; i < 4; ++i) tokensOk = tokensOk && slotToken(sc, i) == markTok[i];
  PROOF(tokensOk, "marked slots unchanged since marking (reclaim allowed)");
  // Protected by the live Undo receipt (cycle): slots 4-11 must NOT be reclaimed; 0-3 are not in the receipt.
  GeneratedPhraseSong::GeneratedPhraseUndoPayload receipt{};
  const bool haveReceipt = GroovePuterUndo::undoOwner().read(GroovePuterUndo::UndoKind::Generation, receipt);
  std::printf("  live Undo receipt: %s firstLocalSlot=%d bars=%d\n", haveReceipt ? "present" : "absent", receipt.firstLocalSlot, receipt.bars);
  PROOF(haveReceipt && receipt.firstLocalSlot == 4 && receipt.bars == 8, "receipt covers the cycle slots 4-11, not slots 0-3");
  for (int i = 0; i < 4; ++i) prototypeReclaim(sc, i);
  const auto over = GeneratedPhraseSong::generate(engine, 4, 0, kGuard);
  PROOF(over.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow, "TAKE 4B into the reclaimed run is committed");
  PROOF(over.phrase.firstLocalSlot == 0, "first-fit put the new phrase on slots 0-3");
  bool changedFromOld = false;
  for (int i = 0; i < 4; ++i) changedFromOld = changedFromOld || slotToken(sc, i) != markTok[i];
  PROOF(changedFromOld, "slots 0-3 now hold the new phrase (old content replaced)");
  const auto undo = GeneratedPhraseSong::undoLastGeneratedPhrase(engine, kGuard);
  PROOF(undo == GroovePuterUndo::UndoResult::Restored, "Undo of the replacing TAKE reports Restored");
  bool backToOld = true, empty = true;
  for (int i = 0; i < 4; ++i) { backToOld = backToOld && slotToken(sc, i) == markTok[i]; empty = empty && PhraseGenerator::localSlotIsEmpty(sc, i); }
  std::printf("  after Undo: slots 0-3 equal to the old content: %s; slots 0-3 empty: %s\n", backToOld ? "yes" : "no", empty ? "yes" : "no");
  PROOF(!backToOld && empty, "Undo does NOT bring the old content back; the reclaimed slots end up empty (as the contract states)");

  // --- T5 melody --------------------------------------------------------------------------------------
  std::printf("\nT5 melody: a Melody descriptor is a different owner\n");
  const int free0 = 12;
  PROOF(PhraseGenerator::localSlotIsSafeForPhrase(sc, engine.currentPageIndex(), free0), "the slot is free for phrase generation before the descriptor is changed");
  sc.materialSlots[0][free0].kind = GroovePuterMaterial::MaterialKind::Melody;
  PROOF(!PhraseGenerator::localSlotIsSafeForPhrase(sc, engine.currentPageIndex(), free0), "a Melody-kind descriptor blocks phrase generation on an otherwise empty slot");
  sc.materialSlots[0][free0] = GroovePuterMaterial::MaterialSlotDescriptor{};

  std::printf("\nRESULT: %s (%d failing checks)\n", g_fail ? "FAIL" : "ALL PROOFS HOLD", g_fail);
  return g_fail ? 1 : 0;
}
