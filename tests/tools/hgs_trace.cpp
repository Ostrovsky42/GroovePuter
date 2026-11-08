#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

#include "src/generation/generation_context.h"
#include "src/generation/harmony/generated/corpus_prior_data.h"
#include "src/generation/harmony/genre_harmonic_policy.h"
#include "src/generation/harmony/harmonic_fingerprint.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_route.h"
#include "src/generation/harmony/harmonic_scale.h"
#include "src/generation/harmony/harmonic_source.h"
#include "src/generation/harmony/song_variant_genome.h"
#include "src/generation/roles/chord_progression.h"

using namespace GroovePuterHarmony;

namespace {
constexpr std::array<const char*, 17> kStyles{{
    "Acid", "DetroitTechno", "Electro", "EBM", "UkGarage",
    "DubTechno", "Ambient", "LoFi", "Chillout", "DeepHouse",
    "Jungle", "Trap", "HipHop", "Footwork", "DnB", "Salsa",
    "Synthwave"}};
constexpr std::array<const char*, 2> kContexts{{"MAJOR", "MINOR"}};
constexpr std::array<const char*, 5> kSections{{
    "TAKE", "KEEP", "DEVELOP", "BREAK", "RETURN"}};
constexpr std::array<SectionIntent, 5> kIntents{{
    SectionIntent::Take, SectionIntent::Keep, SectionIntent::Develop,
    SectionIntent::Break, SectionIntent::Return}};

const char* statusName(HarmonyStatus status) {
  switch (status) {
    case HarmonyStatus::NoHarmony: return "NO_HARMONY";
    case HarmonyStatus::Ready: return "READY";
    case HarmonyStatus::Unsupported: return "UNSUPPORTED";
    case HarmonyStatus::Stale: return "STALE";
    case HarmonyStatus::Forbidden: return "FORBIDDEN";
    case HarmonyStatus::Invalid: return "INVALID";
    case HarmonyStatus::CapacityExceeded: return "CAPACITY_EXCEEDED";
    case HarmonyStatus::ReturnUnreachable: return "RETURN_UNREACHABLE";
  }
  return "UNKNOWN";
}

HarmonicState makeState(uint8_t root, QualityId qualityId) {
  const auto* quality = qualityDefinition(qualityId);
  return {qualityId, transposePitchSet(quality->intervalMask, root), root,
          kNoBassPc};
}

TonalContext makeContext(uint8_t scaleId, uint8_t tonic) {
  const auto* scale = scaleDefinition(scaleId);
  return {scaleId, scaleId, transposePitchSet(scale->intervalMask, tonic),
          transposePitchSet(scale->intervalMask, tonic), tonic, tonic};
}

uint8_t scaleDegreePc(const ScaleDefinition& scale, uint8_t degree) {
  uint8_t current = 0;
  for (uint8_t pitch = 0; pitch < 12; ++pitch) {
    if ((scale.intervalMask & (1u << pitch)) == 0) continue;
    if (current == degree) return pitch;
    ++current;
  }
  return 0;
}

bool sameEvent(const ResolvedHarmonicEvent& a,
               const ResolvedHarmonicEvent& b) {
  return a.state.qualityId == b.state.qualityId &&
         a.state.pitchMask == b.state.pitchMask &&
         a.state.rootPc == b.state.rootPc && a.state.bassPc == b.state.bassPc &&
         a.tonal.scaleId == b.tonal.scaleId &&
         a.tonal.tonicPc == b.tonal.tonicPc &&
         a.relationTags == b.relationTags && a.cadenceClass == b.cadenceClass &&
         a.timingSlotOrdinal == b.timingSlotOrdinal;
}

bool validEvent(const ResolvedHarmonicEvent& event,
                const GenreHarmonicPolicy& policy,
                uint32_t qualityCaps, uint32_t scaleCaps) {
  if (event.state.qualityId >= 32 || event.tonal.scaleId >= 32 ||
      (policy.qualityMask & (1u << event.state.qualityId)) == 0 ||
      (qualityCaps & (1u << event.state.qualityId)) == 0 ||
      (policy.scaleMask & (1u << event.tonal.scaleId)) == 0 ||
      (scaleCaps & (1u << event.tonal.scaleId)) == 0) return false;
  GroovePuterRhythm::HarmonicEvent projection{};
  return projectHarmonicState(event.state, event.tonal, projection) ==
         HarmonyStatus::Ready;
}

struct Totals {
  uint32_t invariantFailures = 0;
  uint32_t routeReady = 0;
  uint32_t routeFailed = 0;
  uint32_t changes = 0;
  uint32_t corpusPriorNonzero = 0;
  uint32_t corpusPriorSum = 0;
  uint32_t returnsHome = 0;
  uint32_t returnsFailed = 0;
};

struct ProfileSummary {
  std::set<uint8_t> roots;
  std::set<uint16_t> qualities;
  std::set<uint16_t> tagSets;
  std::set<uint32_t> contexts;
  std::set<uint64_t> normalizedRouteFingerprints;
  uint32_t statusCounts[8]{};
  uint32_t invariantFailures = 0;
  uint32_t changes = 0;
  uint32_t ready = 0;
  uint32_t failed = 0;
  uint32_t returnHome = 0;
  uint32_t returnFailed = 0;
  uint32_t corpusNonzero = 0;
  uint32_t corpusSum = 0;
  uint32_t normalizedRouteRepeats = 0;
};

void collectCorpusDiagnostic(const ResolvedHarmonicEvent* events,
                             uint8_t count, ProfileSummary& summary) {
  if (count < 3) return;
  CanonicalFunctionToken previous2{}, previous1{}, target{};
  if (canonicalFunctionTokenFor(events[0].state, events[0].tonal, previous2) !=
          HarmonyStatus::Ready ||
      canonicalFunctionTokenFor(events[1].state, events[1].tonal, previous1) !=
          HarmonyStatus::Ready) return;
  for (uint8_t i = 2; i < count; ++i) {
    if (canonicalFunctionTokenFor(events[i].state, events[i].tonal, target) !=
        HarmonyStatus::Ready) continue;
    const uint8_t prior = corpusPrior(previous2, previous1, target,
                                      GeneratedCorpusPrior::kModel);
    summary.corpusSum += prior;
    if (prior != 0) ++summary.corpusNonzero;
    previous2 = previous1;
    previous1 = target;
  }
}

bool run(int seedCount, const std::string& outDir) {
  std::ofstream traces(outDir + "/traces.tsv");
  std::ofstream summaries(outDir + "/summary.tsv");
  std::ofstream examples(outDir + "/ab_examples.tsv");
  if (!traces || !summaries || !examples) {
    std::cerr << "cannot open output package under " << outDir << '\n';
    return false;
  }
  traces << "profile_scope\tstyle\tcontext\tseed\tsection\tstatus\tevent\troot_pc\tquality_id\tscale_id\ttonic_pc\trelation_tags\tcadence\ttiming_slot\n";
  summaries << "profile_scope\tstyle\tcontext\tseeds_requested\tseeds_evaluated\troute_failures_typed\tpolicy_quality_count\tquality_pruned_by_materializer\tpolicy_scale_count\tscale_pruned_by_materializer\troot_diversity\tquality_diversity\ttag_signature_diversity\tcontext_signature_diversity\tnormalized_route_unique\tnormalized_route_repeats\tchanges\tcorpus_prior_nonzero\tcorpus_prior_sum\treturn_home\treturn_failed\troute_invariant_failures\tdeterminism\thome_valid\n";
  examples << "profile_scope\tstyle\tcontext\tseed\tsection\tpair\tevent\troot_pc\tquality_id\tscale_id\ttiming_slot\n";

  const MaterializerCapabilityView caps = materializerCapabilityView();
  Totals totals{};
  for (uint8_t styleId = 0; styleId < kStyles.size(); ++styleId) {
    const auto style = static_cast<HarmonicStyleId>(styleId);
    const GenreHarmonicPolicy* policy = policyFor(style);
    if (policy == nullptr) return false;
    const char* profileScope = policy->hostProfileOnly ? "HOST_PROFILE_ONLY" : "DEVICE_MAPPED";
    for (uint8_t contextIndex = 0; contextIndex < kContexts.size(); ++contextIndex) {
      const uint8_t scaleId = contextIndex == 0 ? 1 : 0;
      const auto* scale = scaleDefinition(scaleId);
      const bool minor = contextIndex == 1;
      const QualityId homeQuality = minor ? kQualityMinor7 : kQualityMajor;
      const uint32_t runtimeQualityMask = policy->qualityMask & caps.qualityMask;
      const uint32_t runtimeScaleMask = policy->scaleMask & caps.scaleMask;
      const bool homeValid = scale != nullptr && homeQuality < 32 &&
          (runtimeQualityMask & (1u << homeQuality)) != 0 &&
          (runtimeScaleMask & (1u << scaleId)) != 0 &&
          qualityDefinition(homeQuality) != nullptr;
      ProfileSummary summary{};
      uint32_t completed = 0;
      RecentHarmonyMemory exampleMemory{};
      for (int seed = 0; seed < seedCount; ++seed) {
        const uint32_t projectSeed = static_cast<uint32_t>(seed) + 1u;
        const uint32_t songIdentity = 0x48475300u ^
            (static_cast<uint32_t>(styleId) << 16u) ^
            (static_cast<uint32_t>(contextIndex) << 8u) ^ projectSeed;
        const HarmonicState home = makeState(0, homeQuality);
        const TonalContext homeContext = makeContext(scaleId, 0);
        HarmonicState current = home;
        TonalContext currentContext = homeContext;
        RecentHarmonyMemory recent{};
        for (uint8_t sectionIndex = 0; sectionIndex < kSections.size(); ++sectionIndex) {
          RouteRequest request{};
          request.projectSeed = projectSeed;
          request.songIdentity = songIdentity;
          request.modelVersion = GeneratedCorpusPrior::kModelVersion;
          request.globalEventOrdinal = static_cast<uint16_t>(sectionIndex * 4u);
          request.firstTimingSlot = 0;
          request.availableTimingSlots = 4;
          request.repetitionTolerance = 2;
          request.candidateRequest.current = current;
          request.candidateRequest.currentContext = currentContext;
          request.candidateRequest.home = home;
          request.candidateRequest.homeContext = homeContext;
          request.candidateRequest.genome = deriveSongVariantGenome(
              projectSeed, songIdentity, style,
              GeneratedCorpusPrior::kModelVersion);
          request.candidateRequest.style = style;
          request.candidateRequest.section = kIntents[sectionIndex];
          request.candidateRequest.runtimeQualityMask = runtimeQualityMask;
          request.candidateRequest.runtimeScaleMask = runtimeScaleMask;
          request.candidateRequest.corpus = GeneratedCorpusPrior::kModel;

          RouteScratch scratch{};
          RouteCandidateWorkspace workspace{};
          ResolvedHarmonicEvent events[32]{};
          uint8_t count = 0;
          HarmonicState terminal{};
          TonalContext terminalContext{};
          const RecentHarmonyMemory memoryBefore = recent;
          const HarmonyStatus status = planRoute(request, recent, scratch,
              workspace, events, 32, count, terminal, terminalContext);
          uint8_t repeatCount = 0;
          HarmonicState terminalRepeat{};
          TonalContext contextRepeat{};
          ResolvedHarmonicEvent repeatedEvents[32]{};
          const HarmonyStatus repeated = planRoute(request, memoryBefore, scratch,
              workspace, repeatedEvents, 32, repeatCount, terminalRepeat,
              contextRepeat);
          bool deterministic = status == repeated && count == repeatCount &&
              terminal.rootPc == terminalRepeat.rootPc &&
              terminal.qualityId == terminalRepeat.qualityId &&
              terminalContext.scaleId == contextRepeat.scaleId &&
              terminalContext.tonicPc == contextRepeat.tonicPc;
          for (uint8_t i = 0; deterministic && i < count; ++i)
            deterministic = sameEvent(events[i], repeatedEvents[i]);
          if (!deterministic) ++summary.invariantFailures;
          ++summary.statusCounts[static_cast<uint8_t>(status)];
          if (status != HarmonyStatus::Ready) {
            ++summary.failed;
            ++totals.routeFailed;
            if (kIntents[sectionIndex] == SectionIntent::Return)
              ++summary.returnFailed;
            traces << profileScope << '\t' << kStyles[styleId] << '\t'
                   << kContexts[contextIndex] << '\t' << seed << '\t'
                   << kSections[sectionIndex] << '\t' << statusName(status)
                   << "\t-1\t-1\t-1\t-1\t-1\t0\t0\t-1\n";
            if (status == HarmonyStatus::Invalid || status == HarmonyStatus::Forbidden ||
                status == HarmonyStatus::CapacityExceeded) {
              // Forbidden/capacity are explicit planner outcomes, not malformed
              // output. Invalid requests indicate a broken fixture/contract.
              if (status == HarmonyStatus::Invalid)
                ++summary.invariantFailures;
            }
            continue;
          }
          ++summary.ready;
          ++totals.routeReady;
          for (uint8_t i = 0; i < count; ++i) {
            const auto& event = events[i];
            if (!validEvent(event, *policy, caps.qualityMask, caps.scaleMask))
              ++summary.invariantFailures;
            if (event.state.rootPc > 11 || event.state.qualityId >= 32 ||
                event.tonal.scaleId >= 32 || count > 32 ||
                event.timingSlotOrdinal > 31) ++summary.invariantFailures;
            summary.roots.insert(event.state.rootPc);
            summary.qualities.insert(event.state.qualityId);
            summary.tagSets.insert(event.relationTags);
            summary.contexts.insert((static_cast<uint32_t>(event.tonal.scaleId) << 8u) |
                                    event.tonal.tonicPc);
            traces << profileScope << '\t' << kStyles[styleId] << '\t'
                   << kContexts[contextIndex] << '\t' << seed << '\t'
                   << kSections[sectionIndex] << "\tREADY\t" << unsigned(i)
                   << '\t' << unsigned(event.state.rootPc) << '\t'
                   << event.state.qualityId << '\t' << event.tonal.scaleId
                   << '\t' << unsigned(event.tonal.tonicPc) << '\t'
                   << event.relationTags << '\t'
                   << unsigned(event.cadenceClass) << '\t'
                   << unsigned(event.timingSlotOrdinal) << '\n';
            if (i > 0 && (events[i - 1].state.rootPc != event.state.rootPc ||
                          events[i - 1].state.qualityId != event.state.qualityId))
              ++summary.changes;
          }
          HarmonicFingerprintSet routeFingerprint{};
          const ResolvedRouteView routeView{events, count, home, homeContext};
          if (fingerprintRoute(routeView, routeFingerprint) == HarmonyStatus::Ready) {
            const uint64_t normalized = routeFingerprint.rootMotion;
            if (!summary.normalizedRouteFingerprints.insert(normalized).second)
              ++summary.normalizedRouteRepeats;
          } else {
            ++summary.invariantFailures;
          }
          collectCorpusDiagnostic(events, count, summary);
          if (kIntents[sectionIndex] == SectionIntent::Return) {
            const bool returned = terminal.rootPc == home.rootPc &&
                terminal.qualityId == home.qualityId &&
                terminalContext.scaleId == homeContext.scaleId &&
                terminalContext.tonicPc == homeContext.tonicPc;
            if (returned) ++summary.returnHome;
            else {
              ++summary.returnFailed;
              ++summary.invariantFailures;
            }
          }
          const ResolvedRouteView accepted{events, count, home, homeContext};
          if (commitAcceptedRoute(recent, accepted) != HarmonyStatus::Ready)
            ++summary.invariantFailures;
          current = terminal;
          currentContext = terminalContext;
        }
        ++completed;

        // Ten deterministic examples per profile/context. A and B use the
        // same fixed four-slot timing fixture; this is event evidence, not audio.
        if (seed < 10) {
          RouteRequest exampleRequest{};
          exampleRequest.projectSeed = projectSeed;
          exampleRequest.songIdentity = songIdentity;
          exampleRequest.modelVersion = GeneratedCorpusPrior::kModelVersion;
          exampleRequest.availableTimingSlots = 4;
          exampleRequest.candidateRequest.current = home;
          exampleRequest.candidateRequest.currentContext = homeContext;
          exampleRequest.candidateRequest.home = home;
          exampleRequest.candidateRequest.homeContext = homeContext;
          exampleRequest.candidateRequest.genome = deriveSongVariantGenome(
              projectSeed, songIdentity, style, GeneratedCorpusPrior::kModelVersion);
          exampleRequest.candidateRequest.style = style;
          exampleRequest.candidateRequest.section = SectionIntent::Develop;
          exampleRequest.candidateRequest.structuralMotionAllowed = 1;
          exampleRequest.candidateRequest.runtimeQualityMask = runtimeQualityMask;
          exampleRequest.candidateRequest.runtimeScaleMask = runtimeScaleMask;
          exampleRequest.candidateRequest.corpus = GeneratedCorpusPrior::kModel;
          RouteScratch exampleScratch{};
          RouteCandidateWorkspace exampleWorkspace{};
          ResolvedHarmonicEvent graphEvents[32]{};
          uint8_t graphCount = 0;
          HarmonicState graphEnd{};
          TonalContext graphTonal{};
          const HarmonyStatus graphStatus = planRoute(exampleRequest, exampleMemory,
              exampleScratch, exampleWorkspace, graphEvents, 32, graphCount,
              graphEnd, graphTonal);
          GroovePuterRhythm::ChordProgressionSourceRequest legacyRequest{};
          legacyRequest.requestedId = GroovePuterRhythm::ProgressionId::Auto;
          legacyRequest.family = GroovePuterRhythm::RhythmFamily::FourFloor;
          legacyRequest.generation = {projectSeed,
              static_cast<uint16_t>(seed)};
          legacyRequest.phraseBars = 1;
          const auto legacy = GroovePuterRhythm::realizeChordProgressionSource(legacyRequest);
          for (uint8_t i = 0; i < 4; ++i) {
            if (graphStatus == HarmonyStatus::Ready && graphCount != 0) {
              const auto& event = graphEvents[i % graphCount];
              examples << profileScope << '\t' << kStyles[styleId] << '\t'
                       << kContexts[contextIndex] << '\t' << seed << "\tDEVELOP\tB_GRAPH\t"
                       << unsigned(i) << '\t' << unsigned(event.state.rootPc) << '\t'
                       << event.state.qualityId << '\t' << event.tonal.scaleId << '\t'
                       << unsigned(i) << '\n';
            } else {
              examples << profileScope << '\t' << kStyles[styleId] << '\t'
                       << kContexts[contextIndex] << '\t' << seed << "\tDEVELOP\tB_GRAPH_"
                       << statusName(graphStatus) << '\t' << unsigned(i)
                       << "\t-1\t-1\t-1\t" << unsigned(i) << '\n';
            }
            if ((legacy.status == GroovePuterRhythm::ChordProgressionStatus::Ok ||
                 legacy.status == GroovePuterRhythm::ChordProgressionStatus::ValidButStatic) &&
                legacy.source.period != 0) {
              const auto& legacyEvent = legacy.source.events[i % legacy.source.period];
              QualityId quality{};
              GroovePuterHarmony::legacyQualityToId(legacyEvent.quality, quality);
              int rootValue = static_cast<int>(
                  scaleDegreePc(*scale, legacyEvent.degree)) +
                  legacyEvent.rootOffsetSemitones;
              while (rootValue < 0) rootValue += 12;
              const uint8_t root = static_cast<uint8_t>(rootValue % 12);
              examples << profileScope << '\t' << kStyles[styleId] << '\t'
                       << kContexts[contextIndex] << '\t' << seed
                       << "\tDEVELOP\tA_LEGACY\t" << unsigned(i) << '\t'
                       << unsigned(root) << '\t' << quality << '\t' << scaleId
                       << '\t' << unsigned(i) << '\n';
            }
          }
          if (graphStatus == HarmonyStatus::Ready) {
            const ResolvedRouteView exampleRoute{graphEvents, graphCount, home,
                                                  homeContext};
            if (commitAcceptedRoute(exampleMemory, exampleRoute) !=
                HarmonyStatus::Ready) ++summary.invariantFailures;
          }
        }
      }

      const uint32_t typedFailures = summary.failed;
      auto countBits = [](uint32_t value) {
        uint16_t count = 0;
        while (value != 0) {
          count = static_cast<uint16_t>(count + (value & 1u));
          value >>= 1u;
        }
        return count;
      };
      summaries << profileScope << '\t' << kStyles[styleId] << '\t'
                << kContexts[contextIndex] << '\t' << seedCount << '\t'
                << completed << '\t' << typedFailures << '\t'
                << countBits(policy->qualityMask) << '\t'
                << countBits(policy->qualityMask & ~caps.qualityMask) << '\t'
                << countBits(policy->scaleMask) << '\t'
                << countBits(policy->scaleMask & ~caps.scaleMask) << '\t'
                << summary.roots.size() << '\t' << summary.qualities.size() << '\t'
                << summary.tagSets.size() << '\t' << summary.contexts.size() << '\t'
                << summary.normalizedRouteFingerprints.size() << '\t'
                << summary.normalizedRouteRepeats << '\t' << summary.changes << '\t'
                << summary.corpusNonzero << '\t' << summary.corpusSum << '\t'
                << summary.returnHome << '\t' << summary.returnFailed << '\t'
                << summary.invariantFailures << '\t'
                << (summary.invariantFailures == 0 ? "PASS" : "FAIL") << '\t'
                << (homeValid ? "YES" : "NO") << '\n';
      totals.invariantFailures += summary.invariantFailures;
      totals.changes += summary.changes;
      totals.corpusPriorNonzero += summary.corpusNonzero;
      totals.corpusPriorSum += summary.corpusSum;
      totals.returnsHome += summary.returnHome;
      totals.returnsFailed += summary.returnFailed;
    }
  }
  std::cout << "styles=" << kStyles.size() << " contexts=" << kContexts.size()
            << " seeds=" << seedCount << " route_ready=" << totals.routeReady
            << " route_failed=" << totals.routeFailed
            << " invariant_failures=" << totals.invariantFailures
            << " changes=" << totals.changes
            << " corpus_prior_nonzero=" << totals.corpusPriorNonzero
            << " corpus_prior_sum=" << totals.corpusPriorSum
            << " return_home=" << totals.returnsHome
            << " return_failed=" << totals.returnsFailed << '\n';
  return totals.invariantFailures == 0;
}
}  // namespace

int main(int argc, char** argv) {
  int seedCount = 256;
  std::string outDir = "build/hgs-listening-package/evidence";
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--seed-count" && i + 1 < argc) {
      seedCount = std::stoi(argv[++i]);
    } else if (arg == "--out-dir" && i + 1 < argc) {
      outDir = argv[++i];
    } else {
      std::cerr << "unknown or incomplete argument: " << arg << '\n';
      return 2;
    }
  }
  if (seedCount < 1 || seedCount > 256) {
    std::cerr << "seed count must be in 1..256\n";
    return 2;
  }
  return run(seedCount, outDir) ? 0 : 1;
}
