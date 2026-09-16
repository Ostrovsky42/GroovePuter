#include <cstdio>
#include <cstring>
#include "src/dsp/musical_development.h"

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "R1.1 FAIL: %s\n", message);
    ++g_failures;
  } else {
    std::printf("R1.1 PASS: %s\n", message);
  }
}

PhraseRuntime::RuntimeSynthEventBuffer makeTestPhrase(uint8_t note0 = 48, uint8_t note1 = 55) {
  PhraseRuntime::RuntimeSynthEventBuffer buf{};
  buf.lengthTicks = PhraseRuntime::kTicksPerBar;
  buf.count = 2;
  buf.events[0].startTick = 0;
  buf.events[0].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  buf.events[0].note = note0;
  buf.events[0].velocity = 100;
  buf.events[0].probability = 100;

  buf.events[1].startTick = 24;
  buf.events[1].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  buf.events[1].note = note1;
  buf.events[1].velocity = 90;
  buf.events[1].probability = 100;
  return buf;
}

// 1. Witness DISPLACE does not fabricate metricAlignment PASS
void testDisplaceMetricAlignmentUnknown() {
  auto phrase = makeTestPhrase();
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Displace;
  req.displaceTicks = 12;
  auto dev = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(dev.evidence.rhythm.metricAlignment == GroovePuterDevelopment::TriState::Unknown,
         "DISPLACE metricAlignment must be Unknown, not fabricated Pass");
}

// 2. Witness THIN does not fabricate metricAlignment PASS
void testThinMetricAlignmentUnknown() {
  auto phrase = makeTestPhrase();
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Thin;
  auto dev = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(dev.evidence.rhythm.metricAlignment == GroovePuterDevelopment::TriState::Unknown,
         "THIN metricAlignment must be Unknown, not fabricated Pass");
}

// 3. Witness HOLD contour preservation is derived from pitches
void testHoldContourPreserved() {
  auto phrase = makeTestPhrase(48, 55); // ascending contour
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Hold;
  auto dev = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(dev.evidence.bass.contourPreserved == GroovePuterDevelopment::TriState::Pass,
         "HOLD contour preservation must be Pass when pitch sequence is preserved");
}

// 4. Witness CONNECT contour preservation is derived from pitches
void testConnectContourPreserved() {
  auto phrase = makeTestPhrase(48, 55);
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Connect;
  auto dev = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(dev.evidence.bass.contourPreserved == GroovePuterDevelopment::TriState::Pass,
         "CONNECT contour preservation must be Pass when pitch sequence is preserved");
}

// 5. Witness MOVE contour detects inversion and does not fabricate Pass
void testMoveContourDetectsInversion() {
  // Source: Note 0 is G4 (67), Note 1 is B5 (83). Ascending contour (67 < 83).
  // With +1 octave shift (12 semitones):
  // Note 0: 67 + 12 = 79 <= 84 (does not wrap).
  // Note 1: 83 + 12 = 95 > 84 -> wraps down by 24 to 71!
  // Candidate: Note 0 is 79, Note 1 is 71. Descending contour (79 > 71)!
  // Direction inverted!
  auto phrase = makeTestPhrase(67, 83);
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Move;
  req.octaveShift = 1;
  auto dev = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(dev.evidence.bass.contourPreserved == GroovePuterDevelopment::TriState::Fail,
         "MOVE with octave wrap inverting direction must report contourPreserved = Fail");
}

// 6. Witness REPEAT does not fabricate Genre PASS or Temporal PASS
void testRepeatClassificationTruth() {
  auto phrase = makeTestPhrase();
  GroovePuterDevelopment::DevelopmentRequest req{};
  auto res = GroovePuterDevelopment::growMaterial(phrase, 2, GroovePuterDevelopment::GrowthMode::Repeat, req);
  expect(res.classification.idea == GroovePuterMaterial::IdeaClassification::Preserved,
         "REPEAT idea must be Preserved");
  expect(res.classification.genre == GroovePuterDevelopment::GenreResult::Unknown,
         "REPEAT genre must be Unknown, not fabricated Pass");
  expect(res.classification.temporalRole == GroovePuterDevelopment::TemporalRoleResult::Unknown,
         "REPEAT temporalRole must be Unknown, not fabricated Pass");
  // Policy allows exact user-requested REPEAT with Preserved to Publish!
  expect(res.disposition == GroovePuterDevelopment::DevelopmentDisposition::Publish,
         "REPEAT policy disposition must be Publish");
  expect(res.success, "REPEAT growth must succeed via policy");
}

// 7. Witness REVOICE does not fabricate rootPreserved without tonal root authority
void testRevoiceRootPreservedUnknown() {
  auto phrase = makeTestPhrase(48, 55);
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Revoice;
  req.octaveShift = 1;
  auto dev = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(dev.evidence.harmony.pitchClassesPreserved,
         "REVOICE must observe pitchClassesPreserved = true");
  expect(dev.evidence.harmony.rootPreserved == GroovePuterDevelopment::TriState::Unknown,
         "REVOICE rootPreserved must be Unknown without authoritative tonal model");
}

// 8. Witness Provenance accurately reports UNKNOWN states
void testProvenanceReportsUnknownTruth() {
  GroovePuterDevelopment::DevelopmentProvenance prov{};
  prov.growthMode = GroovePuterDevelopment::GrowthMode::Repeat;
  prov.classification.idea = GroovePuterMaterial::IdeaClassification::Preserved;
  prov.classification.genre = GroovePuterDevelopment::GenreResult::Unknown;
  prov.classification.temporalRole = GroovePuterDevelopment::TemporalRoleResult::Unknown;
  prov.disposition = GroovePuterDevelopment::DevelopmentDisposition::Publish;

  char buf[256];
  GroovePuterDevelopment::formatProvenance(prov, buf, sizeof(buf));
  expect(std::strstr(buf, "GENRE: UNKNOWN") != nullptr,
         "Provenance must format GENRE: UNKNOWN");
  expect(std::strstr(buf, "TEMPORAL: UNKNOWN") != nullptr,
         "Provenance must format TEMPORAL: UNKNOWN");
  expect(std::strstr(buf, "POLICY: PUBLISH") != nullptr,
         "Provenance must format POLICY: PUBLISH");
}

}  // namespace

int main() {
  testDisplaceMetricAlignmentUnknown();
  testThinMetricAlignmentUnknown();
  testHoldContourPreserved();
  testConnectContourPreserved();
  testMoveContourDetectsInversion();
  testRepeatClassificationTruth();
  testRevoiceRootPreservedUnknown();
  testProvenanceReportsUnknownTruth();

  std::printf("\n=== R1.1 Epistemic Gate: %d failures ===\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
