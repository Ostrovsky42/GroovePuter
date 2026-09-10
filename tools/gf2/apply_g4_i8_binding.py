#!/usr/bin/env python3
from pathlib import Path

path = Path("src/generation/composition/generation_profile.cpp")
text = path.read_text(encoding="utf-8")

old_dub = '''constexpr WeightedIdentityCandidate kBassDub[] = {
    weighted(BassRhythmId::KickAnswer, 85),
    weighted(BassRhythmId::GapFill, 75),
    weighted(BassRhythmId::SparseAnchor, 120),
    weighted(BassRhythmId::SustainAndDrop, 100),
};'''
new_dub = '''constexpr WeightedIdentityCandidate kBassDub[] = {
    weighted(BassRhythmId::KickAnswer, 85),
    // Dub Techno admits FourFloor as one structural carrier. Keep a second
    // restrained FourFloor-compatible contour so compatibility filtering does
    // not collapse that carrier to SustainAndDrop only.
    weighted(BassRhythmId::OffbeatPush, 70),
    weighted(BassRhythmId::GapFill, 75),
    weighted(BassRhythmId::SparseAnchor, 120),
    weighted(BassRhythmId::SustainAndDrop, 100),
};'''

old_helper = '''uint32_t profileSalt(const GenerationProfileView& profile) {
  return (static_cast<uint32_t>(profile.generativeMode) << 24u) |
         (static_cast<uint32_t>(profile.recipe) << 16u);
}'''
new_helper = '''WeightedIdentityView bassCandidatesForFamily(
    WeightedIdentityView input, RhythmFamily family,
    WeightedIdentityCandidate* storage, uint8_t capacity) {
  if (input.candidates == nullptr || storage == nullptr || input.count == 0 ||
      input.count > capacity) {
    return {};
  }
  uint8_t count = 0;
  for (uint8_t index = 0; index < input.count; ++index) {
    const WeightedIdentityCandidate candidate = input.candidates[index];
    if (!isBassRhythmCompatibleWithFamily(
            family, static_cast<BassRhythmId>(candidate.id))) {
      continue;
    }
    storage[count++] = candidate;
  }
  return {storage, count};
}

uint32_t profileSalt(const GenerationProfileView& profile) {
  return (static_cast<uint32_t>(profile.generativeMode) << 24u) |
         (static_cast<uint32_t>(profile.recipe) << 16u);
}'''

old_selection = '''  const uint32_t baseSalt = profileSalt(profile);
  uint8_t feel=0,bass=0,chord=0,progression=0,melodic=0,motif=0,phraseChoice=0;
  if (!selectWeightedIdentityFromView(profile.feels, GenerationDomain::FeelProfileSelection, rhythm.archetypeId, baseSalt, generation, feel) ||
      !selectWeightedIdentityFromView(profile.bassRhythms, GenerationDomain::BassRhythmSelection, rhythm.archetypeId, baseSalt, generation, bass) ||'''
new_selection = '''  const ReferenceVocabulary::Definition* rhythmDefinition =
      ReferenceVocabulary::definitionForId(rhythm.archetypeId);
  if (rhythmDefinition == nullptr) {
    result.status = GenerationCompositionStatus::InvalidProfile;
    return result;
  }
  WeightedIdentityCandidate compatibleBassStorage[kMaxWeightedCandidates]{};
  const WeightedIdentityView compatibleBass = bassCandidatesForFamily(
      profile.bassRhythms, rhythmDefinition->family,
      compatibleBassStorage, kMaxWeightedCandidates);

  const uint32_t baseSalt = profileSalt(profile);
  uint8_t feel=0,bass=0,chord=0,progression=0,melodic=0,motif=0,phraseChoice=0;
  if (!selectWeightedIdentityFromView(profile.feels, GenerationDomain::FeelProfileSelection, rhythm.archetypeId, baseSalt, generation, feel) ||
      !selectWeightedIdentityFromView(compatibleBass, GenerationDomain::BassRhythmSelection, rhythm.archetypeId, baseSalt, generation, bass) ||'''

replacements = [(old_dub, new_dub), (old_helper, new_helper), (old_selection, new_selection)]
changed = False
for old, new in replacements:
    if new in text:
        continue
    if text.count(old) != 1:
        raise SystemExit(f"expected exactly one match for replacement, got {text.count(old)}")
    text = text.replace(old, new, 1)
    changed = True

if changed:
    path.write_text(text, encoding="utf-8")
    print("G4-I8 production binding applied")
else:
    print("G4-I8 production binding already applied")
