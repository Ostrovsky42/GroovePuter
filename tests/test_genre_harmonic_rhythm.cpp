#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/composition/genre_harmonic_rhythm.h"

using namespace GroovePuterRhythm;

namespace {

StepMask mask(std::initializer_list<uint8_t> steps) {
  StepMask result = 0;
  for (uint8_t step : steps)
    result = static_cast<StepMask>(result | stepBit(step));
  return result;
}

ChordRhythmPlan plan(ChordRhythmId id,
                     StepMask onsets,
                     StepMask continuations = 0,
                     StepMask releases = 0) {
  ChordRhythmPlan value{};
  value.id = id;
  value.onsets = onsets;
  value.continuations = continuations;
  value.releasePoints = releases;
  return value;
}

GenreHarmonicRhythmRequest requestFor(ChordRhythmPlan chord) {
  GenreHarmonicRhythmRequest request{};
  request.family = RhythmFamily::FourFloor;
  request.chord = chord;
  request.progression = ProgressionId::PopCycle;
  request.phraseLaw = PhraseEvolutionLawId::Loop;
  request.phraseBars = 4;
  request.barOrdinal = 0;
  return request;
}

void assertTopologyPreserved(const ChordRhythmPlan& source,
                             const GenreHarmonicRhythmPlan& result) {
  assert(result.currentBar.audibleOnsets == source.onsets);
  assert(result.currentBar.continuations == source.continuations);
  assert(result.currentBar.releasePoints == source.releasePoints);
  assert((result.currentBar.sourceAdvanceOnsets &
          result.currentBar.sameChordRetriggers) == 0);
  assert(static_cast<StepMask>(result.currentBar.sourceAdvanceOnsets |
                               result.currentBar.sameChordRetriggers) ==
         source.onsets);
}

void testOffbeatStabBecomesAabbNotFourChordChanges() {
  const ChordRhythmPlan source = plan(
      ChordRhythmId::OffbeatStab, mask({2, 6, 10, 14}));
  GenreHarmonicRhythmRequest request = requestFor(source);
  const GenreHarmonicRhythmPlan result = realizeGenreHarmonicRhythm(request);

  assert(result.status == GenreHarmonicRhythmStatus::Ok);
  assert(result.timelineStatus == ChordRhythmTimelineStatus::Ok);
  assert(result.retriggerStatus == ChordRhythmRetriggerStatus::Ok);
  assert(result.boundedPhraseBars == 4);
  assert(result.timeline.onsetCount == 16);
  assert(result.currentBar.sourceAdvanceOnsets == mask({2, 10}));
  assert(result.currentBar.sameChordRetriggers == mask({6, 14}));
  assert(result.currentBar.sourceAdvanceCount == 2);
  assert(result.currentBar.audibleOnsetCount == 4);
  assertTopologyPreserved(source, result);
}

void testRepeatReplyRelaxesReplyBarWithoutChangingRhythm() {
  const ChordRhythmPlan source = plan(
      ChordRhythmId::OffbeatStab, mask({2, 6, 10, 14}));
  GenreHarmonicRhythmRequest request = requestFor(source);
  request.phraseLaw = PhraseEvolutionLawId::RepeatReply;

  request.barOrdinal = 0;
  const GenreHarmonicRhythmPlan statement =
      realizeGenreHarmonicRhythm(request);
  request.barOrdinal = 1;
  const GenreHarmonicRhythmPlan reply = realizeGenreHarmonicRhythm(request);

  assert(statement.status == GenreHarmonicRhythmStatus::Ok);
  assert(reply.status == GenreHarmonicRhythmStatus::Ok);
  assert(statement.currentBar.sourceAdvanceCount == 2);
  assert(reply.currentBar.sourceAdvanceCount == 1);
  assert(reply.currentBar.sourceAdvanceOnsets == stepBit(2));
  assert(reply.currentBar.sameChordRetriggers == mask({6, 10, 14}));
  assertTopologyPreserved(source, statement);
  assertTopologyPreserved(source, reply);
}

void testStaticProgressionNeverBurnsSourceOnRetriggers() {
  const ChordRhythmPlan source = plan(
      ChordRhythmId::SyncopatedComp, mask({1, 4, 7, 10, 13}));
  GenreHarmonicRhythmRequest request = requestFor(source);
  request.family = RhythmFamily::Funk16;
  request.progression = ProgressionId::StaticModal;
  const GenreHarmonicRhythmPlan result = realizeGenreHarmonicRhythm(request);

  assert(result.status == GenreHarmonicRhythmStatus::Ok);
  assert(result.currentBar.sourceAdvanceCount == 1);
  assert(result.currentBar.sourceAdvanceOnsets == stepBit(1));
  assert(result.currentBar.sameChordRetriggers == mask({4, 7, 10, 13}));
  assertTopologyPreserved(source, result);
}

void testHeldPadPreservesExplicitGateAndRelease() {
  const ChordRhythmPlan source = plan(
      ChordRhythmId::HeldPad,
      stepBit(0),
      mask({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}),
      stepBit(12));
  GenreHarmonicRhythmRequest request = requestFor(source);
  request.family = RhythmFamily::SparsePulse;
  request.progression = ProgressionId::ParallelShift;
  const GenreHarmonicRhythmPlan result = realizeGenreHarmonicRhythm(request);

  assert(result.status == GenreHarmonicRhythmStatus::Ok);
  assert(result.currentBar.sourceAdvanceOnsets == stepBit(0));
  assert(result.currentBar.sameChordRetriggers == 0);
  assertTopologyPreserved(source, result);
}

void testWholeBarHoldDoesNotInventCrossBarContinuation() {
  const ChordRhythmPlan source = plan(
      ChordRhythmId::WholeBarHold,
      stepBit(0),
      mask({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
  GenreHarmonicRhythmRequest request = requestFor(source);
  request.family = RhythmFamily::DubPulse;
  request.progression = ProgressionId::PedalDrone;
  request.phraseLaw = PhraseEvolutionLawId::SparseDrift;
  const GenreHarmonicRhythmPlan result = realizeGenreHarmonicRhythm(request);

  assert(result.status == GenreHarmonicRhythmStatus::Ok);
  assert(result.timeline.status == ChordRhythmTimelineStatus::Ok);
  assert(result.currentBar.sourceAdvanceOnsets == stepBit(0));
  assert(result.currentBar.sameChordRetriggers == 0);
  assertTopologyPreserved(source, result);
}

void testEightBarCompositionMetadataIsBoundedToP2FourBars() {
  const ChordRhythmPlan source = plan(
      ChordRhythmId::BackbeatStab, mask({4, 12}));
  GenreHarmonicRhythmRequest request = requestFor(source);
  request.family = RhythmFamily::HipHopBackbeat;
  request.phraseBars = 8;
  request.phraseLaw = PhraseEvolutionLawId::SparseDrift;
  request.barOrdinal = 7;
  const GenreHarmonicRhythmPlan result = realizeGenreHarmonicRhythm(request);

  assert(result.status == GenreHarmonicRhythmStatus::Ok);
  assert(result.requestedPhraseBars == 8);
  assert(result.boundedPhraseBars == 4);
  assert(result.phraseBarOrdinal == 3);
  assert(result.timeline.barCount == 4);
  assert(result.timeline.stepCount == 64);
  assertTopologyPreserved(source, result);
}

void testEmptyChordBarStaysEmpty() {
  const ChordRhythmPlan source = plan(ChordRhythmId::DubChordSpace, 0);
  GenreHarmonicRhythmRequest request = requestFor(source);
  request.family = RhythmFamily::DubPulse;
  request.progression = ProgressionId::PedalDrone;
  const GenreHarmonicRhythmPlan result = realizeGenreHarmonicRhythm(request);

  assert(result.status == GenreHarmonicRhythmStatus::ValidButEmpty);
  assert(result.timelineStatus == ChordRhythmTimelineStatus::ValidButEmpty);
  assert(result.retriggerStatus == ChordRhythmRetriggerStatus::ValidButEmpty);
  assertTopologyPreserved(source, result);
}

void testBrokenAndFunkKeepTwoChangesButSparseFamiliesKeepOne() {
  const ChordRhythmPlan source = plan(
      ChordRhythmId::SyncopatedComp, mask({1, 4, 7, 10, 13}));

  GenreHarmonicRhythmRequest broken = requestFor(source);
  broken.family = RhythmFamily::UkTwoStep;
  broken.progression = ProgressionId::BorrowedLift;
  const GenreHarmonicRhythmPlan brokenResult =
      realizeGenreHarmonicRhythm(broken);
  assert(brokenResult.status == GenreHarmonicRhythmStatus::Ok);
  assert(brokenResult.currentBar.sourceAdvanceCount == 2);
  assert(brokenResult.currentBar.sourceAdvanceOnsets == mask({1, 7}));

  GenreHarmonicRhythmRequest sparse = requestFor(source);
  sparse.family = RhythmFamily::HipHopBackbeat;
  sparse.progression = ProgressionId::BorrowedLift;
  const GenreHarmonicRhythmPlan sparseResult =
      realizeGenreHarmonicRhythm(sparse);
  assert(sparseResult.status == GenreHarmonicRhythmStatus::Ok);
  assert(sparseResult.currentBar.sourceAdvanceCount == 1);
  assert(sparseResult.currentBar.sourceAdvanceOnsets == stepBit(1));

  assertTopologyPreserved(source, brokenResult);
  assertTopologyPreserved(source, sparseResult);
}

void testDeterministicAcrossRepeatedCalls() {
  const ChordRhythmPlan source = plan(
      ChordRhythmId::OffbeatStab, mask({2, 6, 10, 14}));
  GenreHarmonicRhythmRequest request = requestFor(source);
  request.phraseLaw = PhraseEvolutionLawId::RepeatReply;
  request.barOrdinal = 3;
  const GenreHarmonicRhythmPlan a = realizeGenreHarmonicRhythm(request);
  const GenreHarmonicRhythmPlan b = realizeGenreHarmonicRhythm(request);

  assert(a.status == b.status);
  assert(a.boundedPhraseBars == b.boundedPhraseBars);
  assert(a.phraseBarOrdinal == b.phraseBarOrdinal);
  assert(a.timeline.onsets == b.timeline.onsets);
  assert(a.timeline.continuations == b.timeline.continuations);
  assert(a.timeline.releasePoints == b.timeline.releasePoints);
  assert(a.currentBar.sourceAdvanceOnsets == b.currentBar.sourceAdvanceOnsets);
  assert(a.currentBar.sameChordRetriggers == b.currentBar.sameChordRetriggers);
  assert(a.currentBar.continuations == b.currentBar.continuations);
  assert(a.currentBar.releasePoints == b.currentBar.releasePoints);
}

}  // namespace

int main() {
  testOffbeatStabBecomesAabbNotFourChordChanges();
  testRepeatReplyRelaxesReplyBarWithoutChangingRhythm();
  testStaticProgressionNeverBurnsSourceOnRetriggers();
  testHeldPadPreservesExplicitGateAndRelease();
  testWholeBarHoldDoesNotInventCrossBarContinuation();
  testEightBarCompositionMetadataIsBoundedToP2FourBars();
  testEmptyChordBarStaysEmpty();
  testBrokenAndFunkKeepTwoChangesButSparseFamiliesKeepOne();
  testDeterministicAcrossRepeatedCalls();
  std::cout << "Genre harmonic rhythm production semantics: OK\n";
  return 0;
}
