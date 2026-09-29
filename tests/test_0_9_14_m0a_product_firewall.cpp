#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/phrase_generator.h"
#include "src/state/generated_synth_a_origin.h"
#include "src/dsp/p0_preservation_evaluator.h"
#include "src/generation/tonal/bass_pitch_class_witness.h"
#include "src/state/material_slot_access.h"

SerialMock Serial;
SDMock SD;

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using GroovePuterMaterial::GeneratedSynthABarOrigin;
using GroovePuterMaterial::GeneratedSynthAOrigin;
using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialSlotDescriptor;

constexpr float kSampleRate = 44100.0f;
const auto kGuard = [](auto&& body) { body(); };

// ---- helpers ---------------------------------------------------------------

void configureScene(MiniAcid& engine, GenerativeMode mode, GenreRecipeId recipe) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(mode);
  scene.genre.recipe = recipe;
  scene.genre.morphTarget = 0;
  scene.genre.morphAmount = 0;
  scene.genre.regenerateOnApply = false;
  scene.genre.applyTempoOnApply = false;
  scene.activeSongSlot = 0;
  scene.songs[0] = Song{};
  scene.songs[1] = Song{};
  scene.feel.patternBars = 1;
  for (int b = 0; b < kBankCount; ++b) {
    for (int p = 0; p < Bank<SynthPattern>::kPatterns; ++p) {
      scene.synthABanks[b].patterns[p] = SynthPattern{};
      scene.synthBBanks[b].patterns[p] = SynthPattern{};
      scene.drumBanks[b].patterns[p] = DrumPatternSet{};
    }
  }
  for (int v = 0; v < Scene::kMaterialVoices; ++v) {
    for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) {
      scene.materialSlots[v][s] = MaterialSlotDescriptor{};
    }
  }
  for (int v = 0; v < NUM_303_VOICES; ++v) {
    engine.pendingMaterial_[v].queued = false;
    engine.pendingMaterial_[v].lifecycleBound = false;
  }
  engine.genreManager().setGenerativeMode(mode);
  if (recipe <= 64) engine.genreManager().setRecipe(recipe);
}

// Active-step topology of a Synth A Pattern. Bass continuations are realized
// as held notes on the following steps, so a plan explains exactly
// onsets | continuations.
uint16_t onsetMask(const SynthPattern& pattern) {
  uint16_t mask = 0;
  for (int i = 0; i < 16; ++i) {
    if (pattern.steps[i].note >= 0) {
      mask = static_cast<uint16_t>(mask | GroovePuterRhythm::stepBit(static_cast<uint8_t>(i)));
    }
  }
  return mask;
}

uint16_t planMask(const GroovePuterRhythm::BassRhythmPlan& plan) {
  return static_cast<uint16_t>(plan.onsets | plan.continuations);
}

bool sameBass(const GroovePuterRhythm::BassRhythmPlan& a,
              const GroovePuterRhythm::BassRhythmPlan& b) {
  return a.id == b.id && a.kickRelationship == b.kickRelationship &&
         a.onsets == b.onsets && a.continuations == b.continuations;
}

bool sameHarmonic(const GroovePuterRhythm::HarmonicRhythmPlan& a,
                  const GroovePuterRhythm::HarmonicRhythmPlan& b) {
  return a.progression == b.progression && a.onsets == b.onsets &&
         a.eventCount == b.eventCount &&
         a.phraseBarOrdinal == b.phraseBarOrdinal &&
         a.phraseHarmonicPosition == b.phraseHarmonicPosition;
}

bool sameSource(const GroovePuterRhythm::ChordProgressionSource& a,
                const GroovePuterRhythm::ChordProgressionSource& b) {
  if (a.id != b.id || a.period != b.period) return false;
  for (int i = 0; i < GroovePuterRhythm::kMaxChordProgressionSourceEvents; ++i) {
    if (a.events[i].degree != b.events[i].degree ||
        a.events[i].quality != b.events[i].quality ||
        a.events[i].rootOffsetSemitones != b.events[i].rootOffsetSemitones) {
      return false;
    }
  }
  return true;
}

bool sameOrigin(const GeneratedSynthAOrigin& a, const GeneratedSynthAOrigin& b) {
  if (a.common.phraseGenerationIdentity != b.common.phraseGenerationIdentity ||
      a.common.barCount != b.common.barCount ||
      a.common.rootPitchClass != b.common.rootPitchClass ||
      a.common.scaleTypeValue != b.common.scaleTypeValue ||
      !sameSource(a.common.progressionSource, b.common.progressionSource)) {
    return false;
  }
  for (int i = 0; i < a.common.barCount; ++i) {
    const auto& x = a.bars[i];
    const auto& y = b.bars[i];
    if (!(x.material.address == y.material.address) ||
        x.material.id != y.material.id ||
        x.originPatternVersion != y.originPatternVersion ||
        !sameBass(x.bassRhythm, y.bassRhythm) ||
        !sameHarmonic(x.harmonicRhythm, y.harmonicRhythm) ||
        x.phraseBarOrdinal != y.phraseBarOrdinal) {
      return false;
    }
  }
  return true;
}

struct Route {
  uint8_t bars;
  GenerativeMode mode;
  GenreRecipeId recipe;
};
// All four supported phrase lengths through P1R routes (see D1-A).
const Route kRoutes[] = {
    {1, GenerativeMode::Chip, 0},
    {2, GenerativeMode::Chip, 0},
    {4, GenerativeMode::LoFi, 0},
    {8, GenerativeMode::LoFi, 0},
};

void freshProject(const char* name) {
  assert(PatternPagingService::setProjectName(name));
  assert(PatternPagingService::clearProjectPages());
}

std::filesystem::path newestIdentityMeta() {
  std::filesystem::path newest;
  std::filesystem::file_time_type newestTime{};
  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           std::filesystem::current_path())) {
    if (!entry.is_regular_file() ||
        entry.path().filename() != "material_id.meta") {
      continue;
    }
    const auto time = entry.last_write_time();
    if (newest.empty() || time >= newestTime) {
      newest = entry.path();
      newestTime = time;
    }
  }
  return newest;
}



namespace Sem = GroovePuterDevelopmentSemantic;
namespace Dev = GroovePuterDevelopment;
using GroovePuterRhythm::StepMask;
using GroovePuterRhythm::stepBit;
using SemStatus = Sem::ClaimEvidenceStatus;

// ---------------------------------------------------------------------------
// Fixture: a real P1R generated Synth A phrase with published origin, bar
// `bar` selected as CURRENT (accepted Pattern).
// ---------------------------------------------------------------------------
struct Fixture {
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  int slot = 0, bank = 0, index = 0;

  Fixture(const char* project, GenerativeMode mode, GenreRecipeId recipe,
          uint8_t bars, int selectBar) {
    freshProject(project);
    engine.init();
    engine.setSongMode(false);
    configureScene(engine, mode, recipe);
    const auto r = GeneratedPhraseSong::generate(engine, bars, 0, kGuard);
    assert(r.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
    assert(r.p1r.usedP1r);
    assert(engine.generatedSynthAOrigin() != nullptr);
    slot = r.phrase.firstLocalSlot + selectBar;
    bank = slot / Bank<SynthPattern>::kPatterns;
    index = slot % Bank<SynthPattern>::kPatterns;
    engine.set303BankIndex(0, bank);
    engine.set303PatternIndex(0, index);
    assert(engine.captureCurrentPreparationBasis(0).valid());
  }
  Scene& scene() { return engine.sceneManager().currentScene(); }
  SynthPattern& current() { return scene().synthABanks[bank].patterns[index]; }
  const GeneratedSynthABarOrigin& origin() {
    const auto* o = engine.findGeneratedSynthAOrigin(engine.captureCurrentPreparationBasis(0).reference);
    assert(o != nullptr);
    return *o;
  }
  Sem::DevelopmentSemanticObservation develop(Dev::TransformationKind kind, int8_t octave = 0,
                                              uint16_t displace = 12) {
    Dev::DevelopmentRequest req{};
    req.transformation = kind;
    req.octaveShift = octave;
    req.displaceTicks = displace;
    req.genreId = static_cast<uint8_t>(GenerativeMode::Acid);
    Dev::DevelopmentResult result{};
    Sem::DevelopmentSemanticObservation obs{};
    (void)engine.cancelNextMaterial(0);
    lastPrepare = engine.developWorkingMaterial(0, req, &result, &obs);
    lastResult = result;
    return obs;
  }
  MiniAcid::NextPrepareResult lastPrepare{};
  Dev::DevelopmentResult lastResult{};
};

void expectNeverNewIdea(const Sem::DevelopmentSemanticObservation& obs) {
  assert(obs.facts.lineage != Sem::LineageStatus::NewIdea);
  assert(obs.preservation.lineageSummary != SemStatus::Fail);
}

void expectContinues(const Sem::DevelopmentSemanticObservation& obs) {
  assert(obs.available);
  assert(obs.preservation.r1BarExtent == SemStatus::Pass);
  assert(obs.preservation.r2BassOnsetTopology == SemStatus::Pass);
  assert(obs.preservation.r3PitchClassAtOnset == SemStatus::Pass);
  assert(obs.preservation.lineageSummary == SemStatus::Pass);
  assert(obs.facts.lineage == Sem::LineageStatus::Continues);
  assert(Sem::capabilityFor(obs.facts, Sem::CapabilityClaim::RhythmTopology) ==
         Sem::CapabilityStatus::Available);
  expectNeverNewIdea(obs);
}

void expectUnknownLineage(const Sem::DevelopmentSemanticObservation& obs) {
  assert(obs.facts.lineage == Sem::LineageStatus::Unknown);
  assert(obs.preservation.lineageSummary != SemStatus::Pass);
  expectNeverNewIdea(obs);
}

// Physical step of the first / a rest step of the CURRENT pattern.
int firstNoteStep(const SynthPattern& p) {
  for (int i = 0; i < 16; ++i) if (p.steps[i].note >= 0) return i;
  return -1;
}
int firstRestStep(const SynthPattern& p) {
  for (int i = 0; i < 16; ++i) if (p.steps[i].note < 0) return i;
  return -1;
}
int lastNoteStep(const SynthPattern& p) {
  for (int i = 15; i >= 0; --i) if (p.steps[i].note >= 0) return i;
  return -1;
}


// M0-A PRODUCT FIREWALL (runtime): semantic facts are observations, not policy.
//  1. semantic UNKNOWN does NOT prevent generation, development or NEXT.
//  2. semantic CONTINUES does NOT mean the candidate is musically good.
void test_unknown_never_blocks() {
  Fixture f("m0a-unknown", GenerativeMode::Acid, 0, 2, 0);
  size_t unknownCases = 0;
  for (auto kind : {Dev::TransformationKind::Displace, Dev::TransformationKind::Thin}) {
    auto obs = f.develop(kind);
    if (!f.lastResult.success) continue;  // the transform itself may refuse; then nothing to compare
    assert(obs.available);
    assert(obs.facts.lineage == Sem::LineageStatus::Unknown);      // semantic UNKNOWN...
    assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared); // ...never blocks NEXT
    assert(f.engine.pendingMaterial_[0].queued);
    assert(f.engine.pendingMaterial_[0].lifecycleBound);
    ++unknownCases;
  }
  assert(unknownCases > 0);

  // No origin at all -> UNKNOWN as well, still fully developable.
  f.engine.clearGeneratedSynthAOrigin();
  auto none = f.develop(Dev::TransformationKind::Revoice, 1);
  assert(!none.available && none.facts.lineage == Sem::LineageStatus::Unknown);
  assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared);
  assert(f.engine.pendingMaterial_[0].queued);

  // ...and generation itself never consults semantic state: a fresh phrase is
  // generated (and republishes origin evidence) after the sidecar was cleared.
  configureScene(f.engine, GenerativeMode::Acid, 0);
  const auto again = GeneratedPhraseSong::generate(f.engine, 2, 0, kGuard);
  assert(again.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  std::puts("M0-A firewall: semantic UNKNOWN never blocks generate/develop/NEXT: PASS");
}

void test_continues_is_not_quality() {
  Fixture f("m0a-continues", GenerativeMode::Acid, 0, 2, 0);
  PhraseRuntime::RuntimeSynthEventBuffer source{};
  uint8_t steps[16];
  assert(f.engine.acquireWorkingMelodySourceImpl_(0, source, &steps));

  // Same attacks, same pitch classes -- but four octaves up (>= MIDI 84): the
  // evaluator proves continuity, and says NOTHING about whether it sounds good.
  auto shrill = source;
  int minNote = 127;
  for (uint16_t i = 0; i < shrill.count; ++i) {
    shrill.events[i].note = static_cast<uint8_t>(shrill.events[i].note + 48 > 127 ? shrill.events[i].note % 12 + 108
                                                                                  : shrill.events[i].note + 48);
    minNote = std::min<int>(minNote, shrill.events[i].note);
  }
  const auto e = Sem::evaluateP0Preservation(f.origin(), source, steps, shrill);
  assert(e.lineageSummary == SemStatus::Pass);  // CONTINUES...
  assert(minNote >= 60);                          // ...for a line far outside a bass register
  Dev::DevelopmentEvidence ev{};
  Dev::DevelopmentRequest req{};
  const auto facts = Sem::adaptP0Preservation(source, shrill, ev, req, e);
  assert(facts.lineage == Sem::LineageStatus::Continues);
  // CONTINUES grants no GENRE / OPERATION verdict either.
  assert(facts.genre == Sem::GenreStatus::Unknown);
  assert(facts.operation == Sem::OperationConformance::Unknown);
  std::puts("M0-A firewall: semantic CONTINUES makes no quality/genre claim: PASS");
}

// M0-A audit MEASUREMENT (descriptive; asserts only "the operation ran"): what do
// the two real front-panel keys do to a generated bass bar? 'D' (labelled
// DEVELOP) issues Revoice; 'V' (VARY) issues Connect, both with the UI's default
// request (octaveShift=0, degreeShift=2).
void test_measure_front_panel_keys() {
  struct Case { const char* name; GenerativeMode mode; GenreRecipeId recipe; uint8_t bars; };
  for (const Case& c : {Case{"Acid/0", GenerativeMode::Acid, 0, 2}, Case{"UKG/0", GenerativeMode::UkGarage, 0, 4},
                        Case{"DnB/0", GenerativeMode::DrumAndBass, 0, 4}, Case{"Dub/5", GenerativeMode::Reggae, 5, 4}}) {
    Fixture f("m0a-keys", c.mode, c.recipe, c.bars, 0);
    PhraseRuntime::RuntimeSynthEventBuffer source{};
    uint8_t steps[16];
    assert(f.engine.acquireWorkingMelodySourceImpl_(0, source, &steps));
    for (auto kind : {Dev::TransformationKind::Revoice, Dev::TransformationKind::Connect}) {
      auto obs = f.develop(kind, 0);
      const auto& cand = f.lastResult.candidate;
      int pcChanged = 0, octChanged = 0, tickChanged = 0;
      const uint16_t n = std::min<uint16_t>(cand.count, source.count);
      for (uint16_t i = 0; i < n; ++i) {
        pcChanged += (cand.events[i].note % 12) != (source.events[i].note % 12);
        octChanged += (cand.events[i].note / 12) != (source.events[i].note / 12);
        tickChanged += cand.events[i].startTick != source.events[i].startTick;
      }
      std::printf("  KEY %-7s %-8s success=%d count %u->%u  pitchClassChanged=%d octaveChanged=%d tickChanged=%d  "
                  "R1/R2/R3=%d/%d/%d lineage=%s\n",
                  c.name, kind == Dev::TransformationKind::Revoice ? "D(Revoice)" : "V(Connect)", f.lastResult.success,
                  source.count, cand.count, pcChanged, octChanged, tickChanged,
                  int(obs.preservation.r1BarExtent), int(obs.preservation.r2BassOnsetTopology),
                  int(obs.preservation.r3PitchClassAtOnset),
                  obs.facts.lineage == Sem::LineageStatus::Continues ? "CONTINUES" : "UNKNOWN");
    }
  }
  std::puts("M0-A audit: front-panel D/V key measurement recorded: PASS");
}

}  // namespace

int main() {
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");
  test_unknown_never_blocks();
  test_continues_is_not_quality();
  test_measure_front_panel_keys();
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");
  std::filesystem::remove("grooveputer_scene_name.txt");
  std::puts("0.9.14 M0-A product firewall: ALL PASS");
  return 0;
}
