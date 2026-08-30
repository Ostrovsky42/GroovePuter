// 0.9.9-PMB-A2 — research-only characterization of the LEGACY (non-P1R)
// strong-rhythm route inside GeneratedPhraseSong::prepareWithGenerationAttempt.
// Proves whether the legacy per-bar physical generation can share the same
// bounded single-buffer preflight/replay strategy PMB-A1 established for the
// P1R route.
//
// This test does not change and must not be read as authorizing any change
// to production staging (PreparedPhraseArrangement::material[8]).

#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/miniacid_engine.h"

#include <cassert>
#include <cstdio>
#include <cstring>

SerialMock Serial;
SDMock SD;

namespace {

bool sameSynth(const SynthPattern& a, const SynthPattern& b) {
  return std::memcmp(&a, &b, sizeof(SynthPattern)) == 0;
}
bool sameDrums(const DrumPatternSet& a, const DrumPatternSet& b) {
  return std::memcmp(&a, &b, sizeof(DrumPatternSet)) == 0;
}
bool sameBar(const PhraseGenerator::PhraseBar& a,
             const PhraseGenerator::PhraseBar& b) {
  return sameSynth(a.synthA, b.synthA) && sameSynth(a.synthB, b.synthB) &&
         sameDrums(a.drums, b.drums);
}

void resetScene(MiniAcid& engine, GenerativeMode mode, GenreRecipeId recipe) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(mode);
  scene.genre.recipe = recipe;
  scene.activeSongSlot = 0;
  scene.songs[0] = Song{};
  scene.feel.patternBars = 1;
  for (int bank = 0; bank < kBankCount; ++bank) {
    for (int slot = 0; slot < Bank<SynthPattern>::kPatterns; ++slot) {
      scene.synthABanks[bank].patterns[slot] = SynthPattern{};
      scene.synthBBanks[bank].patterns[slot] = SynthPattern{};
      scene.drumBanks[bank].patterns[slot] = DrumPatternSet{};
    }
  }
}

// FINDING 1 — reachability. Every recipe value the live GENRE page can
// actually select (recipeChoicesForGenre in genre_page.cpp) resolves to a
// non-Legacy route: kBaseRecipeId under every mode (proven in PMB-A1), and
// every named recipe constant 1..kDustyJazzRecipeId(17) is explicitly
// special-cased by selectStrongRhythmRoute. The Legacy branch is reachable
// only through an out-of-range scene.genre.recipe value that
// prepareWithGenerationAttempt reads unclamped (e.g. stale/imported scene
// data) -- normal live recipe cycling cannot select it. This is documented,
// not fixed: normalizeRecipeForGenre/sceneRecipe() clamp for display, but
// prepareWithGenerationAttempt reads scene.genre.recipe directly.
void testLegacyIsUnreachableViaLiveRecipeSelection() {
  bool anyLegacyInLiveRange = false;
  for (int modeIdx = 0; modeIdx < kGenerativeModeCount; ++modeIdx) {
    // kBaseRecipeId .. kDustyJazzRecipeId spans every named/live-selectable
    // recipe constant across all per-genre pickers.
    for (int recipe = 0; recipe <= 17; ++recipe) {
      GenreSettings genre{};
      genre.generativeMode = static_cast<uint8_t>(modeIdx);
      genre.recipe = static_cast<GenreRecipeId>(recipe);
      if (GroovePuterRhythm::selectStrongRhythmRoute(genre) ==
          GroovePuterRhythm::StrongRhythmRoute::Legacy) {
        anyLegacyInLiveRange = true;
      }
    }
  }
  assert(!anyLegacyInLiveRange);
  std::printf(
      "PMB-A2 FINDING 1: Legacy route unreachable via live recipe range "
      "0..17 across all %d modes: CONFIRMED\n",
      kGenerativeModeCount);
}

// FINDING 2 — sub-route survey. Legacy IS reachable via an out-of-range
// persisted recipe id. Record which sub-route (Atlas vs procedural) each
// (mode, out-of-range recipe) pair resolves to.
void surveyLegacySubroutes() {
  int atlasCount = 0;
  int proceduralCount = 0;
  for (int modeIdx = 0; modeIdx < kGenerativeModeCount; ++modeIdx) {
    for (int recipe = 18; recipe < 40; ++recipe) {
      GenreSettings genre{};
      genre.generativeMode = static_cast<uint8_t>(modeIdx);
      genre.recipe = static_cast<GenreRecipeId>(recipe);
      if (GroovePuterRhythm::selectStrongRhythmRoute(genre) !=
          GroovePuterRhythm::StrongRhythmRoute::Legacy) {
        continue;
      }
      const bool atlas =
          AtlasRuntime::hasRecipe(static_cast<uint8_t>(recipe)) &&
          AtlasRuntime::variationCount(static_cast<uint8_t>(recipe)) >= 3;
      if (atlas) {
        ++atlasCount;
      } else {
        ++proceduralCount;
      }
    }
  }
  std::printf(
      "PMB-A2 FINDING 2: out-of-range recipe survey (18..39 x %d modes): "
      "legacy+atlas=%d legacy+procedural=%d\n",
      kGenerativeModeCount, atlasCount, proceduralCount);
  // Not asserted zero: AtlasRuntime's recipe catalog is generated/append-only
  // and could in principle grow into this range. If atlasCount becomes
  // nonzero, PMB-A2's "Atlas sub-route is effectively unreachable in
  // combination with Legacy" observation must be revisited, not silently
  // assumed to still hold.
}

// FINDING 3 — AtlasRuntime::applyRecipe is a pure static-table lookup: no
// RNG, no mutable state, output determined entirely by (recipeId,
// variationIndex). Proven by calling it twice for the same recipe with
// data known to exist (any recipe reachable via the live UI has Atlas data
// for genres that use it), independent of Legacy-route reachability.
void testAtlasApplyRecipeIsPureLookup() {
  // Use a live-reachable Atlas-backed recipe (Acid variant 6, per
  // kAcidRecipes in genre_page.cpp) purely as a data source -- this does not
  // claim Atlas+Legacy co-occurs; it isolates AtlasRuntime::applyRecipe's
  // own purity, independent of which route selects it.
  constexpr uint8_t kRecipe = 6;
  if (!AtlasRuntime::hasRecipe(kRecipe) ||
      AtlasRuntime::variationCount(kRecipe) == 0) {
    std::printf(
        "PMB-A2 FINDING 3: SKIPPED (recipe %u has no Atlas data on this "
        "build)\n",
        kRecipe);
    return;
  }
  PhraseGenerator::PhraseBar barX{};
  PhraseGenerator::PhraseBar barY{};
  const bool okX = AtlasRuntime::applyRecipe(kRecipe, 0, barX.synthA,
                                             barX.synthB, barX.drums, nullptr);
  const bool okY = AtlasRuntime::applyRecipe(kRecipe, 0, barY.synthA,
                                             barY.synthB, barY.drums, nullptr);
  assert(okX && okY);
  assert(sameBar(barX, barY));
  std::printf(
      "PMB-A2 FINDING 3: AtlasRuntime::applyRecipe pure static lookup, "
      "repeat call byte-identical: CONFIRMED\n");
}

// FINDING 4 — deriveBar is void (infallible) and tolerates in-place
// aliasing: &base == &output produces the same result as two separate
// buffers. This is the key fact enabling a single-PhraseBar-sized working
// buffer for the procedural sub-route (no separate "base" buffer needed).
void testDeriveBarInPlaceAliasing() {
  PhraseGenerator::PhraseBar reference{};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    reference.synthA.steps[i].note = static_cast<int8_t>(40 + i);
    reference.synthB.steps[i].note = static_cast<int8_t>(60 + i);
  }
  PhraseGenerator::PhraseBar twoBuffer = reference;
  PhraseGenerator::PhraseBar output{};
  PhraseGenerator::deriveBar(twoBuffer,
                             PhraseGenerator::PhraseBarRole::MicroVariation,
                             0x1234u, 3, output);

  PhraseGenerator::PhraseBar aliased = reference;
  PhraseGenerator::deriveBar(aliased,
                             PhraseGenerator::PhraseBarRole::MicroVariation,
                             0x1234u, 3, aliased);  // base IS output

  assert(sameBar(output, aliased));
  std::printf(
      "PMB-A2 FINDING 4: deriveBar tolerates in-place aliasing "
      "(byte-identical to two-buffer call): CONFIRMED\n");
}

// FINDING 5 — the central PMB-A2 claim: proceduralBase does not require
// persistent retention across a preflight/commit boundary. Two
// independently constructed, independently seeded GrooveboxModeManager
// instances (mirroring "build it once for preflight, build it again for
// commit") produce a byte-identical base pattern set.
void testProceduralBaseIsRegenerableOnDemand(MiniAcid& engine) {
  constexpr GenreRecipeId kForcedLegacyRecipe = 200;  // forces Legacy fallthrough
  resetScene(engine, GenerativeMode::Acid, kForcedLegacyRecipe);
  engine.modeManager().setGenerationSeed(0x778899AAu);

  const uint32_t seed = GeneratedPhraseSong::phraseSeed(
      engine, engine.currentPageIndex(), /*songStart=*/0, /*bars=*/8);
  auto& genreManager = engine.genreManager();
  const GrooveboxMode mappedMode = GenreManager::grooveboxModeForRecipe(
      genreManager.recipe(), genreManager.generativeMode());
  const GenerativeParams params = genreManager.getCompiledGenerativeParams();
  const GenreBehavior behavior = genreManager.getBehavior();

  auto buildBase = [&]() {
    PhraseGenerator::PhraseBar base{};
    GrooveboxModeManager scratch(engine);
    scratch.setModeLocal(mappedMode);
    scratch.setFlavorLocal(engine.modeManager().flavor());
    scratch.setGenerationSeed(seed);
    scratch.generatePattern(base.synthA, engine.bpm(), params, behavior, 0);
    scratch.generatePattern(base.synthB, engine.bpm(), params, behavior, 1);
    scratch.generateDrumPattern(base.drums, params, behavior);
    return base;
  };

  const PhraseGenerator::PhraseBar baseX = buildBase();
  const PhraseGenerator::PhraseBar baseY = buildBase();
  assert(sameBar(baseX, baseY));
  std::printf(
      "PMB-A2 FINDING 5: proceduralBase independently regenerated from two "
      "fresh GrooveboxModeManager instances: BYTE-IDENTICAL\n");
}

// FINDING 6 — full-array control case: the real legacy procedural
// prepare() path, called twice end-to-end with identical inputs, is
// byte-identical across all 8 bars. This is the baseline Finding 5 and 4
// must be consistent with.
void testLegacyProceduralFullArrayRepeat(MiniAcid& engine) {
  constexpr GenreRecipeId kForcedLegacyRecipe = 200;
  resetScene(engine, GenerativeMode::Acid, kForcedLegacyRecipe);
  engine.modeManager().setGenerationSeed(0x778899AAu);

  GeneratedPhraseSong::PreparedPhraseArrangement a{};
  GeneratedPhraseSong::PreparedPhraseArrangement b{};
  const bool okA = GeneratedPhraseSong::prepare(engine, 8, 0, a);
  const bool okB = GeneratedPhraseSong::prepare(engine, 8, 0, b);
  assert(okA && okB);
  for (int bar = 0; bar < 8; ++bar) {
    assert(sameBar(a.material[bar], b.material[bar]));
  }
  std::printf(
      "PMB-A2 FINDING 6: legacy procedural full-array repeat, all 8 bars: "
      "BYTE-IDENTICAL\n");
}

}  // namespace

int main() {
  testLegacyIsUnreachableViaLiveRecipeSelection();
  surveyLegacySubroutes();
  testAtlasApplyRecipeIsPureLookup();
  testDeriveBarInPlaceAliasing();

  MiniAcid engine(22050.0f, nullptr);
  testProceduralBaseIsRegenerableOnDemand(engine);
  testLegacyProceduralFullArrayRepeat(engine);

  std::printf(
      "PMB-A2 CANDIDATE CARRIER: PhraseBar=%zu GenerativeParams=%zu "
      "GenreBehavior=%zu GenreSettings=%zu (single buffer + scalars, no "
      "persistent second PhraseBar)\n",
      sizeof(PhraseGenerator::PhraseBar), sizeof(GenerativeParams),
      sizeof(GenreBehavior), sizeof(GenreSettings));
  std::printf("0.9.9-PMB-A2 legacy replay/preflight audit: PASS\n");
  return 0;
}
