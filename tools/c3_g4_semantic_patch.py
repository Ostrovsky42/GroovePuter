#!/usr/bin/env python3
from pathlib import Path


def once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"C3 anchor {label}: expected 1, got {count}")
    return text.replace(old, new, 1)


def patch_generation_profile() -> None:
    p = Path("src/generation/composition/generation_profile.cpp")
    s = p.read_text()
    if "constexpr WeightedIdentityCandidate kBassBreakbeat[]" not in s:
        old = """constexpr WeightedIdentityCandidate kBassMachine[] = {
    weighted(BassRhythmId::KickAnswer, 90),
    weighted(BassRhythmId::GapFill, 90),
    weighted(BassRhythmId::OffbeatPush, 70),
    weighted(BassRhythmId::SyncopatedHook, 120),
};
constexpr WeightedIdentityCandidate kBassSlow[] = {
"""
        new = """constexpr WeightedIdentityCandidate kBassMachine[] = {
    weighted(BassRhythmId::KickAnswer, 90),
    weighted(BassRhythmId::GapFill, 90),
    weighted(BassRhythmId::OffbeatPush, 70),
    weighted(BassRhythmId::SyncopatedHook, 120),
};
// G4-I1: DnB's rhythm candidates are all Breakbeat. Keep its editorial
// weights inside the same semantic vocabulary already owned by bass_rhythm.cpp
// rather than sending FourFloor-only identities through an explicit request.
constexpr WeightedIdentityCandidate kBassBreakbeat[] = {
    weighted(BassRhythmId::KickAnswer, 100),
    weighted(BassRhythmId::GapFill, 100),
    weighted(BassRhythmId::HalfTimePocket, 100),
    weighted(BassRhythmId::SyncopatedHook, 100),
};
constexpr WeightedIdentityCandidate kBassSlow[] = {
"""
        s = once(s, old, new, "DnB bass vocabulary")
    s = once(
        s,
        "profile(GenerativeMode::DrumAndBass, 0, view(kFeelStraightDrive), view(kBassDrive),",
        "profile(GenerativeMode::DrumAndBass, 0, view(kFeelStraightDrive), view(kBassBreakbeat),",
        "DnB profile owner",
    ) if "profile(GenerativeMode::DrumAndBass, 0, view(kFeelStraightDrive), view(kBassDrive)," in s else s
    p.write_text(s)


def patch_rhythm_selection() -> None:
    p = Path("src/generation/composition/rhythm_selection.cpp")
    s = p.read_text()
    house_old = """// Stage 14 uses only repository-approved production identities. Pending
// HARD_02/HARD_04/HARD_05 never appear in these compatibility edges.
constexpr RhythmCompatibilityCandidate kHouseBase[] = {
    candidate(Archetype::StraightDrive, 90), candidate(Archetype::OffbeatOpenHat, 90),
    candidate(Archetype::StackedQuarters, 125), candidate(Archetype::FunkHouseBridge, 130),
    candidate(Archetype::ShuffledFourFour, 70),
};
"""
    house_new = """// Stage 14 uses only repository-approved production identities. Pending
// HARD_02/HARD_04/HARD_05 never appear in these compatibility edges.
// G4-I5: House BASE owns quarter-pulse ideas. FunkHouseBridge remains in the
// shared vocabulary and its other owners, but its kick grammar does not carry
// the House quarter-pulse witness and therefore is not admitted here.
constexpr RhythmCompatibilityCandidate kHouseBase[] = {
    candidate(Archetype::StraightDrive, 90), candidate(Archetype::OffbeatOpenHat, 90),
    candidate(Archetype::StackedQuarters, 125),
    candidate(Archetype::ShuffledFourFour, 70),
};
"""
    if "candidate(Archetype::FunkHouseBridge, 130)" in s:
        s = once(s, house_old, house_new, "House structural owner")

    dub_old = """constexpr RhythmCompatibilityCandidate kDubTechno[] = {
    candidate(Archetype::OneDropSpace, 110), candidate(Archetype::Steppers, 100),
    candidate(Archetype::SparseSkank, 110), candidate(Archetype::ChordResponse, 120),
};
"""
    dub_new = """// G4-I4: Dub Techno recipe 5 owns only ideas with an explicit structural
// techno-skeleton witness. The retained Steppers candidate also preserves the
// dub chord-response vocabulary. OneDropSpace, SparseSkank and ChordResponse
// remain available to Reggae/Dub owners instead of entering through timbre/FX.
constexpr RhythmCompatibilityCandidate kDubTechno[] = {
    candidate(Archetype::StraightDrive, 55),
    candidate(Archetype::BrokenTechno, 65),
    candidate(Archetype::Steppers, 100),
};
"""
    if "candidate(Archetype::OneDropSpace, 110), candidate(Archetype::Steppers, 100)" in s:
        s = once(s, dub_old, dub_new, "Dub Techno structural owner")
    p.write_text(s)


def patch_phrase_execution() -> None:
    p = Path("src/generation/migration/phrase_execution.cpp")
    s = p.read_text()
    old = """  // GF2-I3: realize the declared bar-function programme once for the phrase.
  destination.phraseTrajectory = admittedPhraseTrajectory(
      destination.selection.composition.rhythmArchetypeId,
      destination.selection.composition.phraseLaw, materialization.level,
      destination.length.effectivePhraseBars);
  if (destination.phraseTrajectory != kNoTrajectoryId) {
"""
    new = """  // G4-I3: Phrase is the first causal owner of multi-bar development. A
  // selected non-Loop label is truthful only when this exact archetype, level
  // and length admit the trajectory it names. Otherwise retain the selected
  // Phrase length but normalize the semantic law to the neutral Loop instead
  // of advertising development that cannot causally reach production.
  const PhraseEvolutionLawId selectedPhraseLaw =
      destination.selection.composition.phraseLaw;
  destination.phraseTrajectory = admittedPhraseTrajectory(
      destination.selection.composition.rhythmArchetypeId,
      selectedPhraseLaw, materialization.level,
      destination.length.effectivePhraseBars);
  if (selectedPhraseLaw != PhraseEvolutionLawId::Loop &&
      destination.phraseTrajectory == kNoTrajectoryId) {
    destination.selection.composition.phraseLaw = PhraseEvolutionLawId::Loop;
  }

  if (destination.phraseTrajectory != kNoTrajectoryId) {
"""
    if "const PhraseEvolutionLawId selectedPhraseLaw" not in s:
        s = once(s, old, new, "phrase-law admission")
    old2 = """    if (evolved.status != BarEvolutionStatus::Ok || evolved.plan.barCount == 0) {
      // A law that cannot be realized must not fail the phrase; the established
      // per-bar realization stays in force.
      destination.phraseTrajectory = kNoTrajectoryId;
    } else {
"""
    new2 = """    if (evolved.status != BarEvolutionStatus::Ok || evolved.plan.barCount == 0) {
      // A programme that still cannot be realized must not leave a false
      // non-Loop semantic label behind. Preserve Phrase and fall back to Loop.
      destination.phraseTrajectory = kNoTrajectoryId;
      destination.selection.composition.phraseLaw = PhraseEvolutionLawId::Loop;
    } else {
"""
    if "non-Loop semantic label" not in s:
        s = once(s, old2, new2, "phrase-law realization failure")
    p.write_text(s)


def main() -> None:
    patch_generation_profile()
    patch_rhythm_selection()
    patch_phrase_execution()


if __name__ == "__main__":
    main()
