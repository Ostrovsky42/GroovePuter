#include <cstdlib>
#include <iostream>
#include <set>

#include "../scenes.h"
#include "../src/dsp/genre_manager.h"
#include "../src/generation/composition/rhythm_selection.h"
#include "../src/generation/rhythm/reference_vocabulary.h"

namespace {

using namespace GroovePuterRhythm;
using GroovePuterRhythm::ReferenceVocabulary::Archetype;

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "G4_C4_C6_STRUCTURAL_FAIL " << message << "\n";
    std::exit(1);
  }
}

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  return settings;
}

std::set<RhythmArchetypeId> admitted(GenerativeMode mode, uint8_t recipe) {
  const GenreSettings settings = settingsFor(mode, recipe);
  std::set<RhythmArchetypeId> result;
  const uint8_t count = compatibleRhythmCount(settings);
  for (uint8_t i = 0; i < count; ++i) result.insert(compatibleRhythmId(settings, i));
  return result;
}

const RhythmArchetype& archetype(Archetype key) {
  const RhythmArchetype* value = ReferenceVocabulary::archetypeFor(key);
  require(value != nullptr, "reference_archetype_missing");
  return *value;
}

const LaneGrammar& lane(const RhythmArchetype& value, RhythmRole role) {
  for (uint8_t i = 0; i < value.laneCount; ++i) {
    if (value.lanes[i].role == role) return value.lanes[i];
  }
  require(false, "required_lane_missing");
  return value.lanes[0];
}

void testClassicTwoStep() {
  const std::set<RhythmArchetypeId> expected{417, 419};
  require(admitted(GenerativeMode::Broken, 8) == expected,
          "classic_2step_admission_not_417_419");

  const RhythmArchetype& classic = archetype(Archetype::ClassicTwoStep);
  const RhythmArchetype& shuffled = archetype(Archetype::ShuffledFourFour);
  const StepMask backbeat = static_cast<StepMask>(stepBit(4) | stepBit(12));
  const StepMask quarters = static_cast<StepMask>(
      stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));

  require(lane(classic, RhythmRole::Backbeat).canonicalAnchors == backbeat,
          "classic_2step_backbeat_not_2_4");
  require(lane(shuffled, RhythmRole::Backbeat).canonicalAnchors == backbeat,
          "shuffled_4x4_backbeat_not_2_4");
  require(lane(classic, RhythmRole::Kick).canonicalAnchors == stepBit(0),
          "classic_2step_kick_not_broken_anchor");
  require(lane(classic, RhythmRole::Kick).canonicalAnchors != quarters,
          "classic_2step_accidentally_four_floor");
  require(lane(shuffled, RhythmRole::Kick).canonicalAnchors == quarters,
          "shuffled_4x4_kick_not_quarter_pulse");
  require(classic.timing.compatibility == TimingCompatibility::ShufflePreferred,
          "classic_2step_not_shuffle_oriented");
  require(shuffled.timing.compatibility == TimingCompatibility::ShufflePreferred,
          "shuffled_4x4_not_shuffle_oriented");

  // Recipe identity lives partly in what is excluded: the broader UKG/Broken
  // vocabulary still owns 418 and 420, but Classic 2-Step does not.
  require(expected.count(418) == 0, "classic_2step_admits_skippy_418");
  require(expected.count(420) == 0, "classic_2step_admits_machine_syncopation_420");
  std::cout << "G4_C4_STRUCTURAL_PASS admission=417,419 backbeat=4,12 kick_forms=broken+quarter shuffle=preferred\n";
}

void testPsytrance() {
  const std::set<RhythmArchetypeId> expected{401, 402, 406};
  require(admitted(GenerativeMode::Rave, 4) == expected,
          "psytrance_admission_not_401_402_406");

  const StepMask quarters = static_cast<StepMask>(
      stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
  const Archetype keys[] = {
      Archetype::StraightDrive,
      Archetype::OffbeatOpenHat,
      Archetype::RollingAcid,
  };
  for (Archetype key : keys) {
    const RhythmArchetype& value = archetype(key);
    require(lane(value, RhythmRole::Kick).canonicalAnchors == quarters,
            "psytrance_candidate_lost_quarter_kick_skeleton");
  }
  std::cout << "G4_C5_STRUCTURAL_PASS admission=401,402,406 quarter_kick=0,4,8,12\n";
}

void testReggae() {
  const std::set<RhythmArchetypeId> baseExpected{409, 410, 411, 412};
  const std::set<RhythmArchetypeId> minimalExpected{409, 411, 412};
  const std::set<RhythmArchetypeId> base = admitted(GenerativeMode::Reggae, 0);
  const std::set<RhythmArchetypeId> minimal = admitted(GenerativeMode::Reggae, 11);

  require(base == baseExpected, "reggae_base_admission_not_409_410_411_412");
  require(minimal == minimalExpected, "reggae_minimal_admission_not_409_411_412");

  std::set<RhythmArchetypeId> difference;
  for (RhythmArchetypeId id : base) {
    if (minimal.count(id) == 0) difference.insert(id);
  }
  require(difference == std::set<RhythmArchetypeId>{410},
          "reggae_minimal_not_base_minus_steppers");

  const Archetype keys[] = {
      Archetype::OneDropSpace,
      Archetype::Steppers,
      Archetype::SparseSkank,
      Archetype::ChordResponse,
  };
  for (Archetype key : keys) {
    require(archetype(key).family == RhythmFamily::DubPulse,
            "reggae_candidate_not_dubpulse_structure");
  }
  require(minimal.count(410) == 0, "reggae_minimal_admits_steppers");
  std::cout << "G4_C6_STRUCTURAL_PASS base=409,410,411,412 minimal=409,411,412 prohibition=410\n";
}

}  // namespace

int main() {
  testClassicTwoStep();
  testPsytrance();
  testReggae();
  std::cout << "G4-C4-C6 structural witnesses: PASS\n";
  return 0;
}
