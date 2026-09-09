#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/phrase_execution.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/rhythm_types.h"
#include "tests/support/gf2_generation_observation.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kFirstIdentity = 1;
constexpr uint16_t kIdentityCount = 128;
constexpr uint8_t kLevelCount = 3;

struct Pilot {
  const char* name;
  GenerativeMode mode;
  uint8_t recipe;
};

constexpr Pilot kPilots[] = {
    {"Acid", GenerativeMode::Acid, kBaseRecipeId},
    {"House", GenerativeMode::House, kBaseRecipeId},
    {"DubTechno", GenerativeMode::Reggae, 5},
    {"DnB", GenerativeMode::DrumAndBass, kBaseRecipeId},
};

constexpr std::array<RealizationLevel, kLevelCount> kLevels = {
    RealizationLevel::P1Canonical,
    RealizationLevel::P2Variation,
    RealizationLevel::P3Transformation,
};

constexpr const char* kLevelNames[kLevelCount] = {"P1", "P2", "P3"};

uint32_t mixByte(uint32_t hash, uint8_t value) {
  return (hash ^ static_cast<uint32_t>(value)) * 16777619u;
}

uint32_t mixMask(uint32_t hash, StepMask value) {
  hash = mixByte(hash, static_cast<uint8_t>(value >> 8u));
  return mixByte(hash, static_cast<uint8_t>(value & 0xFFu));
}

uint32_t mix32(uint32_t hash, uint32_t value) {
  hash = mixByte(hash, static_cast<uint8_t>((value >> 24u) & 0xFFu));
  hash = mixByte(hash, static_cast<uint8_t>((value >> 16u) & 0xFFu));
  hash = mixByte(hash, static_cast<uint8_t>((value >> 8u) & 0xFFu));
  return mixByte(hash, static_cast<uint8_t>(value & 0xFFu));
}

uint8_t countBits(StepMask value) {
  uint8_t count = 0;
  while (value != 0) {
    value = static_cast<StepMask>(value & static_cast<StepMask>(value - 1u));
    ++count;
  }
  return count;
}

GenreSettings settingsFor(const Pilot& pilot) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(pilot.mode);
  settings.recipe = pilot.recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(RealizationLevel level) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = 0;
  context.level = level;
  context.generationAttemptOrdinal = 0;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

PhraseExecutionMaterializationSettings phraseSettings(RealizationLevel level) {
  PhraseExecutionMaterializationSettings settings{};
  settings.level = level;
  settings.generationAttemptOrdinal = 0;
  settings.feelProfile = FeelProfileId::Straight;
  settings.feelAmount = 0;
  settings.tonalMaterializationEnabled = true;
  settings.rootPitchClass = 0;
  settings.scaleTypeValue = kScaleDorian;
  return settings;
}

StepMask drumOnsets(const DrumPatternSet& drums, uint8_t voice) {
  StepMask result = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[voice].steps[step].hit) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
}

StepMask synthOnsets(const SynthPattern& synth) {
  StepMask result = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (synth.steps[step].note >= 0) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
}

uint32_t relativePitchFingerprint(const SynthPattern& synth) {
  uint32_t hash = 2166136261u;
  bool havePrevious = false;
  int8_t previous = 0;
  uint8_t noteCount = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const int8_t note = synth.steps[step].note;
    if (note < 0) continue;
    ++noteCount;
    if (havePrevious) {
      const int16_t interval = static_cast<int16_t>(note) - previous;
      hash = mixByte(hash, static_cast<uint8_t>(interval + 128));
    }
    previous = note;
    havePrevious = true;
  }
  return mixByte(hash, noteCount);
}

uint32_t roleStructureFingerprint(const DrumPatternSet& drums,
                                  const SynthPattern& synthA,
                                  const SynthPattern& synthB) {
  uint32_t hash = 2166136261u;
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    hash = mixMask(hash, drumOnsets(drums, voice));
  }
  hash = mixMask(hash, synthOnsets(synthA));
  hash = mixMask(hash, synthOnsets(synthB));
  hash = mix32(hash, relativePitchFingerprint(synthA));
  hash = mix32(hash, relativePitchFingerprint(synthB));
  return hash;
}

StepMask clickStructure(const DrumPatternSet& drums,
                        const SynthPattern& synthA,
                        const SynthPattern& synthB) {
  StepMask result = static_cast<StepMask>(
      synthOnsets(synthA) | synthOnsets(synthB));
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    result = static_cast<StepMask>(result | drumOnsets(drums, voice));
  }
  return result;
}

uint32_t semanticFingerprint(const GenerationCompositionResult& composition) {
  uint32_t hash = 2166136261u;
  hash = mixMask(hash, composition.rhythmArchetypeId);
  hash = mixByte(hash, static_cast<uint8_t>(composition.bassRhythm));
  hash = mixByte(hash, static_cast<uint8_t>(composition.chordRhythm));
  hash = mixByte(hash, static_cast<uint8_t>(composition.progression));
  hash = mixByte(hash, static_cast<uint8_t>(composition.melodicRhythm));
  hash = mixByte(hash, static_cast<uint8_t>(composition.motifShape));
  hash = mixByte(hash, static_cast<uint8_t>(composition.phraseLaw));
  hash = mixByte(hash, composition.phraseBars);
  hash = mixByte(hash, static_cast<uint8_t>(composition.harmonicChangeRate));
  hash = mixByte(hash, static_cast<uint8_t>(composition.secondaryRole));
  return hash;
}

struct Sample {
  bool ready = false;
  uint16_t identity = 0;
  uint8_t levelIndex = 0;
  uint32_t semantic = 0;
  uint32_t surface = 0;
  uint32_t roleStructure = 0;
  StepMask click = 0;
  RhythmArchetypeId archetype = kNoArchetypeId;
  BassRhythmId bass = BassRhythmId::Auto;
  ChordRhythmId chord = ChordRhythmId::Auto;
  ProgressionId progression = ProgressionId::Auto;
  MelodicRhythmId melodic = MelodicRhythmId::Auto;
  MotifShapeId motif = MotifShapeId::Auto;
  PhraseEvolutionLawId phraseLaw = PhraseEvolutionLawId::Loop;
  uint8_t phraseBars = 0;
  StepMask kick = 0;
  uint8_t drumHits = 0;
  uint8_t bassOnsets = 0;
  uint8_t secondaryOnsets = 0;
};

Sample materializeSample(const Pilot& pilot, uint16_t identity,
                         uint8_t levelIndex) {
  Sample sample{};
  sample.identity = identity;
  sample.levelIndex = levelIndex;

  const GenreSettings settings = settingsFor(pilot);
  const StrongRhythmMigrationContext context = contextFor(kLevels[levelIndex]);
  StrongRhythmFrozenSelection selection{};
  const StrongRhythmMigrationResult selected =
      resolveStrongRhythmFrozenSelection(settings, context, identity, selection);
  if (selected.status != StrongRhythmMigrationStatus::Applied ||
      !selection.resolved) {
    return sample;
  }

  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  const StrongRhythmMigrationResult materialized =
      migrateStrongRhythmFrozenMaterial(
          settings, selection, context, drums, synthA, synthB);
  if (materialized.status != StrongRhythmMigrationStatus::Applied) {
    return sample;
  }

  sample.ready = true;
  sample.semantic = semanticFingerprint(selection.composition);
  sample.surface = GF2Measurement::materialFingerprint(drums, synthA, synthB);
  sample.roleStructure = roleStructureFingerprint(drums, synthA, synthB);
  sample.click = clickStructure(drums, synthA, synthB);
  sample.archetype = selection.composition.rhythmArchetypeId;
  sample.bass = selection.composition.bassRhythm;
  sample.chord = selection.composition.chordRhythm;
  sample.progression = selection.composition.progression;
  sample.melodic = selection.composition.melodicRhythm;
  sample.motif = selection.composition.motifShape;
  sample.phraseLaw = selection.composition.phraseLaw;
  sample.phraseBars = selection.composition.phraseBars;
  sample.kick = drumOnsets(drums, KICK);
  sample.bassOnsets = countBits(synthOnsets(synthA));
  sample.secondaryOnsets = countBits(synthOnsets(synthB));
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    sample.drumHits = static_cast<uint8_t>(
        sample.drumHits + countBits(drumOnsets(drums, voice)));
  }
  return sample;
}

void printSample(const char* prefix, const Pilot& pilot, const Sample& sample) {
  std::printf(
      "%s pilot=%s identity=%u level=%s semantic=%08x surface=%08x role=%08x click=%04x archetype=%u bass=%u chord=%u progression=%u melodic=%u motif=%u law=%u bars=%u\n",
      prefix,
      pilot.name,
      static_cast<unsigned>(sample.identity),
      kLevelNames[sample.levelIndex],
      static_cast<unsigned>(sample.semantic),
      static_cast<unsigned>(sample.surface),
      static_cast<unsigned>(sample.roleStructure),
      static_cast<unsigned>(sample.click),
      static_cast<unsigned>(sample.archetype),
      static_cast<unsigned>(sample.bass),
      static_cast<unsigned>(sample.chord),
      static_cast<unsigned>(sample.progression),
      static_cast<unsigned>(sample.melodic),
      static_cast<unsigned>(sample.motif),
      static_cast<unsigned>(sample.phraseLaw),
      static_cast<unsigned>(sample.phraseBars));
}

struct LevelSummary {
  uint16_t ready = 0;
  std::set<uint32_t> semantic;
  std::set<uint32_t> surface;
  std::set<uint32_t> roleStructure;
  std::set<StepMask> click;
  uint16_t fourFloorKick = 0;
  uint32_t drumHitTotal = 0;
  uint32_t bassOnsetTotal = 0;
  uint32_t secondaryOnsetTotal = 0;
};

void printCollapseWitnesses(const Pilot& pilot,
                            const std::vector<Sample>& samples) {
  std::map<uint32_t, Sample> firstByRole;
  std::map<StepMask, Sample> firstByClick;
  bool printedSurfaceRole = false;
  bool printedSemanticRole = false;
  bool printedRoleClick = false;

  for (const Sample& sample : samples) {
    if (!sample.ready) continue;

    const auto roleIt = firstByRole.find(sample.roleStructure);
    if (roleIt == firstByRole.end()) {
      firstByRole.emplace(sample.roleStructure, sample);
    } else {
      const Sample& first = roleIt->second;
      if (!printedSurfaceRole && first.surface != sample.surface) {
        printSample("G4_C0 WITNESS surface_diff_role_same A", pilot, first);
        printSample("G4_C0 WITNESS surface_diff_role_same B", pilot, sample);
        printedSurfaceRole = true;
      }
      if (!printedSemanticRole && first.semantic != sample.semantic) {
        printSample("G4_C0 WITNESS semantic_diff_role_same A", pilot, first);
        printSample("G4_C0 WITNESS semantic_diff_role_same B", pilot, sample);
        printedSemanticRole = true;
      }
    }

    const auto clickIt = firstByClick.find(sample.click);
    if (clickIt == firstByClick.end()) {
      firstByClick.emplace(sample.click, sample);
    } else if (!printedRoleClick &&
               clickIt->second.roleStructure != sample.roleStructure) {
      printSample("G4_C0 WITNESS role_diff_click_same A", pilot, clickIt->second);
      printSample("G4_C0 WITNESS role_diff_click_same B", pilot, sample);
      printedRoleClick = true;
    }
  }
}

struct PhraseSummary {
  uint16_t ready = 0;
  uint16_t trajectory = 0;
  uint16_t roleDeveloping = 0;
  uint16_t clickDeveloping = 0;
  uint16_t roleStatic = 0;
  uint16_t clickStatic = 0;
  uint16_t totalBars = 0;
  uint16_t silentSlotPhrases = 0;
};

PhraseSummary measurePhraseCorpus(const Pilot& pilot) {
  PhraseSummary summary{};
  const GenreSettings settings = settingsFor(pilot);
  const PhraseExecutionMaterializationSettings materialization =
      phraseSettings(RealizationLevel::P2Variation);

  for (uint16_t identity = kFirstIdentity;
       identity < static_cast<uint16_t>(kFirstIdentity + kIdentityCount);
       ++identity) {
    StrongRhythmFrozenSelection selection{};
    const StrongRhythmMigrationResult selected =
        resolveStrongRhythmFrozenSelection(
            settings, contextFor(RealizationLevel::P2Variation),
            identity, selection);
    if (selected.status != StrongRhythmMigrationStatus::Applied ||
        !selection.resolved || selection.composition.phraseBars == 0) {
      continue;
    }

    PhraseExecutionScratch scratch{};
    PreparedPhraseExecution prepared{};
    const uint8_t bars = selection.composition.phraseBars;
    const PhraseExecutionStatus status = preparePhraseExecution(
        settings, materialization, identity, bars, scratch, prepared);
    if (status != PhraseExecutionStatus::Ready ||
        prepared.length.effectivePhraseBars != bars) {
      continue;
    }

    ++summary.ready;
    summary.totalBars = static_cast<uint16_t>(summary.totalBars + bars);
    if (prepared.phraseTrajectory != kNoTrajectoryId) ++summary.trajectory;

    std::set<uint32_t> roleBars;
    std::set<StepMask> clickBars;
    bool hasSilentSlot = false;
    for (uint8_t bar = 0; bar < bars; ++bar) {
      DrumPatternSet drums{};
      SynthPattern synthA{};
      SynthPattern synthB{};
      const StrongRhythmMigrationResult materialized =
          materializePreparedPhraseBar(
              prepared, bar, static_cast<int16_t>(16 + bar),
              drums, synthA, synthB);
      if (materialized.status != StrongRhythmMigrationStatus::Applied) {
        roleBars.clear();
        clickBars.clear();
        break;
      }
      roleBars.insert(roleStructureFingerprint(drums, synthA, synthB));
      const StepMask click = clickStructure(drums, synthA, synthB);
      clickBars.insert(click);
      if (countBits(click) < kStepsPerBar) hasSilentSlot = true;
    }

    if (roleBars.empty() || clickBars.empty()) continue;
    if (roleBars.size() > 1) {
      ++summary.roleDeveloping;
    } else {
      ++summary.roleStatic;
    }
    if (clickBars.size() > 1) {
      ++summary.clickDeveloping;
    } else {
      ++summary.clickStatic;
    }
    if (hasSilentSlot) ++summary.silentSlotPhrases;
  }
  return summary;
}

int runPilot(const Pilot& pilot) {
  const GenerationProfileView profile = generationProfileFor(settingsFor(pilot));
  if (!isValidGenerationProfile(profile)) {
    std::fprintf(stderr, "G4-C0 FAIL: invalid profile for %s\n", pilot.name);
    return 1;
  }

  std::array<LevelSummary, kLevelCount> levels{};
  std::vector<Sample> allSamples;
  allSamples.reserve(static_cast<size_t>(kIdentityCount) * kLevelCount);
  std::array<std::array<Sample, kLevelCount>, kIdentityCount> byIdentity{};

  for (uint16_t offset = 0; offset < kIdentityCount; ++offset) {
    const uint16_t identity = static_cast<uint16_t>(kFirstIdentity + offset);
    for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
      const Sample sample = materializeSample(pilot, identity, levelIndex);
      byIdentity[offset][levelIndex] = sample;
      allSamples.push_back(sample);
      if (!sample.ready) continue;

      LevelSummary& summary = levels[levelIndex];
      ++summary.ready;
      summary.semantic.insert(sample.semantic);
      summary.surface.insert(sample.surface);
      summary.roleStructure.insert(sample.roleStructure);
      summary.click.insert(sample.click);
      summary.drumHitTotal += sample.drumHits;
      summary.bassOnsetTotal += sample.bassOnsets;
      summary.secondaryOnsetTotal += sample.secondaryOnsets;
      const StepMask fourFloor = static_cast<StepMask>(
          stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
      if ((sample.kick & fourFloor) == fourFloor) ++summary.fourFloorKick;
    }
  }

  uint16_t semanticStable = 0;
  uint16_t surfaceStable = 0;
  uint16_t roleStable = 0;
  uint16_t clickStable = 0;
  uint16_t allReady = 0;
  for (uint16_t offset = 0; offset < kIdentityCount; ++offset) {
    const Sample& p1 = byIdentity[offset][0];
    const Sample& p2 = byIdentity[offset][1];
    const Sample& p3 = byIdentity[offset][2];
    if (!p1.ready || !p2.ready || !p3.ready) continue;
    ++allReady;
    if (p1.semantic == p2.semantic && p2.semantic == p3.semantic) {
      ++semanticStable;
    }
    if (p1.surface == p2.surface && p2.surface == p3.surface) {
      ++surfaceStable;
    }
    if (p1.roleStructure == p2.roleStructure &&
        p2.roleStructure == p3.roleStructure) {
      ++roleStable;
    }
    if (p1.click == p2.click && p2.click == p3.click) {
      ++clickStable;
    }
  }

  std::printf(
      "G4_C0 PILOT name=%s mode=%u recipe=%u bpm=%u/%u/%u density=%u/%u identities=%u\n",
      pilot.name,
      static_cast<unsigned>(pilot.mode),
      static_cast<unsigned>(pilot.recipe),
      static_cast<unsigned>(profile.corridor.bpmMin),
      static_cast<unsigned>(profile.corridor.suggestedBpm),
      static_cast<unsigned>(profile.corridor.bpmMax),
      static_cast<unsigned>(profile.corridor.densityMin),
      static_cast<unsigned>(profile.corridor.densityMax),
      static_cast<unsigned>(kIdentityCount));

  for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
    const LevelSummary& summary = levels[levelIndex];
    const uint32_t divisor = summary.ready == 0 ? 1u : summary.ready;
    std::printf(
        "G4_C0 LEVEL pilot=%s level=%s ready=%u semantic_unique=%zu surface_unique=%zu role_unique=%zu click_unique=%zu four_floor=%u avg_drum_hits_x100=%u avg_bass_onsets_x100=%u avg_secondary_onsets_x100=%u\n",
        pilot.name,
        kLevelNames[levelIndex],
        static_cast<unsigned>(summary.ready),
        summary.semantic.size(),
        summary.surface.size(),
        summary.roleStructure.size(),
        summary.click.size(),
        static_cast<unsigned>(summary.fourFloorKick),
        static_cast<unsigned>((summary.drumHitTotal * 100u) / divisor),
        static_cast<unsigned>((summary.bassOnsetTotal * 100u) / divisor),
        static_cast<unsigned>((summary.secondaryOnsetTotal * 100u) / divisor));
  }

  std::printf(
      "G4_C0 PLEVEL pilot=%s all_ready=%u semantic_stable=%u surface_stable=%u role_stable=%u click_stable=%u\n",
      pilot.name,
      static_cast<unsigned>(allReady),
      static_cast<unsigned>(semanticStable),
      static_cast<unsigned>(surfaceStable),
      static_cast<unsigned>(roleStable),
      static_cast<unsigned>(clickStable));

  printCollapseWitnesses(pilot, allSamples);

  const PhraseSummary phrase = measurePhraseCorpus(pilot);
  std::printf(
      "G4_C0 PHRASE pilot=%s ready=%u trajectory=%u role_developing=%u click_developing=%u role_static=%u click_static=%u total_bars=%u silent_slot_phrases=%u\n",
      pilot.name,
      static_cast<unsigned>(phrase.ready),
      static_cast<unsigned>(phrase.trajectory),
      static_cast<unsigned>(phrase.roleDeveloping),
      static_cast<unsigned>(phrase.clickDeveloping),
      static_cast<unsigned>(phrase.roleStatic),
      static_cast<unsigned>(phrase.clickStatic),
      static_cast<unsigned>(phrase.totalBars),
      static_cast<unsigned>(phrase.silentSlotPhrases));

  if (allReady != kIdentityCount) {
    std::fprintf(stderr,
                 "G4-C0 FAIL: %s did not materialize all identities at all levels (%u/%u)\n",
                 pilot.name,
                 static_cast<unsigned>(allReady),
                 static_cast<unsigned>(kIdentityCount));
    return 1;
  }
  if (semanticStable != allReady) {
    std::fprintf(stderr,
                 "G4-C0 FAIL: %s changed composition identity across P-levels (%u/%u stable)\n",
                 pilot.name,
                 static_cast<unsigned>(semanticStable),
                 static_cast<unsigned>(allReady));
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
  int failures = 0;
  for (const Pilot& pilot : kPilots) failures += runPilot(pilot);
  if (failures == 0) {
    std::puts("G4_C0 STATUS measurement_complete");
    return 0;
  }
  std::fprintf(stderr, "G4_C0 STATUS measurement_failed failures=%d\n", failures);
  return 1;
}
