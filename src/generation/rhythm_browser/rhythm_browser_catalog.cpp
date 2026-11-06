#include "rhythm_browser_catalog.h"

#include "../composition/rhythm_selection.h"
#include "../rhythm/reference_vocabulary.h"
#include "../rhythm/rhythm_realizer.h"
#include "../../dsp/atlas_runtime.h"
#include "../../dsp/genre_manager.h"
#include "../../generated/atlas_runtime.generated.h"

namespace RhythmBrowser {
namespace {
using namespace GroovePuterRhythm;

constexpr const char* kNames[] = {
    "Straight Drive", "Offbeat Open Hat", "Hypnotic Sparse", "Broken Techno",
    "Straight Acid", "Rolling Acid", "Syncopated Acid", "Sparse Acid",
    "One Drop Space", "Steppers", "Sparse Skank", "Chord Response",
    "Two Step Roll", "Ghosted Roll", "Sparse Fast Break", "Halftime Switch",
    "Classic Two Step", "Skippy Two Step", "Shuffled Four Four",
    "Machine Syncopation", "Stacked Quarters", "Electro Backskip",
    "Funk House Bridge", "Electro Gap Push"};
static_assert(sizeof(kNames) / sizeof(kNames[0]) ==
                  static_cast<unsigned>(ReferenceVocabulary::Archetype::Count),
              "Browser names must follow reference vocabulary ordering");
static_assert(sizeof(kNames) / sizeof(kNames[0]) + AtlasGenerated::kRecipeCount <= 255,
              "Browser catalog must fit uint8_t indices");

Group atlasGroup(uint16_t id) {
  switch (id) {
    case 6: case 7: return Group::Acid;
    case 8: case 9: return Group::UkGarage;
    case 10: case 11: return Group::Dub;
    default: return Group::Count;
  }
}

bool hasDrumHits(const DrumPatternSet& drums) {
  for (const auto& voice : drums.voices)
    for (const auto& step : voice.steps)
      if (step.hit) return true;
  return false;
}

bool groupMode(Group group, GenerativeMode& mode) {
  switch (group) {
    case Group::House: mode = GenerativeMode::House; return true;
    case Group::Techno: mode = GenerativeMode::Techno; return true;
    case Group::UkGarage: mode = GenerativeMode::UkGarage; return true;
    case Group::Breaks: mode = GenerativeMode::DrumAndBass; return true;
    case Group::HipHop: mode = GenerativeMode::HipHop; return true;
    case Group::Acid: mode = GenerativeMode::Acid; return true;
    case Group::Dub: mode = GenerativeMode::Reggae; return true;
    case Group::Electro: mode = GenerativeMode::Electro; return true;
    default: return false;
  }
}
}  // namespace

uint8_t count() {
  return static_cast<uint8_t>(ReferenceVocabulary::definitionCount() +
                              AtlasGenerated::kRecipeCount);
}

bool descriptorAt(uint8_t index, Descriptor& out) {
  const uint8_t vocabularyCount = ReferenceVocabulary::definitionCount();
  if (index < vocabularyCount) {
    const auto& item = ReferenceVocabulary::definition(index);
    const auto nameIndex = static_cast<unsigned>(item.key);
    if (nameIndex >= sizeof(kNames) / sizeof(kNames[0])) return false;
    out = {Source::Vocabulary, item.archetypeId, kNames[nameIndex], 1,
           item.suggestedBpmMin, item.suggestedBpmMax,
           Provenance::VocabularyGrammar, true, Renderability::RequiresMaterialization};
    return true;
  }
  const unsigned atlasIndex = static_cast<unsigned>(index - vocabularyCount);
  if (atlasIndex >= AtlasGenerated::kRecipeCount) return false;
  const auto& item = AtlasGenerated::kRecipes[atlasIndex];
  if (!item.displayName || !item.patterns || item.patternCount == 0) return false;
  out = {Source::Atlas, item.runtimeRecipeId, item.displayName, item.patternCount, item.bpm, item.bpm,
         Provenance::CompiledAtlasRightsReview, true, Renderability::RequiresMaterialization};
  return true;
}

bool lookup(Source source, uint16_t stableId, Descriptor& out) {
  for (uint8_t i = 0; i < count(); ++i) {
    Descriptor item{};
    if (descriptorAt(i, item) && item.source == source && item.stableId == stableId) {
      out = item;
      return true;
    }
  }
  return false;
}

bool matchesGroup(const Descriptor& descriptor, Group group) {
  Descriptor item{};
  if (!lookup(descriptor.source, descriptor.stableId, item)) return false;
  if (group == Group::All) return true;
  if (group >= Group::Count) return false;
  if (item.source == Source::Atlas) return atlasGroup(item.stableId) == group;
  GenerativeMode mode{};
  if (!groupMode(group, mode)) return false;
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = kBaseRecipeId;
  return isRhythmCompatible(settings, item.stableId);
}

Status materialize(const Identity& identity, Workspace& scratch, Candidate& out) {
  if (identity.source != Source::Vocabulary && identity.source != Source::Atlas)
    return Status::InvalidSource;
  Descriptor entry{};
  if (!lookup(identity.source, identity.stableId, entry)) return Status::UnknownId;
  if (identity.variant >= entry.variantCount) return Status::InvalidVariant;

  if (identity.source == Source::Atlas) {
    if (!AtlasRuntime::materializeDrums(static_cast<uint8_t>(identity.stableId),
                                        identity.variant, scratch.material.drums))
      return Status::Unrealizable;
  } else {
    RhythmRealizationRequest request{};
    request.catalog = &ReferenceVocabulary::catalog();
    request.archetypeId = identity.stableId;
    request.phraseBars = 1;
    request.level = RealizationLevel::P1Canonical;
    request.generation = {identity.previewSeed, 0};
    const auto realized = realizeRhythmPhrase(request);
    if (realized.status != RealizationStatus::Ok &&
        realized.status != RealizationStatus::ValidButSparse)
      return Status::Unrealizable;
    const auto ignored = rhythmRoleBit(RhythmRole::BassRhythm) |
                         rhythmRoleBit(RhythmRole::ChordRhythm) |
                         rhythmRoleBit(RhythmRole::MelodicRhythm);
    const auto binding = standardDrumPatternBinding(ignored);
    if (materializeRhythmPattern(realized.plan, binding, scratch.material) !=
        PatternMaterializeStatus::Ok) return Status::Unrealizable;
  }
  if (!hasDrumHits(scratch.material.drums)) return Status::EmptyDrums;
  out.drums = scratch.material.drums;
  out.identity = identity;
  return Status::Ok;
}
}  // namespace RhythmBrowser
