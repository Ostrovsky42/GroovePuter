#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "src/generation/harmony/harmonic_scale.h"
#include "src/generation/roles/chord_progression.h"

using namespace GroovePuterRhythm;

static_assert(static_cast<uint8_t>(ProgressionId::Auto) == 0);
static_assert(static_cast<uint8_t>(ProgressionId::StaticModal) == 1);
static_assert(static_cast<uint8_t>(ProgressionId::PedalDrone) == 2);
static_assert(static_cast<uint8_t>(ProgressionId::PopCycle) == 3);
static_assert(static_cast<uint8_t>(ProgressionId::TwoFiveOne) == 4);
static_assert(static_cast<uint8_t>(ProgressionId::ParallelShift) == 5);
static_assert(static_cast<uint8_t>(ProgressionId::MinorFall) == 6);
static_assert(static_cast<uint8_t>(ProgressionId::BorrowedLift) == 7);
static_assert(static_cast<uint8_t>(ProgressionId::Count) == 8);
static_assert(static_cast<uint8_t>(ChordQuality::Triad) == 0);
static_assert(static_cast<uint8_t>(ChordQuality::Minor7) == 1);
static_assert(static_cast<uint8_t>(ChordQuality::Major7) == 2);
static_assert(static_cast<uint8_t>(ChordQuality::Dominant7) == 3);
static_assert(static_cast<uint8_t>(ChordQuality::Sus4) == 4);
static_assert(static_cast<uint8_t>(ChordQuality::Minor9) == 5);
static_assert(static_cast<uint8_t>(ChordQuality::Major9) == 6);
static_assert(static_cast<uint8_t>(ChordQuality::Diminished) == 7);
static_assert(static_cast<uint8_t>(ChordQuality::Count) == 8);
static_assert(static_cast<uint8_t>(RhythmFamily::FourFloor) == 0);
static_assert(static_cast<uint8_t>(RhythmFamily::MachineSyncopation) == 1);
static_assert(static_cast<uint8_t>(RhythmFamily::Breakbeat) == 2);
static_assert(static_cast<uint8_t>(RhythmFamily::UkTwoStep) == 3);
static_assert(static_cast<uint8_t>(RhythmFamily::HipHopBackbeat) == 4);
static_assert(static_cast<uint8_t>(RhythmFamily::DubPulse) == 5);
static_assert(static_cast<uint8_t>(RhythmFamily::Funk16) == 6);
static_assert(static_cast<uint8_t>(RhythmFamily::SparsePulse) == 7);
static_assert(static_cast<uint8_t>(RhythmFamily::Count) == 8);
static_assert(static_cast<uint8_t>(GenerationDomain::ArchetypeSelection) == 0);
static_assert(static_cast<uint8_t>(GenerationDomain::RhythmIdentity) == 1);
static_assert(static_cast<uint8_t>(GenerationDomain::P1Variation) == 2);
static_assert(static_cast<uint8_t>(GenerationDomain::P2Variation) == 3);
static_assert(static_cast<uint8_t>(GenerationDomain::P3Transformation) == 4);
static_assert(static_cast<uint8_t>(GenerationDomain::TransformationIntent) == 5);
static_assert(static_cast<uint8_t>(GenerationDomain::DrumsOrnament) == 6);
static_assert(static_cast<uint8_t>(GenerationDomain::BassPitch) == 7);
static_assert(static_cast<uint8_t>(GenerationDomain::ChordPitch) == 8);
static_assert(static_cast<uint8_t>(GenerationDomain::LeadPitch) == 9);
static_assert(static_cast<uint8_t>(GenerationDomain::FeelExpression) == 10);
static_assert(static_cast<uint8_t>(GenerationDomain::PhysicalBinding) == 11);
static_assert(static_cast<uint8_t>(GenerationDomain::BarEvolution) == 12);
static_assert(static_cast<uint8_t>(GenerationDomain::BassRhythmSelection) == 13);
static_assert(static_cast<uint8_t>(GenerationDomain::ChordRhythmSelection) == 14);
static_assert(static_cast<uint8_t>(GenerationDomain::MelodicRhythmSelection) == 15);
static_assert(static_cast<uint8_t>(GenerationDomain::MotifSelection) == 16);
static_assert(static_cast<uint8_t>(GenerationDomain::FeelProfileSelection) == 17);
static_assert(static_cast<uint8_t>(GenerationDomain::PhraseLawSelection) == 18);
static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicGenome) == 19);
static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicCandidateTieBreak) == 20);
static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicRoute) == 21);
static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicModulation) == 22);
static_assert(static_cast<uint8_t>(GenerationDomain::HarmonicVerticalColor) == 23);
static_assert(static_cast<uint8_t>(GenerationDomain::PerformanceArp) == 24);
static_assert(static_cast<uint8_t>(GenerationDomain::Count) == 25);
static_assert(static_cast<uint8_t>(ChordProgressionStatus::Ok) == 0);
static_assert(static_cast<uint8_t>(ChordProgressionStatus::ValidButStatic) == 1);
static_assert(static_cast<uint8_t>(ChordProgressionStatus::InvalidRequest) == 2);
static_assert(static_cast<uint8_t>(ChordProgressionStatus::Count) == 3);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::SectionIntent::Take) == 0);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::SectionIntent::Keep) == 1);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::SectionIntent::Develop) == 2);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::SectionIntent::Break) == 3);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::SectionIntent::Return) == 4);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonyStatus::NoHarmony) == 0);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonyStatus::Ready) == 1);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonyStatus::Unsupported) == 2);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonyStatus::Stale) == 3);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonyStatus::Forbidden) == 4);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonyStatus::Invalid) == 5);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonyStatus::CapacityExceeded) == 6);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonyStatus::ReturnUnreachable) == 7);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonicAuthority::Unknown) == 0);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonicAuthority::Generic) == 1);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonicAuthority::Idiom) == 2);
static_assert(static_cast<uint8_t>(GroovePuterHarmony::HarmonicAuthority::Graph) == 3);
static_assert(GroovePuterHarmony::kQualityMajor == 0);
static_assert(GroovePuterHarmony::kQualityMinor9 == 8);
static_assert(GroovePuterHarmony::kQualityMajor9 == 9);
static_assert(GroovePuterHarmony::kBootstrapQualityCount == 10);
static_assert(GroovePuterHarmony::kBootstrapScaleCount == 10);
static_assert(sizeof(HarmonicEvent) == 3);
static_assert(sizeof(ChordProgressionSource) == 14);
static_assert(sizeof(ChordProgressionSourceRequest) == 16);
static_assert(sizeof(ChordProgressionSourceResult) == 15);
static_assert(sizeof(ChordProgressionPlan) == 26);
static_assert(sizeof(ChordProgressionRequest) == 16);
static_assert(sizeof(GroovePuterHarmony::HarmonyEventKey) == 16);
static_assert(sizeof(GroovePuterHarmony::HarmonySnapshot) == 56);

namespace {

constexpr uint8_t kBars[] = {1, 2, 4, 8};

std::string serializeSource(uint32_t seed,
                            ProgressionId requested,
                            RhythmFamily family,
                            uint8_t bars) {
  ChordProgressionSourceRequest request{};
  request.requestedId = requested;
  request.family = family;
  request.generation = GenerationContext{seed, 0};
  request.phraseBars = bars;
  const ChordProgressionSourceResult result =
      realizeChordProgressionSource(request);

  std::ostringstream line;
  line << "S|" << seed << '|'
       << static_cast<unsigned>(requested) << '|'
       << static_cast<unsigned>(family) << '|'
       << static_cast<unsigned>(bars) << '|'
       << static_cast<unsigned>(result.status) << '|'
       << static_cast<unsigned>(result.source.id) << '|'
       << static_cast<unsigned>(result.source.period) << '|';
  for (uint8_t index = 0; index < result.source.period; ++index) {
    if (index != 0) line << ',';
    const HarmonicEvent& event = result.source.events[index];
    line << static_cast<unsigned>(event.degree) << ':'
         << static_cast<unsigned>(event.quality) << ':'
         << static_cast<int>(event.rootOffsetSemitones);
  }
  line << '|';
  const uint8_t traceCount = static_cast<uint8_t>(result.source.period * 2u);
  for (uint8_t ordinal = 0; ordinal < traceCount; ++ordinal) {
    if (ordinal != 0) line << ',';
    const ChordProgressionEventResult event =
        chordProgressionEventAt(result.source, ordinal);
    line << static_cast<unsigned>(event.status) << ':'
         << static_cast<unsigned>(event.event.degree) << ':'
         << static_cast<unsigned>(event.event.quality) << ':'
         << static_cast<int>(event.event.rootOffsetSemitones);
  }
  return line.str();
}

std::string serializeInvalidRequests() {
  std::ostringstream output;
  auto append = [&output](const char* name,
                          const ChordProgressionSourceRequest& request) {
    const auto result = realizeChordProgressionSource(request);
    output << "I|" << name << '|'
           << static_cast<unsigned>(result.status) << '\n';
  };

  ChordProgressionSourceRequest request{};
  request.requestedId = ProgressionId::Count;
  append("progression-count", request);
  request = {};
  request.family = RhythmFamily::Count;
  append("family-count", request);
  request = {};
  request.phraseBars = 0;
  append("bars-zero", request);
  request.phraseBars = 3;
  append("bars-three", request);

  ChordProgressionRequest plan{};
  plan.requestedId = ProgressionId::PopCycle;
  plan.harmonicEventCount = kMaxHarmonicEvents + 1u;
  output << "P|event-count-nine|"
         << static_cast<unsigned>(realizeChordProgression(plan).status) << '\n';

  ChordProgressionSource corrupt{};
  corrupt.id = ProgressionId::PopCycle;
  corrupt.period = kMaxChordProgressionSourceEvents + 1u;
  output << "E|period-five|"
         << static_cast<unsigned>(chordProgressionEventAt(corrupt, 0).status)
         << '\n';
  return output.str();
}

std::string makeFixture() {
  std::ostringstream output;
  output << "# hgs-legacy-replay-v1\n"
         << "# Explicit ids use FourFloor; AUTO covers every valid RhythmFamily.\n"
         << "# Fields serialize semantic members; compiler padding is never observed.\n";
  for (uint32_t seed = 0; seed < 64; ++seed) {
    for (uint8_t bars : kBars) {
      for (uint8_t family = 0;
           family < static_cast<uint8_t>(RhythmFamily::Count); ++family) {
        output << serializeSource(seed, ProgressionId::Auto,
                                  static_cast<RhythmFamily>(family), bars)
               << '\n';
      }
      for (uint8_t id = static_cast<uint8_t>(ProgressionId::StaticModal);
           id < static_cast<uint8_t>(ProgressionId::Count); ++id) {
        output << serializeSource(seed, static_cast<ProgressionId>(id),
                                  RhythmFamily::FourFloor, bars)
               << '\n';
      }
    }
  }
  output << serializeInvalidRequests();
  return output.str();
}

}  // namespace

int main(int argc, char** argv) {
  const std::string expected = makeFixture();
  if (argc == 2 && std::string(argv[1]) == "--write-fixture") {
    std::ofstream fixture("tests/fixtures/hgs_legacy_replay.tsv",
                          std::ios::binary | std::ios::trunc);
    assert(fixture.good());
    fixture << expected;
    assert(fixture.good());
    return 0;
  }
  assert(argc == 1);
  std::ifstream fixture("tests/fixtures/hgs_legacy_replay.tsv",
                        std::ios::binary);
  assert(fixture.good());
  const std::string actual((std::istreambuf_iterator<char>(fixture)),
                           std::istreambuf_iterator<char>());
  if (actual != expected) {
    std::cerr << "HGS legacy replay differs from frozen witness\n";
    return 1;
  }
  std::cout << "HGS legacy replay: PASS (64 seeds, ids, families, bars, invalid requests)\n";
  return 0;
}
