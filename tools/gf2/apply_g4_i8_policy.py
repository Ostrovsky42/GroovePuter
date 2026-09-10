#!/usr/bin/env python3
from pathlib import Path

path = Path("src/generation/composition/generation_profile.cpp")
text = path.read_text(encoding="utf-8")

replacements = [
    (
'''  HarmonicChangeRateId harmonicChangeRate;
  CompositionSecondaryRole secondaryRole;
};''',
'''  HarmonicChangeRateId harmonicChangeRate;
  CompositionSecondaryRole secondaryRole;
  BassSelectionPolicy bassSelectionPolicy;
};'''),
    (
'''    GenerationCorridor corridor,
    CompositionSecondaryRole secondaryRole,
    HarmonicChangeRateId harmonicChangeRate = HarmonicChangeRateId::Every2Beats) {
  return {static_cast<uint8_t>(mode), recipe, feels, bass, chord, progression,
          melodic, motif, phraseLaw, corridor, harmonicChangeRate, secondaryRole};
}''',
'''    GenerationCorridor corridor,
    CompositionSecondaryRole secondaryRole,
    HarmonicChangeRateId harmonicChangeRate = HarmonicChangeRateId::Every2Beats,
    BassSelectionPolicy bassSelectionPolicy = BassSelectionPolicy::Independent) {
  return {static_cast<uint8_t>(mode), recipe, feels, bass, chord, progression,
          melodic, motif, phraseLaw, corridor, harmonicChangeRate, secondaryRole,
          bassSelectionPolicy};
}'''),
    (
'''    profile(GenerativeMode::DrumAndBass, 0, view(kFeelStraightDrive), view(kBassBreakbeat), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseCompact), {160,180,174,16,7,15}, CompositionSecondaryRole::Melodic),''',
'''    profile(GenerativeMode::DrumAndBass, 0, view(kFeelStraightDrive), view(kBassBreakbeat), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseCompact), {160,180,174,16,7,15}, CompositionSecondaryRole::Melodic, HarmonicChangeRateId::Every2Beats, BassSelectionPolicy::FamilyNative),'''),
    (
'''    profile(GenerativeMode::Reggae, 5, view(kFeelDubPocket), view(kBassDub), view(kChordDub), view(kProgressionDub), view(kMelodicDub), view(kMotifSparse), view(kPhraseSlow), {112,128,120,16,2,8}, CompositionSecondaryRole::Chord),''',
'''    profile(GenerativeMode::Reggae, 5, view(kFeelDubPocket), view(kBassDub), view(kChordDub), view(kProgressionDub), view(kMelodicDub), view(kMotifSparse), view(kPhraseSlow), {112,128,120,16,2,8}, CompositionSecondaryRole::Chord, HarmonicChangeRateId::Every2Beats, BassSelectionPolicy::FamilyNative),'''),
    (
'''  result.harmonicChangeRate = definition->harmonicChangeRate;
  result.secondaryRole = definition->secondaryRole;
  return result;''',
'''  result.harmonicChangeRate = definition->harmonicChangeRate;
  result.secondaryRole = definition->secondaryRole;
  result.bassSelectionPolicy = definition->bassSelectionPolicy;
  return result;'''),
    (
'''      !isValidHarmonicChangeRate(profile.harmonicChangeRate) ||
      static_cast<uint8_t>(profile.secondaryRole) >= static_cast<uint8_t>(CompositionSecondaryRole::Count)) {''',
'''      !isValidHarmonicChangeRate(profile.harmonicChangeRate) ||
      static_cast<uint8_t>(profile.secondaryRole) >= static_cast<uint8_t>(CompositionSecondaryRole::Count) ||
      static_cast<uint8_t>(profile.bassSelectionPolicy) >= static_cast<uint8_t>(BassSelectionPolicy::Count)) {'''),
    (
'''  const ReferenceVocabulary::Definition* rhythmDefinition =
      ReferenceVocabulary::definitionForId(rhythm.archetypeId);
  if (rhythmDefinition == nullptr) {
    result.status = GenerationCompositionStatus::InvalidProfile;
    return result;
  }
  WeightedIdentityCandidate compatibleBassStorage[kMaxWeightedCandidates]{};
  const WeightedIdentityView compatibleBass = bassCandidatesForFamily(
      profile.bassRhythms, rhythmDefinition->family,
      compatibleBassStorage, kMaxWeightedCandidates);

  const uint32_t baseSalt = profileSalt(profile);''',
'''  WeightedIdentityView bassCandidates = profile.bassRhythms;
  WeightedIdentityCandidate compatibleBassStorage[kMaxWeightedCandidates]{};
  if (profile.bassSelectionPolicy == BassSelectionPolicy::FamilyNative) {
    const ReferenceVocabulary::Definition* rhythmDefinition =
        ReferenceVocabulary::definitionForId(rhythm.archetypeId);
    if (rhythmDefinition == nullptr) {
      result.status = GenerationCompositionStatus::InvalidProfile;
      return result;
    }
    bassCandidates = bassCandidatesForFamily(
        profile.bassRhythms, rhythmDefinition->family,
        compatibleBassStorage, kMaxWeightedCandidates);
    if (bassCandidates.count == 0) {
      result.status = GenerationCompositionStatus::InvalidProfile;
      return result;
    }
  }

  const uint32_t baseSalt = profileSalt(profile);'''),
    (
'''      !selectWeightedIdentityFromView(compatibleBass, GenerationDomain::BassRhythmSelection, rhythm.archetypeId, baseSalt, generation, bass) ||''',
'''      !selectWeightedIdentityFromView(bassCandidates, GenerationDomain::BassRhythmSelection, rhythm.archetypeId, baseSalt, generation, bass) ||'''),
]

changed = False
for old, new in replacements:
    if new in text:
        continue
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one match, got {count}: {old[:80]!r}")
    text = text.replace(old, new, 1)
    changed = True

if changed:
    path.write_text(text, encoding="utf-8")
    print("G4-I8 opt-in bass policy applied")
else:
    print("G4-I8 opt-in bass policy already applied")
