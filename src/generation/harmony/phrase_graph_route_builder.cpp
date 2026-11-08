#include "phrase_graph_route_builder.h"

#include "corpus_prior.h"
#include "generated/corpus_prior_data.h"
#include "harmonic_quality.h"
#include "harmonic_scale.h"
#include "genre_harmonic_policy.h"
#include "harmonic_source.h"
#include "scenes.h"

namespace GroovePuterHarmony {
namespace {

constexpr uint16_t kProductionHarmonicModelVersion =
    GeneratedCorpusPrior::kModelVersion;

PhraseGraphRouteBuildStatus mapStatus(HarmonyStatus status) {
  switch (status) {
    case HarmonyStatus::Ready: return PhraseGraphRouteBuildStatus::Ready;
    case HarmonyStatus::Unsupported:
      return PhraseGraphRouteBuildStatus::UnsupportedProjection;
    case HarmonyStatus::Invalid:
      return PhraseGraphRouteBuildStatus::InvalidContext;
    case HarmonyStatus::NoHarmony:
    case HarmonyStatus::Stale:
    case HarmonyStatus::Forbidden:
    case HarmonyStatus::CapacityExceeded:
    case HarmonyStatus::ReturnUnreachable:
      return PhraseGraphRouteBuildStatus::PlannerFailure;
  }
  return PhraseGraphRouteBuildStatus::InvalidContext;
}

}  // namespace

PhraseGraphRouteBuildStatus graphRouteCapability(
    const ::GenreSettings& settings, uint8_t scaleId) {
  if (settings.harmonicStrategy != 1) {
    return PhraseGraphRouteBuildStatus::Disabled;
  }
  HarmonicStyleId style{};
  if (harmonicStyleFor(settings, style) != HarmonyStatus::Ready) {
    return PhraseGraphRouteBuildStatus::UnsupportedGenre;
  }
  const GenreHarmonicPolicy* policy = policyFor(style);
  if (policy == nullptr) return PhraseGraphRouteBuildStatus::UnsupportedGenre;

  const ScaleDefinition* scale = scaleDefinition(scaleId);
  const MaterializerCapabilityView capabilities = materializerCapabilityView();
  if (scale == nullptr || scaleId >= 32 ||
      (policy->scaleMask & (1u << scaleId)) == 0 ||
      (capabilities.scaleMask & (1u << scaleId)) == 0) {
    return PhraseGraphRouteBuildStatus::UnsupportedScale;
  }

  const bool minorHome =
      (scale->intervalMask & (1u << 3u)) != 0 &&
      (scale->intervalMask & (1u << 4u)) == 0;
  const QualityId homeQuality = minorHome ? kQualityMinor7 : kQualityMajor;
  if (homeQuality >= 32 ||
      (policy->qualityMask & (1u << homeQuality)) == 0 ||
      (capabilities.qualityMask & (1u << homeQuality)) == 0) {
    return PhraseGraphRouteBuildStatus::UnsupportedHome;
  }
  const ChordQualityDefinition* quality = qualityDefinition(homeQuality);
  if (quality == nullptr) return PhraseGraphRouteBuildStatus::UnsupportedHome;

  HarmonicState home{
      homeQuality, quality->intervalMask, 0, kNoBassPc};
  TonalContext tonal{
      scaleId, scaleId, scale->intervalMask, scale->intervalMask, 0, 0};
  GroovePuterRhythm::HarmonicEvent projected{};
  if (projectHarmonicState(home, tonal, projected) != HarmonyStatus::Ready) {
    return PhraseGraphRouteBuildStatus::UnsupportedProjection;
  }
  return PhraseGraphRouteBuildStatus::Ready;
}

PhraseGraphRouteBuildStatus buildPhraseGraphRoute(
    const ::GenreSettings& settings,
    uint8_t rootPitchClass,
    uint8_t scaleId,
    uint8_t eventCount,
    uint32_t projectSeed,
    uint32_t songIdentity,
    uint32_t materialId,
    uint32_t publicationGeneration,
    const RecentHarmonyMemory& recent,
    RouteScratch& routeScratch,
    RouteCandidateWorkspace& candidateWorkspace,
    PreparedGraphHarmony& graphOut,
    PreparedPhraseGraphMetadata& metadataOut,
    const HarmonicState* current,
    const TonalContext* currentTonal,
    SectionIntent section) {
  if (settings.harmonicStrategy != 1) {
    return PhraseGraphRouteBuildStatus::Disabled;
  }
  if (rootPitchClass > 11 || eventCount == 0 || eventCount > 32 ||
      projectSeed == 0 || songIdentity == 0 || materialId == 0 ||
      publicationGeneration == 0 || (current == nullptr) !=
          (currentTonal == nullptr) ||
      static_cast<uint8_t>(section) >
          static_cast<uint8_t>(SectionIntent::Return)) {
    return PhraseGraphRouteBuildStatus::InvalidContext;
  }

  const PhraseGraphRouteBuildStatus capability =
      graphRouteCapability(settings, scaleId);
  if (capability != PhraseGraphRouteBuildStatus::Ready) return capability;
  HarmonicStyleId style{};
  if (harmonicStyleFor(settings, style) != HarmonyStatus::Ready) {
    return PhraseGraphRouteBuildStatus::UnsupportedGenre;
  }
  const GenreHarmonicPolicy* policy = policyFor(style);
  if (policy == nullptr) return PhraseGraphRouteBuildStatus::UnsupportedGenre;

  const ScaleDefinition* scale = scaleDefinition(scaleId);
  const MaterializerCapabilityView capabilities = materializerCapabilityView();
  if (scale == nullptr) return PhraseGraphRouteBuildStatus::UnsupportedScale;

  const bool minorHome =
      (scale->intervalMask & (1u << 3u)) != 0 &&
      (scale->intervalMask & (1u << 4u)) == 0;
  const QualityId homeQuality = minorHome ? kQualityMinor7 : kQualityMajor;
  if (homeQuality >= 32 ||
      (policy->qualityMask & (1u << homeQuality)) == 0 ||
      (capabilities.qualityMask & (1u << homeQuality)) == 0) {
    return PhraseGraphRouteBuildStatus::UnsupportedHome;
  }
  const ChordQualityDefinition* quality = qualityDefinition(homeQuality);
  if (quality == nullptr) return PhraseGraphRouteBuildStatus::UnsupportedHome;

  PreparedPhraseGraphMetadata metadata{};
  metadata.style = style;
  metadata.genome = deriveSongVariantGenome(
      projectSeed, songIdentity, style, kProductionHarmonicModelVersion);
  metadata.home = {
      homeQuality,
      transposePitchSet(quality->intervalMask, rootPitchClass),
      rootPitchClass,
      kNoBassPc};
  metadata.homeTonal = {
      scaleId,
      scaleId,
      transposePitchSet(scale->intervalMask, rootPitchClass),
      transposePitchSet(scale->intervalMask, rootPitchClass),
      rootPitchClass,
      rootPitchClass};

  RouteRequest request{};
  request.projectSeed = projectSeed;
  request.songIdentity = songIdentity;
  request.modelVersion = kProductionHarmonicModelVersion;
  request.firstTimingSlot = 0;
  request.availableTimingSlots = eventCount;
  request.repetitionTolerance = metadata.genome.repetitionTolerance;
  request.candidateRequest.current =
      current != nullptr ? *current : metadata.home;
  request.candidateRequest.currentContext =
      currentTonal != nullptr ? *currentTonal : metadata.homeTonal;
  request.candidateRequest.home = metadata.home;
  request.candidateRequest.homeContext = metadata.homeTonal;
  request.candidateRequest.genome = metadata.genome;
  request.candidateRequest.style = style;
  request.candidateRequest.section = section;
  request.candidateRequest.structuralMotionAllowed = 1;
  request.candidateRequest.runtimeQualityMask =
      policy->qualityMask & capabilities.qualityMask;
  request.candidateRequest.runtimeScaleMask =
      policy->scaleMask & capabilities.scaleMask;
  request.candidateRequest.corpus = GeneratedCorpusPrior::kModel;

  PreparedGraphHarmony graph{};
  const HarmonyStatus routeStatus = planGraphHarmony(
      request, recent, routeScratch, candidateWorkspace, materialId,
      publicationGeneration, graph, metadata.terminal,
      metadata.terminalTonal);
  if (routeStatus != HarmonyStatus::Ready) return mapStatus(routeStatus);

  // A legal graph edge is not enough: every selected event must also fit the
  // existing bounded phrase materializer. Reject the whole plan before a
  // caller can publish a partially projectable route.
  for (uint8_t i = 0; i < graph.eventCount; ++i) {
    GroovePuterRhythm::HarmonicEvent projected{};
    if (projectHarmonicState(graph.events[i].state,
                             graph.events[i].tonal, projected) !=
        HarmonyStatus::Ready) {
      return PhraseGraphRouteBuildStatus::UnsupportedProjection;
    }
  }

  graphOut = graph;
  metadataOut = metadata;
  return PhraseGraphRouteBuildStatus::Ready;
}

}  // namespace GroovePuterHarmony
