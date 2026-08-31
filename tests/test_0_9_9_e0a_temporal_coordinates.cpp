#include "arduino_compat.h"

#include "../src/audio/audio_config.h"
#include "../src/dsp/generated_phrase_song.h"
#include "../src/dsp/miniacid_engine.h"
#include "../src/phrase/phrase_core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterRhythm::StrongRhythmMigrationContext;
using GroovePuterRhythm::StrongRhythmMigrationResult;
using GroovePuterRhythm::StrongRhythmMigrationStatus;

constexpr uint32_t kTestSeed = 0xE0A09900u;

struct ExpectedCoordinate {
  uint8_t phraseBarOrdinal;
  uint8_t evolutionOrdinal;
  uint8_t vocabularyBarOrdinal;
};

constexpr std::array<ExpectedCoordinate, 8> kExpectedCoordinates = {{
    {0, 0, 0},
    {1, 0, 1},
    {2, 0, 2},
    {3, 0, 3},
    {4, 1, 0},
    {5, 1, 1},
    {6, 1, 2},
    {7, 1, 3},
}};

[[noreturn]] void fail(const char* message) {
  std::fprintf(stderr, "E0A_TEST FAIL: %s\n", message);
  std::exit(1);
}

void expect(bool condition, const char* message) {
  if (!condition) fail(message);
}

void configureFamily(MiniAcid& engine,
                     GenerativeMode mode,
                     GenreRecipeId recipe) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(mode);
  scene.genre.recipe = recipe;
  scene.genre.morphTarget = 0;
  scene.genre.morphAmount = 0;
  scene.genre.regenerateOnApply = false;
  scene.genre.applyTempoOnApply = false;
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

void printCoordinateCase(uint8_t bars) {
  std::printf("E0A_COORD bars=%u P=", static_cast<unsigned>(bars));
  for (uint8_t bar = 0; bar < bars; ++bar) {
    std::printf("%s%u", bar == 0 ? "" : ",",
                static_cast<unsigned>(kExpectedCoordinates[bar].phraseBarOrdinal));
  }
  std::printf(" E=");
  for (uint8_t bar = 0; bar < bars; ++bar) {
    std::printf("%s%u", bar == 0 ? "" : ",",
                static_cast<unsigned>(kExpectedCoordinates[bar].evolutionOrdinal));
  }
  std::printf(" V=");
  for (uint8_t bar = 0; bar < bars; ++bar) {
    std::printf("%s%u", bar == 0 ? "" : ",",
                static_cast<unsigned>(kExpectedCoordinates[bar].vocabularyBarOrdinal));
  }
  std::printf("\n");
}

void verifyCoordinateCase(const Scene& scene, uint8_t bars) {
  for (uint8_t bar = 0; bar < bars; ++bar) {
    const ExpectedCoordinate expected = kExpectedCoordinates[bar];
    const auto coordinates =
        GroovePuterRhythm::phraseTemporalCoordinatesForBar(bar);
    expect(coordinates.phraseBarOrdinal == expected.phraseBarOrdinal,
           "phraseBarOrdinal mapping changed");
    expect(coordinates.evolutionOrdinal == expected.evolutionOrdinal,
           "evolutionOrdinal mapping changed");
    expect(GroovePuterRhythm::phraseVocabularyBarOrdinal(bar) ==
               expected.vocabularyBarOrdinal,
           "four-bar vocabulary-local mapping changed");

    const StrongRhythmMigrationContext context =
        GeneratedPhraseSong::migrationContextFor(scene, 0, bar);
    expect(context.phraseBarOrdinal == expected.phraseBarOrdinal,
           "PREPARE migration context lost phraseBarOrdinal");
    expect(context.evolutionOrdinal == expected.evolutionOrdinal,
           "PREPARE migration context lost evolutionOrdinal");
  }
  printCoordinateCase(bars);
}

// PMB-P1: PreparedPhraseArrangement no longer holds physical material (see
// docs/contracts/0_9_9_PHRASE_PMB_P1_BOUNDED_PREPARE_COMMIT.md) -- PREPARE
// now produces a compact plan (route + execution state). A raw memcmp of the
// whole (trivially copyable) plan is NOT a valid determinism check here: the
// plan's nested execution state (PreparedPhraseExecution) is built by
// assigning from locally-constructed aggregates with explicit-field
// initializers, which leaves their inter-field alignment padding as
// unspecified stack content -- two logically-identical PREPARE calls can
// carry different padding bytes despite every named field matching. The
// same-request determinism contract that actually matters is: replaying the
// same plan MATERIALIZES byte-identical physical bars, which is exactly what
// PMB-A1/PMB-A2 proved and is what this checks directly, on demand, instead
// of memcmp-ing implementation-detail padding.
bool sameMaterializedBars(
    MiniAcid& engine,
    const Scene& scene,
    const GeneratedPhraseSong::PreparedPhraseArrangement& left,
    const GeneratedPhraseSong::PreparedPhraseArrangement& right,
    uint8_t bars) {
  if (left.useP1RRoute != right.useP1RRoute) return false;
  for (uint8_t bar = 0; bar < bars; ++bar) {
    PhraseGenerator::PhraseBar leftBar{};
    PhraseGenerator::PhraseBar rightBar{};
    bool leftOk = false;
    bool rightOk = false;
    if (left.useP1RRoute) {
      leftOk = GeneratedPhraseP1R::materializeOneBar(engine, left.p1rExecution, bar, 0, leftBar);
      rightOk = GeneratedPhraseP1R::materializeOneBar(engine, right.p1rExecution, bar, 0, rightBar);
    } else {
      leftOk = GeneratedPhraseSong::materializeLegacyBar(engine, scene, left, bar, leftBar);
      rightOk = GeneratedPhraseSong::materializeLegacyBar(engine, scene, right, bar, rightBar);
    }
    if (leftOk != rightOk) return false;
    if (leftOk && std::memcmp(&leftBar, &rightBar, sizeof(leftBar)) != 0) return false;
  }
  return true;
}

void verifyPrepareDeterminism(MiniAcid& engine, uint8_t bars) {
  configureFamily(engine, GenerativeMode::Rave, kBaseRecipeId);
  engine.modeManager().setGenerationSeed(kTestSeed);
  const Scene& scene = engine.sceneManager().currentScene();

  GeneratedPhraseSong::PreparedPhraseArrangement first{};
  GeneratedPhraseSong::PreparedPhraseArrangement second{};
  const bool firstOk = GeneratedPhraseSong::prepare(engine, bars, 0, first);
  const bool secondOk = GeneratedPhraseSong::prepare(engine, bars, 0, second);
  // A P1R-capable route may typed-reject a request (e.g. an inadmissible
  // phrase length for the resolved composition/archetype). That is frozen
  // P1R length policy, not an E0a concern: same-request PREPARE must still
  // be deterministic, whether it accepts or rejects.
  expect(firstOk == secondOk,
         "identical PREPARE request changed accept/reject outcome");
  expect(first.request.bars == second.request.bars &&
             first.request.songStart == second.request.songStart &&
             first.request.pageIndex == second.request.pageIndex &&
             first.request.seed == second.request.seed,
         "identical PREPARE request changed between reruns");
  expect(first.result.error == second.result.error &&
             first.result.bars == second.result.bars &&
             first.result.songStart == second.result.songStart &&
             first.result.firstLocalSlot == second.result.firstLocalSlot &&
             first.result.firstGlobalPattern == second.result.firstGlobalPattern,
         "identical PREPARE result metadata changed between reruns");
  if (firstOk) {
    expect(sameMaterializedBars(engine, scene, first, second, bars),
           "same request rerun does not materialize byte-identical physical bars");
  }

  std::printf("E0A_DETERMINISM bars=%u byte_identical=PASS\n",
              static_cast<unsigned>(bars));
}

struct MigrationMaterial {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  StrongRhythmMigrationResult result{};
};

MigrationMaterial migrate(const GenreSettings& genre,
                          const StrongRhythmMigrationContext& context) {
  MigrationMaterial output{};
  output.result = GroovePuterRhythm::migrateStrongRhythmMaterial(
      genre, context, output.drums, output.synthA, output.synthB);
  return output;
}

bool sameMigrationMaterial(const MigrationMaterial& left,
                           const MigrationMaterial& right) {
  return std::memcmp(&left.drums, &right.drums, sizeof(left.drums)) == 0 &&
         std::memcmp(&left.synthA, &right.synthA, sizeof(left.synthA)) == 0 &&
         std::memcmp(&left.synthB, &right.synthB, sizeof(left.synthB)) == 0;
}

void verifyEvolutionOrdinalIsContextOnly(MiniAcid& engine) {
  configureFamily(engine, GenerativeMode::Rave, kBaseRecipeId);
  const Scene& scene = engine.sceneManager().currentScene();

  StrongRhythmMigrationContext first =
      GeneratedPhraseSong::migrationContextFor(scene, 0, 0);
  StrongRhythmMigrationContext second = first;
  first.evolutionOrdinal = 0;
  second.evolutionOrdinal = 7;

  const MigrationMaterial left = migrate(scene.genre, first);
  const MigrationMaterial right = migrate(scene.genre, second);
  expect(left.result.status == StrongRhythmMigrationStatus::Applied,
         "baseline Strong Rhythm migration failed");
  expect(right.result.status == StrongRhythmMigrationStatus::Applied,
         "evolutionOrdinal comparison migration failed");
  expect(sameMigrationMaterial(left, right),
         "evolutionOrdinal manufactured output variation in E0a");

  std::printf("E0A_EVOLUTION_CONTEXT_ONLY changed_ordinal_output=IDENTICAL\n");
}

void verifyNonPhraseCompatibility(MiniAcid& engine) {
  configureFamily(engine, GenerativeMode::LoFi, kClassicChillRecipeId);
  const Scene& scene = engine.sceneManager().currentScene();

  StrongRhythmMigrationContext explicitPhrase =
      GeneratedPhraseSong::migrationContextFor(scene, 2, 2);
  StrongRhythmMigrationContext legacy = explicitPhrase;
  legacy.phraseBarOrdinal = GroovePuterRhythm::kUnspecifiedPhraseBarOrdinal;
  legacy.evolutionOrdinal = 0;

  const MigrationMaterial explicitMaterial = migrate(scene.genre, explicitPhrase);
  const MigrationMaterial legacyMaterial = migrate(scene.genre, legacy);
  expect(explicitMaterial.result.status == StrongRhythmMigrationStatus::Applied,
         "explicit Phrase compatibility migration failed");
  expect(legacyMaterial.result.status == StrongRhythmMigrationStatus::Applied,
         "non-Phrase compatibility migration failed");
  expect(sameMigrationMaterial(explicitMaterial, legacyMaterial),
         "non-Phrase patternAddress compatibility path changed material");

  StrongRhythmMigrationContext defaultContext{};
  expect(defaultContext.phraseBarOrdinal ==
             GroovePuterRhythm::kUnspecifiedPhraseBarOrdinal,
         "default non-Phrase migration context must remain unspecified");
  expect(defaultContext.evolutionOrdinal == 0,
         "default non-Phrase evolution ordinal must remain zero");

  std::printf("E0A_NON_PHRASE_COMPAT patternAddress=2 material=IDENTICAL\n");
}

void verifyPhraseReferenceCarrier() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);

  Song source{};
  source.length = 8;
  for (int bar = 0; bar < 8; ++bar) {
    source.positions[bar].patterns[static_cast<int>(SongTrack::SynthA)] =
        static_cast<int16_t>(10 + bar);
    source.positions[bar].patterns[static_cast<int>(SongTrack::SynthB)] =
        static_cast<int16_t>(30 + bar);
    source.positions[bar].patterns[static_cast<int>(SongTrack::Drums)] =
        static_cast<int16_t>(50 + bar);
  }

  const PhraseCore::Result captured = PhraseCore::captureSongRegion(
      bank, PhraseCore::SlotId::A, source, 0, 0, 8,
      PhraseCore::Role::Main, PhraseCore::Source::Generated);
  expect(static_cast<bool>(captured), "8-bar Phrase reference capture failed");

  const PhraseCore::PhraseSlot* phrase =
      PhraseCore::slotAt(bank, PhraseCore::SlotId::A);
  expect(phrase != nullptr && PhraseCore::isValid(*phrase),
         "captured Phrase reference slot is invalid");
  expect(phrase->metadata.lengthBars == 8,
         "Phrase carrier no longer represents eight physical bars");
  expect(phrase->metadata.storage == PhraseCore::StorageMode::ReferenceView,
         "Phrase carrier must remain a reference view");

  for (uint8_t bar = 0; bar < 8; ++bar) {
    expect(PhraseCore::patternAt(*phrase, bar, SongTrack::SynthA) == 10 + bar,
           "Synth A Phrase reference order changed");
    expect(PhraseCore::patternAt(*phrase, bar, SongTrack::SynthB) == 30 + bar,
           "Synth B Phrase reference order changed");
    expect(PhraseCore::patternAt(*phrase, bar, SongTrack::Drums) == 50 + bar,
           "Drum Phrase reference order changed");
  }

  Song destination{};
  const PhraseCore::Result written = PhraseCore::writeToSong(
      bank, PhraseCore::SlotId::A, destination, 0, true);
  expect(static_cast<bool>(written), "Phrase reference write to Song failed");
  expect(destination.length == 8, "Song arrangement length did not follow Phrase refs");
  for (int bar = 0; bar < 8; ++bar) {
    expect(destination.positions[bar].patterns[static_cast<int>(SongTrack::SynthA)] ==
               10 + bar,
           "Song did not remain Synth A arrangement owner");
    expect(destination.positions[bar].patterns[static_cast<int>(SongTrack::SynthB)] ==
               30 + bar,
           "Song did not remain Synth B arrangement owner");
    expect(destination.positions[bar].patterns[static_cast<int>(SongTrack::Drums)] ==
               50 + bar,
           "Song did not remain drum arrangement owner");
  }

  std::printf("E0A_CARRIER phrase_refs=8 song_arrangement=PASS\n");
}

}  // namespace

static_assert(SynthPattern::kSteps == 16,
              "Pattern must remain one editable 16-step physical bar");
static_assert(DrumPattern::kSteps == 16,
              "Drum Pattern must remain one editable 16-step physical bar");
static_assert(PhraseCore::kMaxBars == 8,
              "Phrase reference carrier must keep 1/2/4/8 physical capacity");
static_assert(GroovePuterRhythm::kGrooveVocabularyPhraseBars == 4,
              "Groove Vocabulary identity must remain four bars");
static_assert(std::is_trivially_copyable<PhraseGenerator::PhraseBar>::value,
              "byte-identical PREPARE characterization requires value material");

int main() {
  MiniAcid engine(kSampleRate, nullptr);
  engine.modeManager().setGenerationSeed(kTestSeed);

  configureFamily(engine, GenerativeMode::Rave, kBaseRecipeId);
  const Scene& scene = engine.sceneManager().currentScene();
  for (uint8_t bars : std::array<uint8_t, 4>{1, 2, 4, 8}) {
    verifyCoordinateCase(scene, bars);
    verifyPrepareDeterminism(engine, bars);
  }

  verifyEvolutionOrdinalIsContextOnly(engine);
  verifyNonPhraseCompatibility(engine);
  verifyPhraseReferenceCarrier();

  std::printf("0.9.9-E0a temporal coordinate characterization: PASS\n");
  return 0;
}
