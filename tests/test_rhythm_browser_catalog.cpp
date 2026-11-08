#include "../src/generation/rhythm_browser/rhythm_browser_catalog.h"
#include "../src/dsp/atlas_runtime.h"
#include <cassert>
#include <cstdio>

using namespace RhythmBrowser;

static bool sameSteps(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int v = 0; v < DrumPatternSet::kVoices; ++v)
    for (int s = 0; s < DrumPattern::kSteps; ++s) {
      const auto& x = a.voices[v].steps[s];
      const auto& y = b.voices[v].steps[s];
      if (x.hit != y.hit || x.accent != y.accent || x.velocity != y.velocity ||
          x.timing != y.timing || x.fx != y.fx || x.fxParam != y.fxParam ||
          x.probability != y.probability) return false;
    }
  return true;
}

int main() {
  assert(count() == 35);
  Workspace workspace{};
  Candidate candidate{};
  for (uint8_t i = 0; i < count(); ++i) {
    Descriptor entry{};
    assert(descriptorAt(i, entry));
    assert(entry.name && entry.name[0] && entry.stableId);
    assert(entry.fourFour && entry.renderability == Renderability::RequiresMaterialization);
    assert(matchesGroup(entry, Group::All));
    Descriptor same{};
    assert(lookup(entry.source, entry.stableId, same));
    assert(same.variantCount == entry.variantCount);
    for (uint8_t j = 0; j < i; ++j) {
      Descriptor previous{};
      assert(descriptorAt(j, previous));
      assert(previous.source != entry.source || previous.stableId != entry.stableId);
    }
    if (entry.source == Source::Atlas) assert(entry.variantCount == 3);
    else if (entry.source == Source::Vocabulary) assert(entry.variantCount == 1);
    else assert(entry.source == Source::Preset && entry.variantCount >= 1);
    assert(entry.provenance == (entry.source == Source::Atlas
                                    ? Provenance::CompiledAtlasRightsReview
                                : entry.source == Source::Preset
                                    ? Provenance::HandAuthoredPreset
                                    : Provenance::VocabularyGrammar));
    for (uint8_t variant = 0; variant < entry.variantCount; ++variant) {
      Identity id{entry.source, entry.stableId, variant, 12345u, 17u};
      assert(materialize(id, workspace, candidate) == Status::Ok);
      assert(candidate.identity.contextVersion == 17);
      const Candidate first = candidate;
      assert(materialize(id, workspace, candidate) == Status::Ok);
      assert(sameSteps(first.drums, candidate.drums));
      unsigned hits = 0;
      for (const auto& voice : candidate.drums.voices)
        for (const auto& step : voice.steps) hits += step.hit;
      assert(hits > 0);
      if (entry.source == Source::Atlas) {
        DrumPatternSet reference{};
        assert(AtlasRuntime::materializeDrums(static_cast<uint8_t>(entry.stableId), variant, reference));
        assert(sameSteps(candidate.drums, reference));
      }
    }
  }
  Descriptor house{}, ukg{};
  assert(lookup(Source::Vocabulary, 401, house));
  assert(matchesGroup(house, Group::House));
  assert(matchesGroup(house, Group::Techno));
  assert(lookup(Source::Vocabulary, 417, ukg));
  assert(matchesGroup(ukg, Group::UkGarage));
  assert(!matchesGroup(ukg, Group::House));
  assert(!matchesGroup(ukg, Group::Count));
  const Identity firstId{Source::Vocabulary, 401, 0, 42, 17};
  const Identity otherId{Source::Vocabulary, 417, 0, 42, 17};
  assert(materialize(firstId, workspace, candidate) == Status::Ok);
  const Candidate firstVocabulary = candidate;
  assert(materialize(otherId, workspace, candidate) == Status::Ok);
  assert(materialize(firstId, workspace, candidate) == Status::Ok);
  assert(sameSteps(firstVocabulary.drums, candidate.drums));
  for (uint8_t group = 1; group < static_cast<uint8_t>(Group::Count); ++group) {
    unsigned found = 0;
    for (uint8_t i = 0; i < count(); ++i) {
      Descriptor entry{};
      assert(descriptorAt(i, entry));
      found += matchesGroup(entry, static_cast<Group>(group));
    }
    assert(found > 0);
  }
  Identity exact{Source::Atlas, 6, 0, 100, 7};
  assert(materialize(exact, workspace, candidate) == Status::Ok);
  const Candidate firstExact = candidate;
  exact.previewSeed = 200;
  assert(materialize(exact, workspace, candidate) == Status::Ok);
  assert(sameSteps(firstExact.drums, candidate.drums));
  assert(candidate.identity.previewSeed == 200);
  Descriptor unchanged{};
  unchanged.stableId = 999;
  assert(!descriptorAt(count(), unchanged) && unchanged.stableId == 999);
  assert(!lookup(Source::Atlas, 262, unchanged) && unchanged.stableId == 999);
  candidate.identity.stableId = 777;
  candidate.drums.voices[0].steps[0].hit = true;
  candidate.drums.groove.swing = 0.25f;
  const Candidate sentinel = candidate;
  assert(materialize({Source::Atlas, 6, 3, 1, 0}, workspace, candidate) == Status::InvalidVariant);
  assert(materialize({Source::Vocabulary, 401, 1, 1, 0}, workspace, candidate) == Status::InvalidVariant);
  assert(materialize({Source::Atlas, 262, 0, 1, 0}, workspace, candidate) == Status::UnknownId);
  assert(materialize({static_cast<Source>(99), 1, 0, 1, 0}, workspace, candidate) == Status::InvalidSource);
  assert(candidate.identity.stableId == 777 && sameSteps(candidate.drums, sentinel.drums));
  assert(candidate.drums.groove.swing == 0.25f);
  std::puts("Rhythm Browser catalog and candidate: PASS");
}
