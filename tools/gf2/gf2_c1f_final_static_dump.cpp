#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "platform_sdl/arduino_compat.h"
#include "scenes.h"
#include "src/dsp/drum_genre_templates.h"
#include "src/dsp/genre_manager.h"
#include "src/generated/atlas_runtime.generated.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/composition/tonal_profile.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"

using namespace GroovePuterRhythm;

SerialMock Serial;
SDMock SD;

namespace {

std::string boolValue(bool value) { return value ? "1" : "0"; }

std::string hex16(uint16_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::setw(4) << std::setfill('0') << value;
  return out.str();
}

std::string floatValue(float value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(6) << value;
  return out.str();
}

const char* genreKey(GenerativeMode mode) {
  switch (mode) {
    case GenerativeMode::Acid: return "Acid";
    case GenerativeMode::Outrun: return "Outrun";
    case GenerativeMode::Darksynth: return "Darksynth";
    case GenerativeMode::Electro: return "Electro";
    case GenerativeMode::Rave: return "Rave";
    case GenerativeMode::Reggae: return "Reggae";
    case GenerativeMode::TripHop: return "TripHop";
    case GenerativeMode::Broken: return "Broken";
    case GenerativeMode::Chip: return "Chip";
    case GenerativeMode::House: return "House";
    case GenerativeMode::Techno: return "Techno";
    case GenerativeMode::HipHop: return "HipHop";
    case GenerativeMode::FunkSoul: return "FunkSoul";
    case GenerativeMode::UkGarage: return "UkGarage";
    case GenerativeMode::DrumAndBass: return "DrumAndBass";
    case GenerativeMode::LoFi: return "LoFi";
    default: return "Unknown";
  }
}

const char* secondaryRoleName(CompositionSecondaryRole role) {
  switch (role) {
    case CompositionSecondaryRole::Chord: return "Chord";
    case CompositionSecondaryRole::Melodic: return "Melodic";
    case CompositionSecondaryRole::ChordWithMelodicFill:
      return "ChordWithMelodicFill";
    default: return "Unknown";
  }
}

const char* strongRouteName(StrongRhythmRoute route) {
  switch (route) {
    case StrongRhythmRoute::Legacy: return "Legacy";
    case StrongRhythmRoute::AcidBase: return "AcidBase";
    case StrongRhythmRoute::TechnoBase: return "TechnoBase";
    case StrongRhythmRoute::RaveBase: return "RaveBase";
    case StrongRhythmRoute::DrumAndBass: return "DrumAndBass";
    case StrongRhythmRoute::DubTechno: return "DubTechno";
    case StrongRhythmRoute::ChicagoJack: return "ChicagoJack";
    case StrongRhythmRoute::RollingAcid: return "RollingAcid";
    case StrongRhythmRoute::DeepChord: return "DeepChord";
    case StrongRhythmRoute::Stage7Composition:
      return "Stage7Composition";
    case StrongRhythmRoute::Count: break;
  }
  return "INVALID";
}

const AtlasGenerated::Recipe* atlasRecipeFor(GenreRecipeId recipe) {
  for (size_t index = 0; index < AtlasGenerated::kRecipeCount; ++index) {
    if (AtlasGenerated::kRecipes[index].runtimeRecipeId == recipe) {
      return &AtlasGenerated::kRecipes[index];
    }
  }
  return nullptr;
}

template <typename NameFunction>
std::string weightedCandidates(WeightedIdentityView view,
                               NameFunction nameFunction) {
  std::vector<WeightedIdentityCandidate> candidates(
      view.candidates, view.candidates + view.count);
  std::sort(candidates.begin(), candidates.end(),
            [](const WeightedIdentityCandidate& left,
               const WeightedIdentityCandidate& right) {
              return left.id < right.id;
            });
  std::ostringstream out;
  for (size_t index = 0; index < candidates.size(); ++index) {
    if (index != 0) out << ';';
    const char* name = nameFunction(candidates[index].id);
    out << static_cast<unsigned>(candidates[index].id) << ':'
        << (name == nullptr ? "UNKNOWN" : name) << '@'
        << static_cast<unsigned>(candidates[index].weight);
  }
  return out.str();
}

std::string rhythmCandidates(RhythmCompatibilityView view) {
  std::vector<RhythmCompatibilityCandidate> candidates(
      view.candidates, view.candidates + view.count);
  std::sort(candidates.begin(), candidates.end(),
            [](const RhythmCompatibilityCandidate& left,
               const RhythmCompatibilityCandidate& right) {
              return left.archetypeId < right.archetypeId;
            });
  std::ostringstream out;
  for (size_t index = 0; index < candidates.size(); ++index) {
    if (index != 0) out << ';';
    const char* name = rhythmSelectionName(candidates[index].archetypeId);
    out << candidates[index].archetypeId << ':'
        << (name == nullptr ? "UNKNOWN" : name) << '@'
        << static_cast<unsigned>(candidates[index].weight);
  }
  return out.str();
}

std::string phraseCandidates(WeightedIdentityView view) {
  std::vector<WeightedIdentityCandidate> candidates(
      view.candidates, view.candidates + view.count);
  std::sort(candidates.begin(), candidates.end(),
            [](const WeightedIdentityCandidate& left,
               const WeightedIdentityCandidate& right) {
              return left.id < right.id;
            });
  std::ostringstream out;
  for (size_t index = 0; index < candidates.size(); ++index) {
    if (index != 0) out << ';';
    const uint8_t id = candidates[index].id;
    const auto law = static_cast<PhraseEvolutionLawId>(id >> 4u);
    out << static_cast<unsigned>(id) << ':'
        << phraseEvolutionLawName(law) << '/'
        << static_cast<unsigned>(id & 0x0Fu) << "bar@"
        << static_cast<unsigned>(candidates[index].weight);
  }
  return out.str();
}

template <typename Enum, typename NameFunction, typename BitFunction>
std::string maskNames(uint16_t mask, Enum count,
                      NameFunction nameFunction, BitFunction bitFunction) {
  std::ostringstream out;
  bool first = true;
  for (uint8_t value = 1; value < static_cast<uint8_t>(count); ++value) {
    const Enum id = static_cast<Enum>(value);
    if ((mask & bitFunction(id)) == 0) continue;
    if (!first) out << ';';
    out << nameFunction(id);
    first = false;
  }
  return first ? "NONE" : out.str();
}

std::string bassContourNames(uint16_t mask) {
  return maskNames(mask, BassPitchContourId::Count, bassPitchContourName,
                   bassPitchContourBit);
}

std::string bassArticulationNames(uint16_t mask) {
  return maskNames(mask, BassArticulationStyleId::Count,
                   bassArticulationStyleName, bassArticulationStyleBit);
}

std::string melodicRhythmOperationNames(uint16_t mask) {
  return maskNames(mask, MelodicRhythmOperationId::Count,
                   melodicRhythmOperationName,
                   melodicRhythmOperationBit);
}

std::string melodicContourNames(uint16_t mask) {
  return maskNames(mask, MelodicContourId::Count, melodicContourName,
                   melodicContourBit);
}

std::string melodicMotifOperationNames(uint16_t mask) {
  return maskNames(mask, MelodicMotifOperationId::Count,
                   melodicMotifOperationName, melodicMotifOperationBit);
}

std::string generativeParamsPayload(const GenerativeParams& value) {
  std::ostringstream out;
  out << "minNotes=" << value.minNotes
      << "|maxNotes=" << value.maxNotes
      << "|minOctave=" << value.minOctave
      << "|maxOctave=" << value.maxOctave
      << "|slide=" << floatValue(value.slideProbability)
      << "|accent=" << floatValue(value.accentProbability)
      << "|gate=" << floatValue(value.gateLengthMultiplier)
      << "|swing=" << floatValue(value.swingAmount)
      << "|microTiming=" << floatValue(value.microTimingAmount)
      << "|velocityMin=" << value.velocityMin
      << "|velocityMax=" << value.velocityMax
      << "|preferDownbeats=" << boolValue(value.preferDownbeats)
      << "|allowRepeats=" << boolValue(value.allowRepeats)
      << "|rootBias=" << floatValue(value.rootNoteBias)
      << "|ghost=" << floatValue(value.ghostProbability)
      << "|chromatic=" << floatValue(value.chromaticProbability)
      << "|sparseKick=" << boolValue(value.sparseKick)
      << "|sparseHats=" << boolValue(value.sparseHats)
      << "|noAccents=" << boolValue(value.noAccents)
      << "|fill=" << floatValue(value.fillProbability)
      << "|drumSyncopation=" << floatValue(value.drumSyncopation)
      << "|drumPreferOffbeat=" << boolValue(value.drumPreferOffbeat)
      << "|drumVoiceCount=" << value.drumVoiceCount;
  return out.str();
}

std::string drumTemplatePayload(const DrumGenreTemplate& value) {
  std::ostringstream out;
  out << "kickMask=" << hex16(value.kickMask)
      << "|snareMask=" << hex16(value.snareMask)
      << "|hatMask=" << hex16(value.hatMask)
      << "|openHatMask=" << hex16(value.openHatMask)
      << "|kickGhost=" << floatValue(value.kickGhostProb)
      << "|snareGhost=" << floatValue(value.snareGhostProb)
      << "|hatVariation=" << floatValue(value.hatVariation)
      << "|kickVelocity=" << static_cast<unsigned>(value.kickVelBase)
      << "|snareVelocity=" << static_cast<unsigned>(value.snareVelBase)
      << "|hatVelocity=" << static_cast<unsigned>(value.hatVelBase)
      << "|useRim=" << boolValue(value.useRim)
      << "|useClap=" << boolValue(value.useClap);
  return out.str();
}

std::string behaviorPayload(const GenreBehavior& value) {
  std::ostringstream out;
  out << "stepMask=" << hex16(value.stepMask)
      << "|motifLength=" << static_cast<unsigned>(value.motifLength)
      << "|preferredScale=" << static_cast<unsigned>(value.preferredScale)
      << "|useMotif=" << boolValue(value.useMotif)
      << "|allowChromatic=" << boolValue(value.allowChromatic)
      << "|forceOctaveJump=" << boolValue(value.forceOctaveJump)
      << "|avoidClusters=" << boolValue(value.avoidClusters);
  return out.str();
}

std::string timbrePayload(const GenreBehavior& value) {
  std::ostringstream out;
  out << "osc=" << floatValue(value.timbre.osc)
      << "|cutoff=" << floatValue(value.timbre.cutoff)
      << "|resonance=" << floatValue(value.timbre.resonance)
      << "|envAmount=" << floatValue(value.timbre.envAmount)
      << "|envDecay=" << floatValue(value.timbre.envDecay);
  return out.str();
}

std::string joinSorted(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  std::ostringstream out;
  for (size_t index = 0; index < values.size(); ++index) {
    if (index != 0) out << ',';
    out << values[index];
  }
  return out.str();
}

bool isDrumRole(RhythmRole role) {
  return static_cast<uint8_t>(role) <=
         static_cast<uint8_t>(RhythmRole::Percussion);
}

std::string archetypePayload(const ReferenceVocabulary::Definition& definition,
                             const RhythmArchetype& value,
                             bool drumOnly) {
  constexpr RhythmRoleMask kDrumRoles =
      rhythmRoleBit(RhythmRole::Kick) |
      rhythmRoleBit(RhythmRole::Backbeat) |
      rhythmRoleBit(RhythmRole::ClosedHat) |
      rhythmRoleBit(RhythmRole::OpenHat) |
      rhythmRoleBit(RhythmRole::Percussion);

  std::ostringstream out;
  out << "family=" << static_cast<unsigned>(value.family)
      << "|suggestedBpmMin=" << definition.suggestedBpmMin
      << "|suggestedBpmMax=" << definition.suggestedBpmMax
      << "|allowedPhraseBars="
      << static_cast<unsigned>(value.allowedPhraseBars)
      << "|activeRoles="
      << hex16(drumOnly ? static_cast<uint16_t>(value.activeRoles & kDrumRoles)
                        : value.activeRoles);

  std::vector<std::string> lanes;
  for (uint8_t index = 0; index < value.laneCount; ++index) {
    const LaneGrammar& lane = value.lanes[index];
    if (drumOnly && !isDrumRole(lane.role)) continue;
    std::ostringstream item;
    item << static_cast<unsigned>(lane.role) << ':'
         << hex16(lane.immutableAnchors) << ':'
         << hex16(lane.canonicalAnchors) << ':'
         << hex16(lane.preferred) << ':'
         << hex16(lane.optional) << ':'
         << hex16(lane.forbidden) << ':'
         << hex16(lane.shortGate) << ':'
         << hex16(lane.heldGate) << ':'
         << hex16(lane.tieGate) << ':'
         << static_cast<unsigned>(lane.structuralMin) << ':'
         << static_cast<unsigned>(lane.structuralMax) << ':'
         << static_cast<unsigned>(lane.ornamentMax) << ':'
         << static_cast<unsigned>(lane.accentProfileId) << ':'
         << static_cast<unsigned>(lane.flags);
    lanes.push_back(item.str());
  }
  out << "|lanes=" << joinSorted(lanes);

  std::vector<std::string> spaces;
  for (uint8_t index = 0; index < value.protectedSpaceCount; ++index) {
    const ProtectedSpace& space = value.protectedSpaces[index];
    const RhythmRoleMask roles = drumOnly
        ? static_cast<RhythmRoleMask>(space.affectedRoles & kDrumRoles)
        : space.affectedRoles;
    if (drumOnly && roles == 0) continue;
    spaces.push_back(hex16(space.steps) + ':' + hex16(roles));
  }
  out << "|protected=" << joinSorted(spaces);

  std::vector<std::string> relationships;
  for (uint8_t index = 0; index < value.relationshipCount; ++index) {
    const LaneRelationship& relationship = value.relationships[index];
    if (drumOnly && !isDrumRole(relationship.source) &&
        !isDrumRole(relationship.target)) {
      continue;
    }
    std::ostringstream item;
    item << static_cast<unsigned>(relationship.source) << ':'
         << static_cast<unsigned>(relationship.target) << ':'
         << static_cast<unsigned>(relationship.op) << ':'
         << static_cast<unsigned>(relationship.strength) << ':'
         << static_cast<unsigned>(relationship.scope) << ':'
         << hex16(relationship.zoneMask) << ':'
         << static_cast<int>(relationship.minOffset) << ':'
         << static_cast<int>(relationship.maxOffset) << ':'
         << static_cast<unsigned>(relationship.minMatches) << ':'
         << static_cast<unsigned>(relationship.maxMatches) << ':'
         << static_cast<unsigned>(relationship.minResponsesPerWindow) << ':'
         << static_cast<unsigned>(relationship.maxResponsesPerWindow) << ':'
         << static_cast<unsigned>(relationship.weight);
    relationships.push_back(item.str());
  }
  out << "|relationships=" << joinSorted(relationships);

  std::vector<std::string> transforms;
  for (uint8_t index = 0; index < value.anchorTransformRuleCount; ++index) {
    const AnchorTransformRule& rule = value.anchorTransformRules[index];
    if (drumOnly && !isDrumRole(rule.role)) continue;
    std::ostringstream item;
    item << static_cast<unsigned>(rule.role) << ':'
         << static_cast<unsigned>(rule.barFunction) << ':'
         << static_cast<unsigned>(rule.intent) << ':'
         << hex16(rule.suppressibleCanonical) << ':'
         << hex16(rule.displaceableCanonical);
    transforms.push_back(item.str());
  }
  out << "|transforms=" << joinSorted(transforms);

  std::vector<std::string> trajectories;
  for (uint8_t index = 0; index < value.trajectoryCount; ++index) {
    const TrajectoryRef& trajectory = value.trajectories[index];
    std::ostringstream item;
    item << static_cast<unsigned>(trajectory.id) << ':'
         << static_cast<unsigned>(trajectory.weight) << ':'
         << static_cast<unsigned>(trajectory.allowedLevels);
    trajectories.push_back(item.str());
  }
  out << "|trajectories=" << joinSorted(trajectories);

  const RhythmRoleMask timingRoles = drumOnly
      ? static_cast<RhythmRoleMask>(value.timing.affectedRoles & kDrumRoles)
      : value.timing.affectedRoles;
  out << "|timing=" << static_cast<unsigned>(value.timing.compatibility)
      << ':' << hex16(value.timing.sensitiveSteps)
      << ':' << hex16(timingRoles)
      << "|density=" << static_cast<unsigned>(value.density.structuralMin)
      << ':' << static_cast<unsigned>(value.density.structuralPreferred)
      << ':' << static_cast<unsigned>(value.density.structuralMax)
      << ':' << static_cast<unsigned>(value.density.ornamentMax);

  out << "|mutation=";
  for (uint8_t level = 0;
       level < static_cast<uint8_t>(RealizationLevel::Count); ++level) {
    if (level != 0) out << ',';
    const MutationBudget& budget = value.mutation.level[level];
    out << static_cast<unsigned>(budget.maxAdds) << ':'
        << static_cast<unsigned>(budget.maxDrops) << ':'
        << static_cast<unsigned>(budget.maxDisplacements) << ':'
        << static_cast<unsigned>(budget.maxAccentChanges) << ':'
        << budget.flags << ':'
        << static_cast<unsigned>(budget.allowedIntents) << ':'
        << static_cast<unsigned>(budget.maxSecondaryAdds) << ':'
        << static_cast<unsigned>(budget.maxGhostAdds);
  }
  return out.str();
}

void dumpArchetypes() {
  std::cout << "archetype_id\tarchetype_key\tname\tfamily\tbpm_min\tbpm_max"
               "\tsemantic_payload\tdrum_payload\n";
  for (uint8_t index = 0;
       index < ReferenceVocabulary::definitionCount(); ++index) {
    const ReferenceVocabulary::Definition& definition =
        ReferenceVocabulary::definition(index);
    const RhythmArchetype* archetype =
        ReferenceVocabulary::archetypeFor(definition.key);
    if (archetype == nullptr) continue;
    std::cout << definition.archetypeId << '\t'
              << static_cast<unsigned>(definition.key) << '\t'
              << definition.name << '\t'
              << static_cast<unsigned>(definition.family) << '\t'
              << definition.suggestedBpmMin << '\t'
              << definition.suggestedBpmMax << '\t'
              << archetypePayload(definition, *archetype, false) << '\t'
              << archetypePayload(definition, *archetype, true) << '\n';
  }
}

void dumpProfiles() {
  std::cout
      << "row_index\tgenre_id\tgenre_key\tgenre_display\trecipe_id"
         "\trecipe_name\tis_base\trhythms\tfeels\tbass\tchord"
         "\tprogressions\tmelodic\tmotifs\tphrases\tbpm_min\tbpm_max"
         "\tbpm_suggested\tgrid_steps\tdensity_min\tdensity_max"
         "\tsecondary_role\tstrong_rhythm_route"
         "\tbass_allowed_contours_mask"
         "\tbass_allowed_contours\tbass_preferred_contours_mask"
         "\tbass_preferred_contours\tbass_allowed_articulations_mask"
         "\tbass_allowed_articulations\tbass_preferred_articulations_mask"
         "\tbass_preferred_articulations\tmelodic_allowed_rhythm_ops_mask"
         "\tmelodic_allowed_rhythm_ops\tmelodic_preferred_rhythm_ops_mask"
         "\tmelodic_preferred_rhythm_ops\tmelodic_allowed_contours_mask"
         "\tmelodic_allowed_contours\tmelodic_preferred_contours_mask"
         "\tmelodic_preferred_contours\tmelodic_allowed_motif_ops_mask"
         "\tmelodic_allowed_motif_ops\tmelodic_preferred_motif_ops_mask"
         "\tmelodic_preferred_motif_ops\tbass_register_min"
         "\tbass_register_max\tbass_register_max_leap"
         "\tsecondary_register_min\tsecondary_register_max"
         "\tsecondary_register_max_leap\tlegacy_params"
         "\tlegacy_drum_source\tlegacy_drum_template\tlegacy_behavior"
         "\tlegacy_timbre\tatlas_backed\tatlas_recipe_id"
         "\tatlas_display_name\tatlas_bpm\tatlas_swing_percent"
         "\tatlas_variation_count\n";

  uint16_t rowIndex = 0;
  for (uint8_t genreValue = 0; genreValue < kGenerativeModeCount;
       ++genreValue) {
    const auto genre = static_cast<GenerativeMode>(genreValue);
    for (GenreRecipeId recipe = 0; recipe < GenreCatalog::recipeCount();
         ++recipe) {
      GenreSettings settings{};
      settings.generativeMode = genreValue;
      settings.recipe = recipe;
      settings.morphTarget = kBaseRecipeId;
      settings.morphAmount = 0;
      settings.rhythmSelectionMode =
          static_cast<uint8_t>(RhythmSelectionMode::Auto);
      settings.rhythmArchetypeId = kNoArchetypeId;

      const GenerationProfileView profile = generationProfileFor(settings);
      if (profile.generativeMode != genreValue || profile.recipe != recipe) {
        continue;
      }

      const TonalGenerationProfile tonal =
          tonalGenerationProfileFor(settings);
      const GenerativeParams params =
          GenreCatalog::compiledGenerativeParams(settings);
      const DrumGenreTemplate* drumOverride =
          GenreCatalog::drumTemplateOverride(settings);
      const DrumGenreTemplate& drum = drumOverride != nullptr
          ? *drumOverride
          : kDrumTemplates[genreValue];
      const GenreBehavior behavior = GenreCatalog::behavior(settings);
      const AtlasGenerated::Recipe* atlas = atlasRecipeFor(recipe);

      std::cout
          << rowIndex++ << '\t'
          << static_cast<unsigned>(genreValue) << '\t'
          << genreKey(genre) << '\t'
          << GenreCatalog::generativeModeName(genre) << '\t'
          << static_cast<unsigned>(recipe) << '\t'
          << (recipe == kBaseRecipeId ? "BASE"
                                      : GenreCatalog::recipeName(recipe))
          << '\t' << boolValue(recipe == kBaseRecipeId) << '\t'
          << rhythmCandidates(profile.rhythms) << '\t'
          << weightedCandidates(profile.feels, [](uint8_t id) {
               return feelProfileName(static_cast<FeelProfileId>(id));
             }) << '\t'
          << weightedCandidates(profile.bassRhythms, [](uint8_t id) {
               return bassRhythmName(static_cast<BassRhythmId>(id));
             }) << '\t'
          << weightedCandidates(profile.chordRhythms, [](uint8_t id) {
               return chordRhythmName(static_cast<ChordRhythmId>(id));
             }) << '\t'
          << weightedCandidates(profile.progressions, [](uint8_t id) {
               return chordProgressionName(static_cast<ProgressionId>(id));
             }) << '\t'
          << weightedCandidates(profile.melodicRhythms, [](uint8_t id) {
               return melodicRhythmName(static_cast<MelodicRhythmId>(id));
             }) << '\t'
          << weightedCandidates(profile.motifShapes, [](uint8_t id) {
               return motifShapeName(static_cast<MotifShapeId>(id));
             }) << '\t'
          << phraseCandidates(profile.phraseLaws) << '\t'
          << profile.corridor.bpmMin << '\t'
          << profile.corridor.bpmMax << '\t'
          << profile.corridor.suggestedBpm << '\t'
          << static_cast<unsigned>(profile.corridor.gridSteps) << '\t'
          << static_cast<unsigned>(profile.corridor.densityMin) << '\t'
          << static_cast<unsigned>(profile.corridor.densityMax) << '\t'
          << secondaryRoleName(profile.secondaryRole) << '\t'
          << strongRouteName(selectStrongRhythmRoute(settings)) << '\t'
          << hex16(tonal.bassPolicy.allowedContours) << '\t'
          << bassContourNames(tonal.bassPolicy.allowedContours) << '\t'
          << hex16(tonal.bassPolicy.preferredContours) << '\t'
          << bassContourNames(tonal.bassPolicy.preferredContours) << '\t'
          << hex16(tonal.bassPolicy.allowedArticulations) << '\t'
          << bassArticulationNames(
                 tonal.bassPolicy.allowedArticulations) << '\t'
          << hex16(tonal.bassPolicy.preferredArticulations) << '\t'
          << bassArticulationNames(
                 tonal.bassPolicy.preferredArticulations) << '\t'
          << hex16(tonal.melodicPolicy.allowedRhythmOperations) << '\t'
          << melodicRhythmOperationNames(
                 tonal.melodicPolicy.allowedRhythmOperations) << '\t'
          << hex16(tonal.melodicPolicy.preferredRhythmOperations) << '\t'
          << melodicRhythmOperationNames(
                 tonal.melodicPolicy.preferredRhythmOperations) << '\t'
          << hex16(tonal.melodicPolicy.allowedContours) << '\t'
          << melodicContourNames(
                 tonal.melodicPolicy.allowedContours) << '\t'
          << hex16(tonal.melodicPolicy.preferredContours) << '\t'
          << melodicContourNames(
                 tonal.melodicPolicy.preferredContours) << '\t'
          << hex16(tonal.melodicPolicy.allowedMotifOperations) << '\t'
          << melodicMotifOperationNames(
                 tonal.melodicPolicy.allowedMotifOperations) << '\t'
          << hex16(tonal.melodicPolicy.preferredMotifOperations) << '\t'
          << melodicMotifOperationNames(
                 tonal.melodicPolicy.preferredMotifOperations) << '\t'
          << static_cast<unsigned>(tonal.bassRegister.minMidi) << '\t'
          << static_cast<unsigned>(tonal.bassRegister.maxMidi) << '\t'
          << static_cast<unsigned>(
                 tonal.bassRegister.maxAdjacentLeapSemitones) << '\t'
          << static_cast<unsigned>(tonal.secondaryRegister.minMidi) << '\t'
          << static_cast<unsigned>(tonal.secondaryRegister.maxMidi) << '\t'
          << static_cast<unsigned>(
                 tonal.secondaryRegister.maxAdjacentLeapSemitones) << '\t'
          << generativeParamsPayload(params) << '\t'
          << (drumOverride == nullptr ? "BASE_MODE_TEMPLATE"
                                      : "RECIPE_OVERRIDE") << '\t'
          << drumTemplatePayload(drum) << '\t'
          << behaviorPayload(behavior) << '\t'
          << timbrePayload(behavior) << '\t'
          << boolValue(atlas != nullptr) << '\t'
          << (atlas == nullptr ? "NONE" : atlas->atlasRecipeId) << '\t'
          << (atlas == nullptr ? "NONE" : atlas->displayName) << '\t'
          << (atlas == nullptr ? 0 : atlas->bpm) << '\t'
          << (atlas == nullptr
                  ? 0
                  : static_cast<unsigned>(atlas->swingPercent)) << '\t'
          << (atlas == nullptr
                  ? 0
                  : static_cast<unsigned>(atlas->patternCount)) << '\n';
    }
  }

  if (rowIndex != 33) {
    std::cerr << "expected 33 production profile rows, found " << rowIndex
              << '\n';
    std::exit(3);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: gf2_c1f_final_static_dump profiles|archetypes\n";
    return 2;
  }
  const std::string command = argv[1];
  if (command == "profiles") {
    dumpProfiles();
    return 0;
  }
  if (command == "archetypes") {
    dumpArchetypes();
    return 0;
  }
  std::cerr << "unknown command: " << command << '\n';
  return 2;
}
