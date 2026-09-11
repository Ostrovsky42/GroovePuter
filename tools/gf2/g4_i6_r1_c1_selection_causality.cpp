#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "scenes.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/generation_context.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

constexpr WeightedIdentityCandidate kHistoricalAcidBass[] = {
    {static_cast<uint8_t>(BassRhythmId::KickLock), 70},
    {static_cast<uint8_t>(BassRhythmId::OffbeatPush), 90},
    {static_cast<uint8_t>(BassRhythmId::RollingDrive), 110},
    {static_cast<uint8_t>(BassRhythmId::SyncopatedHook), 75},
};

struct SelectionTrace {
  uint32_t seed = 0;
  uint32_t draw = 0;
  uint16_t totalWeight = 0;
  uint16_t coordinate = 0;
  uint8_t index = 0;
  uint8_t id = 0;
};

struct CompositionTrace {
  SelectionTrace bass{};
  SelectionTrace chord{};
  SelectionTrace melodic{};
  SelectionTrace motif{};
};

struct DeltaCounts {
  uint32_t rows = 0;
  uint32_t changed = 0;
  uint32_t bassChanged = 0;
  uint32_t chordChanged = 0;
  uint32_t melodicChanged = 0;
  uint32_t motifChanged = 0;
  uint32_t bassSameDownstreamChanged = 0;
  uint32_t bassChangedOnly = 0;
  uint32_t allFourChanged = 0;
  std::array<uint32_t, 8> combinations{};
};

uint32_t baseSalt(const GenerationProfileView& profile) {
  return (static_cast<uint32_t>(profile.generativeMode) << 24u) |
         (static_cast<uint32_t>(profile.recipe) << 16u);
}

bool traceSelection(WeightedIdentityView input,
                    GenerationDomain domain,
                    RhythmArchetypeId upstreamArchetype,
                    uint32_t semanticSalt,
                    const GenerationContext& generation,
                    SelectionTrace& trace) {
  WeightedIdentityCandidate canonical[kMaxWeightedCandidates]{};
  uint8_t count = 0;
  if (input.candidates == nullptr || input.count == 0 ||
      input.count > kMaxWeightedCandidates) {
    return false;
  }
  for (uint8_t sourceIndex = 0; sourceIndex < input.count; ++sourceIndex) {
    const WeightedIdentityCandidate candidate = input.candidates[sourceIndex];
    if (candidate.weight == 0) continue;
    uint8_t insertion = 0;
    while (insertion < count && canonical[insertion].id < candidate.id) ++insertion;
    if (insertion < count && canonical[insertion].id == candidate.id) {
      const uint16_t combined =
          static_cast<uint16_t>(canonical[insertion].weight) + candidate.weight;
      canonical[insertion].weight =
          static_cast<uint8_t>(combined > 255u ? 255u : combined);
      continue;
    }
    for (uint8_t move = count; move > insertion; --move) {
      canonical[move] = canonical[move - 1u];
    }
    canonical[insertion] = candidate;
    ++count;
  }

  for (uint8_t index = 0; index < count; ++index) {
    trace.totalWeight = static_cast<uint16_t>(
        trace.totalWeight + canonical[index].weight);
  }
  if (trace.totalWeight == 0) return false;

  trace.seed = deriveGenerationSeed(
      generation, upstreamArchetype, domain, semanticSalt);
  trace.draw = deterministicValue(trace.seed, 0);
  trace.coordinate = static_cast<uint16_t>(trace.draw % trace.totalWeight);
  uint16_t remaining = trace.coordinate;
  for (uint8_t index = 0; index < count; ++index) {
    if (remaining < canonical[index].weight) {
      trace.index = index;
      trace.id = canonical[index].id;
      uint8_t production = 0;
      if (!selectWeightedIdentityFromView(
              input, domain, upstreamArchetype, semanticSalt, generation,
              production) || production != trace.id) {
        return false;
      }
      return true;
    }
    remaining = static_cast<uint16_t>(remaining - canonical[index].weight);
  }
  return false;
}

bool traceComposition(const GenerationProfileView& profile,
                      const GenerationContext& generation,
                      RhythmArchetypeId archetype,
                      WeightedIdentityView bassView,
                      CompositionTrace& trace) {
  const uint32_t salt = baseSalt(profile);
  if (!traceSelection(bassView, GenerationDomain::BassRhythmSelection,
                      archetype, salt, generation, trace.bass)) {
    return false;
  }
  if (!traceSelection(profile.chordRhythms,
                      GenerationDomain::ChordRhythmSelection, archetype,
                      salt | trace.bass.id, generation, trace.chord)) {
    return false;
  }
  if (!traceSelection(profile.melodicRhythms,
                      GenerationDomain::MelodicRhythmSelection, archetype,
                      salt | (static_cast<uint32_t>(trace.bass.id) << 8u) |
                          trace.chord.id,
                      generation, trace.melodic)) {
    return false;
  }
  if (!traceSelection(profile.motifShapes, GenerationDomain::MotifSelection,
                      archetype, salt | trace.melodic.id, generation,
                      trace.motif)) {
    return false;
  }
  return true;
}

GenreSettings acidSettings(GenreRecipeId recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
  settings.recipe = recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(RealizationLevel level) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = 0;
  context.level = level;
  context.generationAttemptOrdinal = 0;
  context.evolutionOrdinal = 0;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  return context;
}

const char* recipeName(GenreRecipeId recipe) {
  switch (recipe) {
    case 0: return "BASE";
    case 6: return "Chicago Jack";
    case 7: return "Rolling Acid";
    default: return "UNKNOWN";
  }
}

const char* levelName(uint8_t level) {
  switch (level) {
    case 0: return "P1";
    case 1: return "P2";
    case 2: return "P3";
    default: return "PX";
  }
}

void updateCounts(const CompositionTrace& oldTrace,
                  const CompositionTrace& newTrace,
                  DeltaCounts& counts) {
  ++counts.rows;
  const bool b = oldTrace.bass.id != newTrace.bass.id;
  const bool c = oldTrace.chord.id != newTrace.chord.id;
  const bool m = oldTrace.melodic.id != newTrace.melodic.id;
  const bool o = oldTrace.motif.id != newTrace.motif.id;
  if (b || c || m || o) ++counts.changed;
  if (b) ++counts.bassChanged;
  if (c) ++counts.chordChanged;
  if (m) ++counts.melodicChanged;
  if (o) ++counts.motifChanged;
  if (!b && (c || m || o)) ++counts.bassSameDownstreamChanged;
  if (b && !c && !m && !o) ++counts.bassChangedOnly;
  if (b && c && m && o) ++counts.allFourChanged;
  if (b) {
    const uint8_t mask = static_cast<uint8_t>((c ? 4u : 0u) |
                                               (m ? 2u : 0u) |
                                               (o ? 1u : 0u));
    ++counts.combinations[mask];
  }
}

const char* causeClass(const CompositionTrace& oldTrace,
                       const CompositionTrace& newTrace) {
  const bool b = oldTrace.bass.id != newTrace.bass.id;
  const bool downstream = oldTrace.chord.id != newTrace.chord.id ||
                          oldTrace.melodic.id != newTrace.melodic.id ||
                          oldTrace.motif.id != newTrace.motif.id;
  if (!b && downstream) return "UNEXPECTED_DELTA";
  if (b && downstream) return "EXPECTED_P1_PLUS_DERIVED";
  if (b) return "EXPECTED_P1_DELTA";
  return "NO_DELTA";
}

void writeTrace(std::ofstream& out,
                GenreRecipeId recipe,
                uint16_t identity,
                uint8_t levelIndex,
                RhythmArchetypeId archetype,
                uint32_t projectSeed,
                const CompositionTrace& oldTrace,
                const CompositionTrace& newTrace) {
  out << "Acid\t" << recipeName(recipe) << '\t' << identity << '\t'
      << levelName(levelIndex) << '\t' << archetype << '\t' << projectSeed
      << '\t' << oldTrace.bass.totalWeight << '\t'
      << newTrace.bass.totalWeight << '\t'
      << oldTrace.bass.seed << '\t' << newTrace.bass.seed << '\t'
      << oldTrace.bass.draw << '\t' << newTrace.bass.draw << '\t'
      << oldTrace.bass.coordinate << '\t' << newTrace.bass.coordinate << '\t'
      << static_cast<unsigned>(oldTrace.bass.index) << '\t'
      << static_cast<unsigned>(newTrace.bass.index) << '\t'
      << static_cast<unsigned>(oldTrace.bass.id) << '\t'
      << static_cast<unsigned>(newTrace.bass.id) << '\t'
      << oldTrace.chord.seed << '\t' << newTrace.chord.seed << '\t'
      << oldTrace.chord.draw << '\t' << newTrace.chord.draw << '\t'
      << static_cast<unsigned>(oldTrace.chord.index) << '\t'
      << static_cast<unsigned>(newTrace.chord.index) << '\t'
      << static_cast<unsigned>(oldTrace.chord.id) << '\t'
      << static_cast<unsigned>(newTrace.chord.id) << '\t'
      << oldTrace.melodic.seed << '\t' << newTrace.melodic.seed << '\t'
      << oldTrace.melodic.draw << '\t' << newTrace.melodic.draw << '\t'
      << static_cast<unsigned>(oldTrace.melodic.index) << '\t'
      << static_cast<unsigned>(newTrace.melodic.index) << '\t'
      << static_cast<unsigned>(oldTrace.melodic.id) << '\t'
      << static_cast<unsigned>(newTrace.melodic.id) << '\t'
      << oldTrace.motif.seed << '\t' << newTrace.motif.seed << '\t'
      << oldTrace.motif.draw << '\t' << newTrace.motif.draw << '\t'
      << static_cast<unsigned>(oldTrace.motif.index) << '\t'
      << static_cast<unsigned>(newTrace.motif.index) << '\t'
      << static_cast<unsigned>(oldTrace.motif.id) << '\t'
      << static_cast<unsigned>(newTrace.motif.id) << '\t'
      << causeClass(oldTrace, newTrace) << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: g4_i6_r1_c1_selection_causality OUTPUT.tsv\n";
    return 2;
  }

  std::ofstream out(argv[1]);
  if (!out) return 2;
  out << "owner\tprofile\tidentity\tlevel\tarchetype_id\tproject_seed"
         "\told_bass_total_weight\tnew_bass_total_weight"
         "\tbass_seed_old\tbass_seed_new\tbass_draw_old\tbass_draw_new"
         "\tbass_coordinate_old\tbass_coordinate_new"
         "\tbass_index_old\tbass_index_new\told_bass_id\tnew_bass_id"
         "\tchord_seed_old\tchord_seed_new\tchord_draw_old\tchord_draw_new"
         "\tchord_index_old\tchord_index_new\tchord_id_old\tchord_id_new"
         "\tmelodic_seed_old\tmelodic_seed_new\tmelodic_draw_old\tmelodic_draw_new"
         "\tmelodic_index_old\tmelodic_index_new\tmelodic_id_old\tmelodic_id_new"
         "\tmotif_seed_old\tmotif_seed_new\tmotif_draw_old\tmotif_draw_new"
         "\tmotif_index_old\tmotif_index_new\tmotif_id_old\tmotif_id_new"
         "\tcause_class\n";

  constexpr GenreRecipeId recipes[] = {0, 6, 7};
  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };
  const WeightedIdentityView oldBass{
      kHistoricalAcidBass,
      static_cast<uint8_t>(sizeof(kHistoricalAcidBass) /
                           sizeof(kHistoricalAcidBass[0]))};

  DeltaCounts total{};
  std::array<DeltaCounts, 3> perOwner{};

  for (uint8_t recipeIndex = 0; recipeIndex < 3; ++recipeIndex) {
    const GenreRecipeId recipe = recipes[recipeIndex];
    const GenreSettings settings = acidSettings(recipe);
    const GenerationProfileView profile = generationProfileFor(settings);
    if (!isValidGenerationProfile(profile)) return 3;
    if (profile.bassRhythms.count != 5) return 4;

    for (uint16_t identity = 1; identity <= 128; ++identity) {
      for (uint8_t levelIndex = 0; levelIndex < 3; ++levelIndex) {
        StrongRhythmFrozenSelection frozen{};
        const StrongRhythmMigrationResult resolved =
            resolveStrongRhythmFrozenSelection(
                settings, contextFor(levels[levelIndex]), identity, frozen);
        if (resolved.status != StrongRhythmMigrationStatus::Applied ||
            !frozen.resolved ||
            frozen.composition.status != GenerationCompositionStatus::Ok) {
          return 5;
        }

        CompositionTrace historical{};
        CompositionTrace current{};
        if (!traceComposition(profile, frozen.selectionGeneration,
                              frozen.composition.rhythmArchetypeId, oldBass,
                              historical) ||
            !traceComposition(profile, frozen.selectionGeneration,
                              frozen.composition.rhythmArchetypeId,
                              profile.bassRhythms, current)) {
          return 6;
        }

        if (current.bass.id != static_cast<uint8_t>(frozen.composition.bassRhythm) ||
            current.chord.id != static_cast<uint8_t>(frozen.composition.chordRhythm) ||
            current.melodic.id != static_cast<uint8_t>(frozen.composition.melodicRhythm) ||
            current.motif.id != static_cast<uint8_t>(frozen.composition.motifShape)) {
          return 7;
        }
        if (historical.bass.seed != current.bass.seed ||
            historical.bass.draw != current.bass.draw) {
          return 8;
        }

        updateCounts(historical, current, total);
        updateCounts(historical, current, perOwner[recipeIndex]);
        writeTrace(out, recipe, identity, levelIndex,
                   frozen.composition.rhythmArchetypeId,
                   frozen.selectionGeneration.projectSeed,
                   historical, current);
      }
    }
  }

  std::cout << "G4_I6_R1_C1_ROWS " << total.rows << '\n';
  std::cout << "G4_I6_R1_C1_CHANGED " << total.changed << '\n';
  std::cout << "G4_I6_R1_C1_BASS_CHANGED " << total.bassChanged << '\n';
  std::cout << "G4_I6_R1_C1_CHORD_CHANGED " << total.chordChanged << '\n';
  std::cout << "G4_I6_R1_C1_MELODIC_CHANGED " << total.melodicChanged << '\n';
  std::cout << "G4_I6_R1_C1_MOTIF_CHANGED " << total.motifChanged << '\n';
  std::cout << "G4_I6_R1_C1_BASS_SAME_DOWNSTREAM_CHANGED "
            << total.bassSameDownstreamChanged << '\n';
  std::cout << "G4_I6_R1_C1_BASS_CHANGED_ONLY " << total.bassChangedOnly << '\n';
  std::cout << "G4_I6_R1_C1_ALL_FOUR_CHANGED " << total.allFourChanged << '\n';
  for (uint8_t mask = 0; mask < total.combinations.size(); ++mask) {
    if (total.combinations[mask] == 0) continue;
    std::cout << "G4_I6_R1_C1_COMBINATION chord=" << ((mask & 4u) ? 1 : 0)
              << " melodic=" << ((mask & 2u) ? 1 : 0)
              << " motif=" << ((mask & 1u) ? 1 : 0)
              << " rows=" << total.combinations[mask] << '\n';
  }
  for (uint8_t index = 0; index < 3; ++index) {
    std::cout << "G4_I6_R1_C1_OWNER " << recipeName(recipes[index])
              << " changed=" << perOwner[index].changed
              << " bass=" << perOwner[index].bassChanged
              << " chord=" << perOwner[index].chordChanged
              << " melodic=" << perOwner[index].melodicChanged
              << " motif=" << perOwner[index].motifChanged << '\n';
  }

  if (total.rows != 1152 || total.changed != 948 ||
      total.bassChanged != 948 || total.chordChanged != 681 ||
      total.melodicChanged != 687 || total.motifChanged != 519 ||
      total.bassSameDownstreamChanged != 0 ||
      total.bassChangedOnly != 51 || total.allFourChanged != 342) {
    return 9;
  }
  if (total.combinations[0] != 51 || total.combinations[2] != 39 ||
      total.combinations[3] != 177 || total.combinations[4] != 210 ||
      total.combinations[6] != 129 || total.combinations[7] != 342) {
    return 10;
  }
  if (perOwner[0].changed != 333 || perOwner[1].changed != 294 ||
      perOwner[2].changed != 321) {
    return 11;
  }

  std::cout << "G4-I6-R1-C1 selection causality trace: PASS\n";
  return 0;
}
