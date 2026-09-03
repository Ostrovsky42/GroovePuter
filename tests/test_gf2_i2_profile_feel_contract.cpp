// GF2-I2 — profile FEEL contract.
//
// The generation profile already selects a FEEL prior (composition.suggestedFeel)
// but production materialization reads scene.feel.timingProfile, so the prior is
// declared and causally weak. I2 makes it real through an explicit FEEL
// selection mode without letting Genre steal FEEL ownership from the musician:
//
//   scene PROFILE = AUTO      -> resolved = composition.suggestedFeel
//   scene PROFILE = concrete  -> resolved = that concrete profile
//
// AUTO is a selection mode, never a timing curve, and must never reach the
// physical FEEL interpreter.

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/feel/feel_interpreter.h"
#include "src/generation/migration/strong_rhythm_migration.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace GroovePuterRhythm;

namespace {

// Canonical fixture. Reggae / recipe 11 "Minimal Space" uses kFeelDubPocket,
// whose weighted candidates are SwingCompatible / LaidBack / PushPull. It has
// no Straight candidate, so AUTO can never resolve to Straight here and the
// A/B against STRAIGHT is guaranteed to be a real profile change.
constexpr uint8_t kMinimalSpaceRecipe = 11;
constexpr int16_t kPatternAddress = 3;

int g_failures = 0;

void expectTrue(const char* label, bool condition) {
  if (condition) {
    std::printf("%-58s OK\n", label);
    return;
  }
  std::fprintf(stderr, "%-58s FAIL\n", label);
  ++g_failures;
}

void expectFeel(const char* label, FeelProfileId actual, FeelProfileId expected) {
  if (actual == expected) {
    std::printf("%-58s OK   (%s)\n", label, feelProfileName(actual));
    return;
  }
  std::fprintf(stderr, "%-58s FAIL expected %s, got %s\n", label,
               feelProfileName(expected), feelProfileName(actual));
  ++g_failures;
}

GenreSettings minimalSpaceSettings() {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Reggae);
  settings.recipe = kMinimalSpaceRecipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(FeelProfileId profile, uint8_t amount) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = kPatternAddress;
  context.level = RealizationLevel::P2Variation;
  context.feelProfile = profile;
  context.feelAmount = amount;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

struct Material {
  StrongRhythmMigrationResult result{};
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
};

Material materialize(FeelProfileId profile, uint8_t amount) {
  Material material{};
  const StrongRhythmMigrationContext context = contextFor(profile, amount);
  material.result = migrateStrongRhythmMaterial(
      minimalSpaceSettings(), context, material.drums, material.synthA,
      material.synthB);
  return material;
}

// Structural signature: which steps carry an event, deliberately ignoring every
// expressive timing value. Two runs with the same generation identity must
// agree here whatever the FEEL profile is.
struct Topology {
  uint16_t drums[DrumPatternSet::kVoices]{};
  uint16_t synthA = 0;
  uint16_t synthB = 0;

  bool operator==(const Topology& other) const {
    return std::memcmp(this, &other, sizeof(Topology)) == 0;
  }
};

Topology topologyOf(const Material& material) {
  Topology topology{};
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (material.drums.voices[voice].steps[step].hit)
        topology.drums[voice] |= static_cast<uint16_t>(1u << step);
    }
  }
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (material.synthA.steps[step].note >= 0)
      topology.synthA |= static_cast<uint16_t>(1u << step);
    if (material.synthB.steps[step].note >= 0)
      topology.synthB |= static_cast<uint16_t>(1u << step);
  }
  return topology;
}

bool timingDiffers(const Material& a, const Material& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (a.drums.voices[voice].steps[step].timing !=
          b.drums.voices[voice].steps[step].timing) {
        return true;
      }
    }
  }
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (a.synthA.steps[step].timing != b.synthA.steps[step].timing) return true;
    if (a.synthB.steps[step].timing != b.synthB.steps[step].timing) return true;
  }
  return false;
}

bool sameMaterial(const Material& a, const Material& b) {
  return std::memcmp(&a.drums, &b.drums, sizeof(DrumPatternSet)) == 0 &&
         std::memcmp(&a.synthA, &b.synthA, sizeof(SynthPattern)) == 0 &&
         std::memcmp(&a.synthB, &b.synthB, sizeof(SynthPattern)) == 0;
}

// ---------------------------------------------------------------------------
// Persisted identity and selection-mode vocabulary
// ---------------------------------------------------------------------------

void testFeelProfileIdsAreAppendOnly() {
  expectTrue("Straight is 0", static_cast<uint8_t>(FeelProfileId::Straight) == 0);
  expectTrue("SwingCompatible is 1",
             static_cast<uint8_t>(FeelProfileId::SwingCompatible) == 1);
  expectTrue("LaidBack is 2", static_cast<uint8_t>(FeelProfileId::LaidBack) == 2);
  expectTrue("PushPullControlled is 3",
             static_cast<uint8_t>(FeelProfileId::PushPullControlled) == 3);
  expectTrue("Auto is appended as 4",
             static_cast<uint8_t>(FeelProfileId::Auto) == 4);
  expectTrue("Count is 5", static_cast<uint8_t>(FeelProfileId::Count) == 5);
}

void testAutoIsASelectionModeNotATimingProfile() {
  expectTrue("AUTO is not a concrete FEEL profile",
             !isValidFeelProfile(FeelProfileId::Auto));
  expectTrue("AUTO is a selectable FEEL setting",
             isSelectableFeelProfile(FeelProfileId::Auto));
  for (uint8_t raw = 0; raw < static_cast<uint8_t>(FeelProfileId::Auto); ++raw) {
    const auto profile = static_cast<FeelProfileId>(raw);
    expectTrue("concrete profiles stay valid and selectable",
               isValidFeelProfile(profile) && isSelectableFeelProfile(profile));
  }
  expectTrue("AUTO renders as AUTO",
             std::strcmp(feelProfileName(FeelProfileId::Auto), "AUTO") == 0);
}

// AUTO belongs to the musician's selection mode, never to a Genre vocabulary.
void testGenreProfilesOfferConcreteFeelOnly() {
  bool ok = true;
  int candidates = 0;
  for (int modeIndex = 0; modeIndex < kGenerativeModeCount; ++modeIndex) {
    const auto genre = static_cast<GenerativeMode>(modeIndex);
    const uint8_t count = availableRecipeCount(genre);
    for (uint8_t ordinal = 0; ordinal < count; ++ordinal) {
      GenreRecipeId recipe = 0;
      if (!availableRecipeAt(genre, ordinal, recipe)) continue;
      GenreSettings settings{};
      settings.generativeMode = static_cast<uint8_t>(modeIndex);
      settings.recipe = static_cast<uint8_t>(recipe);
      const GenerationProfileView profile = generationProfileFor(settings);
      for (uint8_t index = 0; index < profile.feels.count; ++index) {
        ++candidates;
        if (!isValidFeelProfile(
                static_cast<FeelProfileId>(profile.feels.candidates[index].id))) {
          std::fprintf(stderr,
                       "genre %d recipe %u offers a non-concrete FEEL candidate\n",
                       modeIndex, static_cast<unsigned>(recipe));
          ok = false;
        }
      }
    }
  }
  expectTrue("genre FEEL vocabularies are concrete only", ok && candidates > 0);
}

// ---------------------------------------------------------------------------
// AUTO / manual arbitration
// ---------------------------------------------------------------------------

void testAutoResolvesTheProfileSuggestion() {
  const Material material = materialize(FeelProfileId::Auto, 100);
  expectTrue("AUTO: materialization applies",
             material.result.status == StrongRhythmMigrationStatus::Applied);
  expectTrue("fixture: Minimal Space never suggests STRAIGHT",
             material.result.suggestedFeel != FeelProfileId::Straight);
  expectFeel("AUTO resolves to the profile suggestion",
             material.result.resolvedFeel, material.result.suggestedFeel);
  expectTrue("AUTO resolves to a concrete profile",
             isValidFeelProfile(material.result.resolvedFeel));
}

void testManualProfileOverridesTheGenrePrior() {
  const Material material = materialize(FeelProfileId::Straight, 100);
  expectTrue("MANUAL: materialization applies",
             material.result.status == StrongRhythmMigrationStatus::Applied);
  expectTrue("MANUAL: the genre prior stays visible",
             material.result.suggestedFeel != FeelProfileId::Straight);
  expectFeel("manual STRAIGHT wins over the genre prior",
             material.result.resolvedFeel, FeelProfileId::Straight);

  const Material pushPull = materialize(FeelProfileId::PushPullControlled, 100);
  expectFeel("manual PUSH/PULL wins over the genre prior",
             pushPull.result.resolvedFeel, FeelProfileId::PushPullControlled);
}

void testAutoNeverReachesTheInterpreter() {
  FeelPhrase phrase{};
  phrase.barCount = 1;
  phrase.eventCount = 1;
  phrase.events[0].role = RhythmRole::Kick;
  phrase.events[0].barIndex = 0;
  phrase.events[0].idealTick = 0;
  phrase.events[0].durationTicks = kFeelTicksPerStep;

  FeelInterpretRequest request{};
  request.profile = FeelProfileId::Auto;
  request.amount = 100;
  request.phrase = &phrase;
  request.gridIntervalTicks = kFeelTicksPerStep;

  TimedFeelPhrase timed{};
  expectTrue("unresolved AUTO is rejected by the FEEL interpreter",
             interpretFeelPhrase(request, timed) ==
                 FeelInterpretStatus::InvalidProfile);
}

// ---------------------------------------------------------------------------
// Musical regressions
// ---------------------------------------------------------------------------

// The central I2 proof: same structural language, different temporal placement.
void testProfileChangesTimingNotTopology() {
  const Material straight = materialize(FeelProfileId::Straight, 100);
  const Material automatic = materialize(FeelProfileId::Auto, 100);

  expectTrue("A/B: both runs apply",
             straight.result.status == StrongRhythmMigrationStatus::Applied &&
             automatic.result.status == StrongRhythmMigrationStatus::Applied);
  expectTrue("A/B: same rhythm identity",
             straight.result.archetype == automatic.result.archetype);
  expectTrue("A/B: same onset topology",
             topologyOf(straight) == topologyOf(automatic));
  expectTrue("A/B: at least one event is placed differently",
             timingDiffers(straight, automatic));
}

void testFeelAmountZeroIsNeutral() {
  const Material straight = materialize(FeelProfileId::Straight, 0);
  const Material automatic = materialize(FeelProfileId::Auto, 0);

  expectTrue("amount 0: AUTO still resolves a concrete profile",
             isValidFeelProfile(automatic.result.resolvedFeel) &&
             automatic.result.resolvedFeel != FeelProfileId::Straight);
  expectTrue("amount 0 neutralizes the resolved profile",
             sameMaterial(straight, automatic));
}

// One musical decision -> one FEEL resolution. If any role re-resolved
// independently, materializing with the resolved profile stated manually could
// not reproduce the AUTO run byte for byte.
void testAllRolesShareOneResolvedFeel() {
  const Material automatic = materialize(FeelProfileId::Auto, 100);
  expectTrue("roles: AUTO run applies",
             automatic.result.status == StrongRhythmMigrationStatus::Applied);
  const Material manual = materialize(automatic.result.resolvedFeel, 100);
  expectTrue("every role hears the same resolved FEEL",
             sameMaterial(automatic, manual));
}

}  // namespace

int main() {
  testFeelProfileIdsAreAppendOnly();
  testAutoIsASelectionModeNotATimingProfile();
  testGenreProfilesOfferConcreteFeelOnly();
  testAutoResolvesTheProfileSuggestion();
  testManualProfileOverridesTheGenrePrior();
  testAutoNeverReachesTheInterpreter();
  testProfileChangesTimingNotTopology();
  testFeelAmountZeroIsNeutral();
  testAllRolesShareOneResolvedFeel();

  if (g_failures != 0) {
    std::fprintf(stderr, "GF2-I2 profile feel contract: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("GF2-I2 profile feel contract: PASS\n");
  return 0;
}
