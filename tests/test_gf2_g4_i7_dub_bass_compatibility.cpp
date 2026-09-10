#include <cstdint>
#include <cstdio>
#include <map>

#include "scenes.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"

// Research-only ownership probe: include the implementation in this translation
// unit so the test can observe the exact private candidatesFor(family) owner
// without exporting a new production API. The runner deliberately excludes this
// .cpp from the ordinary source list to avoid duplicate symbols.
#include "src/generation/roles/bass_rhythm.cpp"

namespace GroovePuterRhythm {
namespace {

// Keep the identity coordinates byte-for-byte aligned with C0R5/C0R6. Those
// corpora define the 128-row pilot as identity_ordinal 0..127.
constexpr uint16_t kIdentityFirst = 0;
constexpr uint16_t kIdentityLast = 127;

const char* familyName(RhythmFamily family) {
  switch (family) {
    case RhythmFamily::FourFloor: return "FOUR_FLOOR";
    case RhythmFamily::MachineSyncopation: return "MACHINE_SYNCOPATION";
    case RhythmFamily::Breakbeat: return "BREAKBEAT";
    case RhythmFamily::UkTwoStep: return "UK_TWO_STEP";
    case RhythmFamily::HipHopBackbeat: return "HIP_HOP_BACKBEAT";
    case RhythmFamily::DubPulse: return "DUB_PULSE";
    case RhythmFamily::Funk16: return "FUNK_16";
    case RhythmFamily::SparsePulse: return "SPARSE_PULSE";
    case RhythmFamily::Count: return "COUNT";
  }
  return "UNKNOWN";
}

bool contains(const BassCandidates& candidates, BassRhythmId id) {
  for (uint8_t index = 0; index < candidates.count; ++index) {
    if (candidates.values[index] == id) return true;
  }
  return false;
}

bool profileContains(WeightedIdentityView view, BassRhythmId id) {
  for (uint8_t index = 0; index < view.count; ++index) {
    if (view.candidates[index].id == static_cast<uint8_t>(id)) return true;
  }
  return false;
}

GenreSettings dubSettings() {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Reggae);
  settings.recipe = 5;  // Dub Techno
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext canonicalContext() {
  StrongRhythmMigrationContext context{};
  context.patternAddress = 0;
  context.level = RealizationLevel::P1Canonical;
  context.generationAttemptOrdinal = 0;
  context.evolutionOrdinal = 0;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = false;
  return context;
}

struct FamilyStats {
  uint16_t rows = 0;
  uint16_t compatible = 0;
  uint16_t incompatible = 0;
  uint16_t explicitBypassAccepted = 0;
  uint16_t autoOutsideNative = 0;
};

struct BassStats {
  uint16_t rows = 0;
  uint16_t incompatible = 0;
};

bool usable(BassRhythmStatus status) {
  return status == BassRhythmStatus::Ok ||
         status == BassRhythmStatus::ValidButEmpty;
}

int run() {
  const GenreSettings settings = dubSettings();
  const GenerationProfileView profile = generationProfileFor(settings);
  if (!isValidGenerationProfile(profile)) {
    std::puts("G4_I7_FAIL invalid_dub_profile");
    return 1;
  }

  std::map<RhythmFamily, FamilyStats> families;
  std::map<BassRhythmId, BassStats> basses;
  uint16_t total = 0;
  uint16_t incompatible = 0;
  uint16_t explicitBypassAccepted = 0;
  uint16_t autoOutsideNative = 0;
  uint16_t selectedOutsideProfile = 0;
  uint16_t dubPulseKickAnswer = 0;

  for (uint16_t identity = kIdentityFirst; identity <= kIdentityLast; ++identity) {
    StrongRhythmFrozenSelection selection{};
    const StrongRhythmMigrationResult resolved =
        resolveStrongRhythmFrozenSelection(
            settings, canonicalContext(), identity, selection);
    if (resolved.status != StrongRhythmMigrationStatus::Applied ||
        !selection.resolved ||
        selection.composition.status != GenerationCompositionStatus::Ok) {
      std::printf("G4_I7_FAIL unresolved_identity=%u status=%u\n",
                  static_cast<unsigned>(identity),
                  static_cast<unsigned>(resolved.status));
      return 1;
    }

    const ReferenceVocabulary::Definition* definition =
        ReferenceVocabulary::definitionForId(
            selection.composition.rhythmArchetypeId);
    if (definition == nullptr) {
      std::printf("G4_I7_FAIL unknown_archetype identity=%u id=%u\n",
                  static_cast<unsigned>(identity),
                  static_cast<unsigned>(selection.composition.rhythmArchetypeId));
      return 1;
    }

    const BassRhythmId selected = selection.composition.bassRhythm;
    const BassCandidates native = candidatesFor(definition->family);
    const bool nativeMember = contains(native, selected);
    const bool profileMember = profileContains(profile.bassRhythms, selected);

    ++total;
    FamilyStats& family = families[definition->family];
    ++family.rows;
    BassStats& bass = basses[selected];
    ++bass.rows;
    if (!profileMember) ++selectedOutsideProfile;

    BassRhythmRequest request{};
    request.requestedId = selected;
    request.family = definition->family;
    request.archetypeId = definition->archetypeId;
    request.kickOnsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
    request.generation = selection.realizationGeneration;
    request.barOrdinal = 0;
    request.allowEmptyBar = true;
    const BassRhythmResult explicitResult = realizeBassRhythm(request);

    BassRhythmRequest autoRequest = request;
    autoRequest.requestedId = BassRhythmId::Auto;
    const BassRhythmResult autoResult = realizeBassRhythm(autoRequest);
    const bool autoNative =
        usable(autoResult.status) && contains(native, autoResult.plan.id);
    if (!autoNative) {
      ++autoOutsideNative;
      ++family.autoOutsideNative;
    }

    if (nativeMember) {
      ++family.compatible;
    } else {
      ++incompatible;
      ++family.incompatible;
      ++bass.incompatible;
      if (definition->family == RhythmFamily::DubPulse &&
          selected == BassRhythmId::KickAnswer) {
        ++dubPulseKickAnswer;
      }
      if (usable(explicitResult.status) && explicitResult.plan.id == selected) {
        ++explicitBypassAccepted;
        ++family.explicitBypassAccepted;
      }
    }
  }

  std::printf(
      "G4_I7_COORDINATES first=%u last=%u rows=%u\n",
      static_cast<unsigned>(kIdentityFirst),
      static_cast<unsigned>(kIdentityLast),
      static_cast<unsigned>(total));
  std::printf(
      "G4_I7_SUMMARY rows=%u incompatible=%u explicit_bypass_accepted=%u "
      "auto_outside_native=%u selected_outside_profile=%u families=%u\n",
      static_cast<unsigned>(total),
      static_cast<unsigned>(incompatible),
      static_cast<unsigned>(explicitBypassAccepted),
      static_cast<unsigned>(autoOutsideNative),
      static_cast<unsigned>(selectedOutsideProfile),
      static_cast<unsigned>(families.size()));

  for (const auto& entry : families) {
    const FamilyStats& stats = entry.second;
    const BassCandidates native = candidatesFor(entry.first);
    uint8_t profileNative = 0;
    uint8_t profileOutside = 0;
    for (uint8_t index = 0; index < profile.bassRhythms.count; ++index) {
      const BassRhythmId id = static_cast<BassRhythmId>(
          profile.bassRhythms.candidates[index].id);
      if (contains(native, id)) ++profileNative;
      else ++profileOutside;
    }
    std::printf(
        "G4_I7_FAMILY family=%s rows=%u compatible=%u incompatible=%u "
        "explicit_bypass=%u auto_outside=%u profile_native=%u "
        "profile_outside=%u\n",
        familyName(entry.first),
        static_cast<unsigned>(stats.rows),
        static_cast<unsigned>(stats.compatible),
        static_cast<unsigned>(stats.incompatible),
        static_cast<unsigned>(stats.explicitBypassAccepted),
        static_cast<unsigned>(stats.autoOutsideNative),
        static_cast<unsigned>(profileNative),
        static_cast<unsigned>(profileOutside));
  }

  for (const auto& entry : basses) {
    if (entry.second.incompatible == 0) continue;
    std::printf(
        "G4_I7_BASS bass=%s rows=%u incompatible=%u\n",
        bassRhythmName(entry.first),
        static_cast<unsigned>(entry.second.rows),
        static_cast<unsigned>(entry.second.incompatible));
  }

  std::printf(
      "G4_I7_WITNESS family=DUB_PULSE bass=KICK_ANSWER count=%u\n",
      static_cast<unsigned>(dubPulseKickAnswer));
  std::puts(
      "G4_I7_CAUSAL owner=GENERATION_PROFILE bass_candidates=GENRE_BAG "
      "family_filter_at_selection=ABSENT");
  std::puts(
      "G4_I7_CAUSAL materializer_request=EXPLICIT "
      "explicit_family_filter=ABSENT auto_family_filter=PRESENT");

  if (total != 128) {
    std::puts("G4_I7_FAIL expected_128_rows");
    return 1;
  }
  if (selectedOutsideProfile != 0) {
    std::puts("G4_I7_FAIL selection_not_owned_by_profile_bag");
    return 1;
  }
  if (incompatible == 0) {
    std::puts("G4_I7_FAIL no_incompatible_selection_witness");
    return 1;
  }
  if (explicitBypassAccepted != incompatible) {
    std::puts("G4_I7_FAIL explicit_bypass_not_total");
    return 1;
  }
  if (autoOutsideNative != 0) {
    std::puts("G4_I7_FAIL auto_path_escaped_native_candidates");
    return 1;
  }
  if (dubPulseKickAnswer == 0) {
    std::puts("G4_I7_FAIL no_same_family_dubpulse_kickanswer_witness");
    return 1;
  }

  std::puts("G4-I7 Dub bass compatibility characterization: PASS");
  return 0;
}

}  // namespace
}  // namespace GroovePuterRhythm

int main() { return GroovePuterRhythm::run(); }
