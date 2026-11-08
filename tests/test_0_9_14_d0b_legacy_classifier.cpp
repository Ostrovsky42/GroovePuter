// D0-B RED witnesses against the 0.9.13 legacy classifier.
//
// These assertions intentionally describe 0.9.14 semantics, not current
// behavior. On the frozen d2c06a52 baseline B8 and B9 must be RED because
// GenreResult is currently used as an omnibus admissibility result.

#include "src/dsp/musical_development.h"

#include <cstdio>

using PhraseRuntime::RuntimeSynthEventBuffer;

namespace {

int failures = 0;

void check(bool condition, const char* id, const char* message) {
  if (condition) {
    std::printf("%s PASS: %s\n", id, message);
  } else {
    std::printf("%s RED: %s\n", id, message);
    ++failures;
  }
}

RuntimeSynthEventBuffer makeLine(uint8_t firstNote,
                                 uint16_t firstTick,
                                 uint8_t secondNote,
                                 uint16_t secondTick) {
  RuntimeSynthEventBuffer b{};
  b.lengthTicks = PhraseRuntime::kTicksPerBar;
  b.count = 2;

  b.events[0].startTick = firstTick;
  b.events[0].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[0].note = firstNote;
  b.events[0].velocity = 100;
  b.events[0].probability = 100;

  b.events[1].startTick = secondTick;
  b.events[1].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[1].note = secondNote;
  b.events[1].velocity = 100;
  b.events[1].probability = 100;

  return b;
}

void b7_tiny_change_can_be_genre_violation() {
  const auto source = makeLine(48, 0, 52, 96);
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Displace;
  req.genreId = static_cast<uint8_t>(GenerativeMode::FunkSoul);
  req.forceDisplaceTheOne = true;
  req.displaceTicks = 12;

  const auto result =
      GroovePuterDevelopment::developCandidate(source, req, &source);

  check(result.classification.genre ==
            GroovePuterDevelopment::GenreResult::Fail,
        "D0-B7",
        "destroying The One is already observable as a genre-bearing rejection");
}

void b8_unavailable_capability_must_not_be_genre_failure() {
  const auto source = makeLine(48, 0, 52, 96);
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Extend;
  req.genreId = static_cast<uint8_t>(GenerativeMode::Acid);

  const auto result =
      GroovePuterDevelopment::developCandidate(source, req, &source);

  check(result.classification.genre !=
            GroovePuterDevelopment::GenreResult::Fail,
        "D0-B8",
        "missing tonal-root authority must be CAPABILITY UNAVAILABLE, not GENRE FAIL");
}

void b9_operation_violation_must_not_be_genre_failure() {
  // Revoice adds an octave. 60 -> 72 while 80 wraps to 68, so the original
  // ascending contour becomes descending. The legacy classifier correctly
  // observes contour failure but currently stores that fact in GenreResult.
  const auto source = makeLine(60, 0, 80, 96);
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Revoice;
  req.genreId = static_cast<uint8_t>(GenerativeMode::Acid);

  const auto result =
      GroovePuterDevelopment::developCandidate(source, req, &source);

  check(result.evidence.bass.contourPreserved ==
            GroovePuterDevelopment::TriState::Fail,
        "D0-B9-evidence",
        "fixture must actually break the operation's contour promise");

  check(result.classification.genre !=
            GroovePuterDevelopment::GenreResult::Fail,
        "D0-B9",
        "broken operation promise must be OPERATION VIOLATED, not GENRE FAIL");
}

void b10_genre_valid_new_idea_is_not_a_contradiction() {
  const auto anchor = makeLine(48, 0, 52, 96);
  const auto unrelated = makeLine(61, 24, 67, 120);

  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::None;
  req.genreId = static_cast<uint8_t>(GenerativeMode::Acid);

  const auto result =
      GroovePuterDevelopment::developCandidate(unrelated, req, &anchor);

  check(result.classification.genre ==
            GroovePuterDevelopment::GenreResult::Pass &&
        result.classification.idea ==
            GroovePuterMaterial::IdeaClassification::NewIdea,
        "D0-B10",
        "genre-valid NEW_IDEA must remain representable");
}

}  // namespace

int main() {
  b7_tiny_change_can_be_genre_violation();
  b8_unavailable_capability_must_not_be_genre_failure();
  b9_operation_violation_must_not_be_genre_failure();
  b10_genre_valid_new_idea_is_not_a_contradiction();

  if (failures != 0) {
    std::printf("D0-B legacy classifier separation: RED (%d normative gaps)\n",
                failures);
    return 1;
  }

  std::puts("D0-B legacy classifier separation: PASS");
  return 0;
}
