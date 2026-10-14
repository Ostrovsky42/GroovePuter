#include <cassert>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "src/generation/harmony/generated/corpus_prior_data.h"

#include "src/generation/harmony/harmonic_candidate.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"

using namespace GroovePuterHarmony;

namespace {
HarmonicState state(uint8_t root, QualityId qualityId) {
  const ChordQualityDefinition* quality = qualityDefinition(qualityId);
  assert(quality != nullptr);
  return {qualityId, transposePitchSet(quality->intervalMask, root), root,
          kNoBassPc};
}

TonalContext context(uint8_t tonic, ScaleId scaleId) {
  const ScaleDefinition* scale = scaleDefinition(scaleId);
  assert(scale != nullptr);
  return {scaleId, scaleId, transposePitchSet(scale->intervalMask, tonic),
          transposePitchSet(scale->intervalMask, tonic), tonic, tonic};
}

CandidateRequest request() {
  CandidateRequest result{};
  result.current = state(0, kQualityMajor);
  result.currentContext = context(0, 1);
  result.home = result.current;
  result.homeContext = result.currentContext;
  result.style = HarmonicStyleId::Ambient;
  result.section = SectionIntent::Develop;
  result.runtimeQualityMask = 0x03ffu;
  result.runtimeScaleMask = 0x03ffu;
  return result;
}

bool contains(const CandidateList& list, uint8_t root, QualityId quality,
              uint8_t contextIndex) {
  for (uint8_t i = 0; i < list.count; ++i) {
    const Candidate& item = list.items[i];
    if (item.state.rootPc == root && item.state.qualityId == quality &&
        item.contextIndex == contextIndex) return true;
  }
  return false;
}
}  // namespace

int main() {
  static_assert(sizeof(Candidate) <= 24);
  CandidateScratch scratch{};
  CandidateList candidates{};
  TonalCandidateList contexts{};

  CandidateRequest req = request();
  assert(collectCandidates(req, scratch, candidates, contexts) ==
         HarmonyStatus::Ready);
  assert(candidates.count > 0 && candidates.count <= kMaxHarmonicCandidates);
  assert(candidates.count == kMaxHarmonicCandidates);
  assert(contexts.count > 0 && contexts.count <= kMaxTonalCandidates);
  assert(contains(candidates, 0, kQualityMajor, 0));  // Current/HOLD reserved.
  bool foundLegalUnseenZeroPrior = false;
  for (uint8_t i = 0; i < candidates.count; ++i) {
    if (candidates.items[i].corpusPrior == 0) foundLegalUnseenZeroPrior = true;
    assert(candidates.items[i].contextIndex < contexts.count);
    for (uint8_t j = i + 1; j < candidates.count; ++j) {
      const Candidate& a = candidates.items[i];
      const Candidate& b = candidates.items[j];
      assert(a.state.rootPc != b.state.rootPc ||
             a.state.qualityId != b.state.qualityId ||
             a.state.bassPc != b.state.bassPc ||
             a.contextIndex != b.contextIndex);
    }
  }
  assert(foundLegalUnseenZeroPrior);

  // A one-quality capability and STATIC structural contract produce only the
  // current HOLD; the generator does not silently widen to imported qualities.
  req.runtimeQualityMask = 1u << kQualityMajor;
  req.runtimeScaleMask = 1u << 1;
  req.structuralMotionAllowed = 0;
  req.section = SectionIntent::Keep;
  assert(collectCandidates(req, scratch, candidates, contexts) ==
         HarmonyStatus::Ready);
  assert(candidates.count == 1);
  assert(candidates.items[0].state.qualityId == kQualityMajor);

  // RETURN must retain the exact home state even when optional candidates fill
  // the fixed list. Current/home identity is reserved before optional fill.
  req.section = SectionIntent::Return;
  req.runtimeQualityMask = 0x03ffu;
  req.runtimeScaleMask = 0x03ffu;
  req.structuralMotionAllowed = 1;
  req.home = state(7, kQualityMajor);
  req.homeContext = req.currentContext;
  assert(collectCandidates(req, scratch, candidates, contexts) ==
         HarmonyStatus::Ready);
  assert(contains(candidates, req.home.rootPc, req.home.qualityId, 0));
  assert(candidates.count == kMaxHarmonicCandidates);

  // Empty capability is a valid empty result, not an implicit fallback to ID 0.
  req.runtimeQualityMask = 0;
  req.runtimeScaleMask = 0;
  assert(collectCandidates(req, scratch, candidates, contexts) ==
         HarmonyStatus::Ready);
  assert(candidates.count == 0 && contexts.count == 0);

  // Unknown styles and malformed requested states fail without mutating outputs.
  req = request();
  req.style = static_cast<HarmonicStyleId>(255);
  CandidateList sentinel{};
  sentinel.count = 1;
  sentinel.items[0].state.rootPc = 9;
  candidates = sentinel;
  assert(collectCandidates(req, scratch, candidates, contexts) ==
         HarmonyStatus::Invalid);
  assert(candidates.count == 1 && candidates.items[0].state.rootPc == 9);
  req = request();
  req.genome = {255, 255, 255, 255, 255, 255, 255, 255,
                255, 255, 255, 255, 255, 255};
  req.runtimeQualityMask = 1u << kQualityMajor;
  req.runtimeScaleMask = 1u << 1;
  req.structuralMotionAllowed = 0;
  assert(collectCandidates(req, scratch, candidates, contexts) ==
         HarmonyStatus::Ready);
  assert(candidates.count == 1 && candidates.items[0].state.qualityId == kQualityMajor);

  req = request();
  req.current.qualityId = 31;
  assert(collectCandidates(req, scratch, candidates, contexts) ==
         HarmonyStatus::Invalid);

  // Shared vectors pin the offline/runtime semantic codec, including minor,
  // non-C keys, altered roots, sevenths/ninths and slash bass.
  std::ifstream vectors("tests/fixtures/harmony/canonical_codec_v2.tsv");
  assert(vectors.good());
  std::string line;
  unsigned vectorCount = 0;
  while (std::getline(vectors, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    int tonic, scale, relativeRoot, quality, slash;
    int extensions, degree, signedRoot, group, alterations, bass, spelling;
    assert(row >> tonic >> scale >> relativeRoot >> quality >> slash >>
           extensions >> degree >> signedRoot >> group >> alterations >> bass >> spelling);
    auto chord = state(static_cast<uint8_t>((tonic + relativeRoot) % 12), quality);
    if (slash != 255) chord.bassPc = static_cast<uint8_t>((chord.rootPc + slash) % 12);
    CanonicalFunctionToken token{};
    assert(canonicalFunctionTokenFor(chord, context(tonic, scale), token) == HarmonyStatus::Ready);
    assert(token.extensionFlags == extensions && token.degree == degree &&
           token.rootOffset == signedRoot && token.qualityGroup == group &&
           token.alterationFlags == alterations && token.slashBassClass == bass &&
           token.spellingFlags == spelling);
    ++vectorCount;
  }
  assert(vectorCount == 12);
  CanonicalFunctionToken tonicMajor{};
  assert(canonicalFunctionTokenFor(state(0, kQualityMajor), context(0, 1), tonicMajor)
         == HarmonyStatus::Ready);
  bool exactTonic = false;
  for (uint16_t i = 0; i < GeneratedCorpusPrior::kTokenCount; ++i) {
    const auto& token = GeneratedCorpusPrior::kTokens[i];
    if (token.extensionFlags == tonicMajor.extensionFlags &&
        token.degree == tonicMajor.degree && token.rootOffset == tonicMajor.rootOffset &&
        token.qualityGroup == tonicMajor.qualityGroup &&
        token.alterationFlags == tonicMajor.alterationFlags &&
        token.slashBassClass == tonicMajor.slashBassClass &&
        token.spellingFlags == tonicMajor.spellingFlags) exactTonic = true;
  }
  assert(exactTonic);

  std::cout << "HGS bounded candidates: PASS\n";
  return 0;
}
