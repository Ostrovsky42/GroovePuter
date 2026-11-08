#include <cassert>
#include <iostream>
// Exercise the actual internal scorer without adding a diagnostic firmware API.
#include "src/generation/harmony/harmonic_route.cpp"
using namespace GroovePuterHarmony;
int main() {
  RouteRequest req{};
  req.candidateRequest.style = HarmonicStyleId::LoFi;
  req.candidateRequest.genome.functionalBias = 128;
  const auto* major = qualityDefinition(kQualityMajor);
  const auto* dominant = qualityDefinition(kQualityDominant7);
  req.candidateRequest.current = {kQualityDominant7,
      transposePitchSet(dominant->intervalMask, 7), 7, kNoBassPc};
  req.candidateRequest.home = {kQualityMajor, major->intervalMask, 0, kNoBassPc};
  TonalContext tonal{1, 1, 0x0ab5u, 0x0ab5u, 0, 0};
  req.candidateRequest.currentContext = req.candidateRequest.homeContext = tonal;
  Candidate candidate{};
  candidate.state = req.candidateRequest.home;
  PcVoiceLeadingScratch pcvl{};
  assert(analyzeRelation(req.candidateRequest.current, candidate.state, tonal,
                         tonal, pcvl, candidate.features) == HarmonyStatus::Ready);
  assert(candidate.features.tags & relationTag(RelationTag::DominantToTarget));
  RecentHarmonyMemory recent{};
  candidate.features.tags = 0;
  const auto baseline = scoreCandidate(req, req.candidateRequest, recent, candidate, tonal, 0);
  const auto& w = policyFor(req.candidateRequest.style)->featureWeights;
  candidate.features.tags = relationTag(RelationTag::DiatonicFunctional);
  assert(scoreCandidate(req, req.candidateRequest, recent, candidate, tonal, 0)
         == baseline + w.diatonicTag);
  candidate.features.tags = relationTag(RelationTag::DominantToTarget);
  assert(scoreCandidate(req, req.candidateRequest, recent, candidate, tonal, 0)
         == baseline + w.functionalTag + 4);
  candidate.features.tags = relationTag(RelationTag::SecondaryDominant);
  assert(scoreCandidate(req, req.candidateRequest, recent, candidate, tonal, 0)
         == baseline + w.functionalTag + 4);
  candidate.features.tags |= relationTag(RelationTag::DominantToTarget);
  assert(scoreCandidate(req, req.candidateRequest, recent, candidate, tonal, 0)
         == baseline + w.functionalTag + 4); // no duplicate functional bonus
  std::cout << "HGS diatonic/functional scoring: PASS\n";
}
