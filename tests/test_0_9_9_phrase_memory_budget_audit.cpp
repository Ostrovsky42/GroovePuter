// 0.9.9-PMB-A1 — research-only characterization. Proves the load-bearing
// assumptions behind a future bounded-memory PREPARE->COMMIT redesign of
// GeneratedPhraseSong::generate(): that P1R physical materialization is a
// pure, per-bar-random-access function, so a single-bar preflight-then-
// replay strategy can substitute for holding all 8 bars in memory at once.
//
// This test does not change and must not be read as authorizing any change
// to production staging (PreparedPhraseArrangement::material[8]).

#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/miniacid_engine.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

SerialMock Serial;
SDMock SD;

namespace {

constexpr uint32_t kTestSeed = 0xAB12CD34u;
constexpr uint32_t kAttemptOrdinal = 7;

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

void configureP1RCapableLoFi(MiniAcid& engine) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
  scene.genre.recipe = kBaseRecipeId;
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
  engine.modeManager().setGenerationSeed(kTestSeed);
}

// PROOF 1 — byte decomposition frozen. If any of these drift, the memory
// budget analysis in the PMB-A1 contract doc is stale and must be redone.
void testByteDecomposition() {
  static_assert(sizeof(PhraseGenerator::PhraseBar) == 1416,
                "PhraseBar size drifted — re-run PMB-A1 byte decomposition");
  static_assert(sizeof(GroovePuterRhythm::PreparedPhraseExecution) == 324,
                "PreparedPhraseExecution size drifted — re-run PMB-A1");
  static_assert(
      sizeof(GeneratedPhraseSong::PreparedPhraseArrangement) == 11428,
      "PreparedPhraseArrangement size drifted — re-run PMB-A1");
  constexpr std::size_t materialBytes =
      8 * sizeof(PhraseGenerator::PhraseBar);
  constexpr std::size_t overheadBytes =
      sizeof(GeneratedPhraseSong::PreparedPhraseArrangement) - materialBytes;
  static_assert(materialBytes == 11328, "material[8] byte total drifted");
  static_assert(overheadBytes == 100,
                "non-material overhead in PreparedPhraseArrangement drifted");
  std::printf(
      "PMB-A1 BYTES: PreparedPhraseArrangement=%zu material[8]=%zu (%.1f%%) "
      "overhead=%zu PhraseBar=%zu PreparedPhraseExecution=%zu\n",
      sizeof(GeneratedPhraseSong::PreparedPhraseArrangement), materialBytes,
      100.0 * static_cast<double>(materialBytes) /
          static_cast<double>(
              sizeof(GeneratedPhraseSong::PreparedPhraseArrangement)),
      overheadBytes, sizeof(PhraseGenerator::PhraseBar),
      sizeof(GroovePuterRhythm::PreparedPhraseExecution));
}

// PROOF 2 — full 8-bar P1R prepare, called twice with identical inputs,
// must be byte-identical across every bar. This is the existing-contract
// baseline (already covered elsewhere as "P1R deterministic repeat"); it is
// re-asserted here at the byte level as the control case for PROOF 3.
void testFullArrayRepeatIsByteIdentical(MiniAcid& engine) {
  configureP1RCapableLoFi(engine);
  const Scene& scene = engine.sceneManager().currentScene();

  std::array<PhraseGenerator::PhraseBar, 8> destA{};
  std::array<PhraseGenerator::PhraseBar, 8> destB{};
  GeneratedPhraseP1R::PreparationEvidence evA{};
  GeneratedPhraseP1R::PreparationEvidence evB{};

  const auto dispA = GeneratedPhraseP1R::prepare(
      engine, scene, scene.genre, 8, /*pageIndex=*/0, /*firstLocalSlot=*/0,
      kAttemptOrdinal, true, destA, evA);
  const auto dispB = GeneratedPhraseP1R::prepare(
      engine, scene, scene.genre, 8, /*pageIndex=*/0, /*firstLocalSlot=*/0,
      kAttemptOrdinal, true, destB, evB);

  assert(dispA == GeneratedPhraseP1R::PreparationDisposition::Ready);
  assert(dispB == GeneratedPhraseP1R::PreparationDisposition::Ready);
  for (int bar = 0; bar < 8; ++bar) {
    assert(sameBar(destA[bar], destB[bar]));
  }
  std::printf("PMB-A1 PROOF: full-array repeat byte-identical (8/8 bars): OK\n");
}

// PROOF 3 — the load-bearing claim for a bounded-memory redesign: a lone
// materializePreparedPhraseBar(execution, N, ...) call, using an execution
// built once and a single-bar scratch, reproduces byte-identical output to
// the same bar N produced by materializing the full 8-bar array in one
// pass. Proven for every bar, not just one, and cross-checked against an
// independently-run full-array reference.
void testSingleBarReplayMatchesFullRun(MiniAcid& engine) {
  configureP1RCapableLoFi(engine);
  Scene& scene = engine.sceneManager().currentScene();

  std::array<PhraseGenerator::PhraseBar, 8> reference{};
  GeneratedPhraseP1R::PreparationEvidence refEvidence{};
  const auto refDisposition = GeneratedPhraseP1R::prepare(
      engine, scene, scene.genre, 8, /*pageIndex=*/0, /*firstLocalSlot=*/0,
      kAttemptOrdinal, true, reference, refEvidence);
  assert(refDisposition == GeneratedPhraseP1R::PreparationDisposition::Ready);

  // Independently rebuild the compact execution plan (324 B) once, exactly
  // as a bounded PASS A/PASS B implementation would.
  GroovePuterRhythm::PhraseExecutionScratch scratch{};
  GroovePuterRhythm::PreparedPhraseExecution execution{};
  GroovePuterRhythm::PhraseExecutionMaterializationSettings settings{};
  settings.level = GroovePuterState::currentGenerationLevel();
  settings.generationAttemptOrdinal = kAttemptOrdinal;
  settings.tonalMaterializationEnabled = true;
  const uint16_t identity =
      GeneratedPhraseP1R::phraseIdentityForAttempt(kAttemptOrdinal);
  const auto status = GroovePuterRhythm::preparePhraseExecution(
      scene.genre, settings, identity, 8, scratch, execution);
  assert(status == GroovePuterRhythm::PhraseExecutionStatus::Ready);

  PhraseGenerator::PhraseBar pitchSource{};
  const bool pitchOk =
      GeneratedPhraseP1R::prepareDestinationIndependentPitchSource(
          engine, execution, pitchSource);
  assert(pitchOk);

  // One reusable single-bar scratch buffer, materialized bar-by-bar,
  // discarded/reused after each check — never all 8 bars live at once.
  for (uint8_t bar = 0; bar < 8; ++bar) {
    PhraseGenerator::PhraseBar oneBarScratch = pitchSource;
    const auto result = GroovePuterRhythm::materializePreparedPhraseBar(
        execution, bar,
        static_cast<int16_t>(songPatternFromPageBankIndex(0, 0, bar)),
        oneBarScratch.drums, oneBarScratch.synthA, oneBarScratch.synthB);
    assert(result.status ==
           GroovePuterRhythm::StrongRhythmMigrationStatus::Applied);
    assert(sameBar(oneBarScratch, reference[bar]));
  }
  std::printf(
      "PMB-A1 PROOF: single-bar preflight/replay byte-identical to full-array "
      "run (8/8 bars): OK\n");
  std::printf(
      "PMB-A1 CANDIDATE WORKING SET: execution=%zu scratch=%zu "
      "one-bar-buffer=%zu (reused for pitchSource+materialize output)\n",
      sizeof(execution), sizeof(scratch), sizeof(pitchSource));
}

}  // namespace

int main() {
  testByteDecomposition();
  MiniAcid engine(22050.0f, nullptr);
  testFullArrayRepeatIsByteIdentical(engine);
  testSingleBarReplayMatchesFullRun(engine);
  std::printf("0.9.9-PMB-A1 phrase memory budget audit: PASS\n");
  return 0;
}
