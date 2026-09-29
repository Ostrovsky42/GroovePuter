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


// ---------------------------------------------------------------------------
// D1-C1: attacks vs continuations. A continuation is realized by the owner as
// an extra physical Pattern step (active note, slide set); it is a
// representation fact and never a new attack.
// ---------------------------------------------------------------------------
std::unique_ptr<Fixture> makeContinuationFixture() {
  std::unique_ptr<Fixture> fixture;
  int bar = -1;
  for (int attempt = 0; attempt < 24 && bar < 0; ++attempt) {
    fixture.reset(new Fixture("d1c1-cont", GenerativeMode::Reggae, 3, 8, 0));
    const GeneratedSynthAOrigin* o = fixture->engine.generatedSynthAOrigin();
    for (int i = 0; i < 8; ++i) if (o->bars[i].bassRhythm.continuations != 0) { bar = i; break; }
  }
  assert(bar >= 0);  // meaningful only if a bar carries continuations
  Fixture& f = *fixture;
  const int gs = f.engine.generatedSynthAOrigin()->bars[bar].material.address.globalSlot;
  f.bank = gs / Bank<SynthPattern>::kPatterns;
  f.index = gs % Bank<SynthPattern>::kPatterns;
  f.engine.set303BankIndex(0, f.bank);
  f.engine.set303PatternIndex(0, f.index);
  assert(f.engine.captureCurrentPreparationBasis(0).valid());
  assert(f.origin().bassRhythm.continuations != 0);
  assert(f.origin().bassRhythm.onsets != 0);
  return fixture;
}

int firstStepIn(StepMask mask) {
  for (int st = 0; st < 16; ++st) if (mask & stepBit(static_cast<uint8_t>(st))) return st;
  return -1;
}
int ordinalOfStep(const uint8_t (&steps)[16], uint16_t count, int step) {
  for (uint16_t i = 0; i < count; ++i) if (steps[i] == step) return i;
  return -1;
}

struct Acquired {
  PhraseRuntime::RuntimeSynthEventBuffer source{};
  uint8_t steps[16];
};
Acquired acquire(Fixture& f) {
  Acquired a;
  assert(f.engine.acquireWorkingMelodySourceImpl_(0, a.source, &a.steps));
  return a;
}

void test_d1c1_witnesses() {
  auto fx = makeContinuationFixture();
  Fixture& f = *fx;
  const GeneratedSynthABarOrigin& origin = f.origin();
  const StepMask attacks = origin.bassRhythm.onsets;
  const StepMask conts = origin.bassRhythm.continuations;
  const int contStep = firstStepIn(conts);
  const int attackStep = firstStepIn(attacks);
  std::printf("  D1-C1 fixture bar: attacks=%04x continuations=%04x\n", attacks, conts);

  // A. untouched generated bar with continuations -> CONTINUES (pure + engine).
  {
    auto a = acquire(f);
    assert(a.source.count == static_cast<uint16_t>(__builtin_popcount(attacks | conts)));
    const auto e = Sem::evaluateP0Preservation(origin, a.source, a.steps, a.source);
    assert(e.r1BarExtent == SemStatus::Pass && e.r2BassOnsetTopology == SemStatus::Pass &&
           e.r3PitchClassAtOnset == SemStatus::Pass && e.lineageSummary == SemStatus::Pass);
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared);
    expectContinues(obs);
  }

  // B. continuation REPRESENTATION changes; attacks + attack pitch classes intact.
  {
    auto a = acquire(f);
    // B1: candidate changes only continuation events (tick, duration, flags).
    auto cand = a.source;
    for (uint16_t i = 0; i < cand.count; ++i) {
      if (conts & stepBit(a.steps[i])) {
        cand.events[i].durationSubticks = static_cast<uint16_t>(cand.events[i].durationSubticks + 7);
        cand.events[i].startTick = static_cast<uint16_t>(cand.events[i].startTick + 3);
        cand.events[i].flags = static_cast<uint8_t>(cand.events[i].flags ^ 1u);
      }
    }
    const auto e = Sem::evaluateP0Preservation(origin, a.source, a.steps, cand);
    assert(e.r2BassOnsetTopology == SemStatus::Pass);
    assert(e.r3PitchClassAtOnset == SemStatus::Pass);
    assert(e.lineageSummary == SemStatus::Pass);

    // B2: CURRENT loses a continuation step entirely (representation change only).
    SynthPattern saved = f.current();
    f.current().steps[contStep].note = -1;
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(f.lastResult.success);
    assert(obs.available);
    assert(obs.preservation.r2BassOnsetTopology == SemStatus::Pass);
    assert(obs.preservation.r3PitchClassAtOnset == SemStatus::Pass);
    assert(obs.facts.lineage == Sem::LineageStatus::Continues);
    f.current() = saved;

    // B3: CURRENT continuation slide flag toggled.
    f.current().steps[contStep].slide = !f.current().steps[contStep].slide;
    obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(obs.preservation.r2BassOnsetTopology == SemStatus::Pass);
    assert(obs.facts.lineage == Sem::LineageStatus::Continues);
    f.current() = saved;
  }

  // C. moved attack -> R2 FAIL (candidate-level and CURRENT-level).
  {
    auto a = acquire(f);
    auto cand = a.source;
    const int o = ordinalOfStep(a.steps, a.source.count, attackStep);
    assert(o >= 0);
    cand.events[o].startTick = static_cast<uint16_t>(cand.events[o].startTick + 12);
    const auto e = Sem::evaluateP0Preservation(origin, a.source, a.steps, cand);
    assert(e.r2BassOnsetTopology == SemStatus::Fail);
    assert(e.r3PitchClassAtOnset == SemStatus::Unknown);
    assert(e.lineageSummary == SemStatus::Unknown);

    SynthPattern saved = f.current();
    const int rest = firstRestStep(f.current());
    assert(rest >= 0);
    f.current().steps[rest] = f.current().steps[attackStep];
    f.current().steps[attackStep] = SynthStep{};
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(obs.available && obs.preservation.r2BassOnsetTopology == SemStatus::Fail);
    expectUnknownLineage(obs);
    f.current() = saved;
  }

  // D. missing attack (directly provable from CURRENT) -> R2 FAIL.
  {
    SynthPattern saved = f.current();
    f.current().steps[attackStep].note = -1;
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(obs.available && obs.preservation.r2BassOnsetTopology == SemStatus::Fail);
    expectUnknownLineage(obs);
    f.current() = saved;
  }

  // E. candidate correspondence lost -> UNKNOWN (not an invented FAIL).
  {
    auto a = acquire(f);
    auto cand = a.source;
    cand.count = static_cast<uint16_t>(cand.count - 1);
    const auto e = Sem::evaluateP0Preservation(origin, a.source, a.steps, cand);
    assert(e.r2BassOnsetTopology == SemStatus::Unknown);
    assert(e.r3PitchClassAtOnset == SemStatus::Unknown);
    assert(e.lineageSummary == SemStatus::Unknown);
    // Real THIN (count changes) behaves the same.
    auto obs = f.develop(Dev::TransformationKind::Thin);
    if (f.lastResult.success && f.lastResult.candidate.count != a.source.count) {
      assert(obs.preservation.r2BassOnsetTopology == SemStatus::Unknown);
      expectUnknownLineage(obs);
    }
  }

  // F. chromatic change on a CONTINUATION only is not an attack failure.
  {
    auto a = acquire(f);
    auto cand = a.source;
    const int o = ordinalOfStep(a.steps, a.source.count, contStep);
    assert(o >= 0);
    cand.events[o].note = static_cast<uint8_t>(cand.events[o].note + 1);
    const auto e = Sem::evaluateP0Preservation(origin, a.source, a.steps, cand);
    assert(e.r2BassOnsetTopology == SemStatus::Pass);
    assert(e.r3PitchClassAtOnset == SemStatus::Pass);
    assert(e.lineageSummary == SemStatus::Pass);
    SynthPattern saved = f.current();
    f.current().steps[contStep].note = static_cast<int8_t>(f.current().steps[contStep].note + 1);
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(obs.preservation.r3PitchClassAtOnset == SemStatus::Pass);
    assert(obs.facts.lineage == Sem::LineageStatus::Continues);
    f.current() = saved;
  }

  // G. chromatic change on an actual ATTACK -> R3 FAIL (candidate + CURRENT).
  {
    auto a = acquire(f);
    auto cand = a.source;
    const int o = ordinalOfStep(a.steps, a.source.count, attackStep);
    cand.events[o].note = static_cast<uint8_t>(cand.events[o].note + 1);
    const auto e = Sem::evaluateP0Preservation(origin, a.source, a.steps, cand);
    assert(e.r2BassOnsetTopology == SemStatus::Pass);
    assert(e.r3PitchClassAtOnset == SemStatus::Fail);
    assert(e.lineageSummary == SemStatus::Unknown);
    SynthPattern saved = f.current();
    f.current().steps[attackStep].note = static_cast<int8_t>(f.current().steps[attackStep].note + 1);
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(obs.preservation.r2BassOnsetTopology == SemStatus::Pass);
    assert(obs.preservation.r3PitchClassAtOnset == SemStatus::Fail);
    expectUnknownLineage(obs);
    f.current() = saved;
  }

  // Unclassifiable extra event (neither origin attack nor known continuation):
  // meaning is not invented -> R2 UNKNOWN.
  {
    SynthPattern saved = f.current();
    const int rest = firstRestStep(f.current());
    f.current().steps[rest].note = 60;
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(obs.preservation.r2BassOnsetTopology == SemStatus::Unknown);
    expectUnknownLineage(obs);
    f.current() = saved;
  }
  std::puts("D1-C1 A-G witnesses (attack vs continuation): PASS");
}

// H. No path emits NEW_IDEA / Fail summary: single-event mutations of tick,
// pitch and duration on attack AND continuation ordinals, plus count loss.
void test_d1c1_never_new_idea() {
  auto fx = makeContinuationFixture();
  Fixture& f = *fx;
  auto a = acquire(f);
  size_t cases = 0;
  for (uint16_t i = 0; i < a.source.count; ++i) {
    for (int dn : {-1, 0, 1, 12, -12}) {
      for (int dt : {-12, 0, 12}) {
        auto c = a.source;
        c.events[i].note = static_cast<uint8_t>(std::max(0, std::min(127, c.events[i].note + dn)));
        c.events[i].startTick = static_cast<uint16_t>(std::max(0, c.events[i].startTick + dt));
        const auto e = Sem::evaluateP0Preservation(f.origin(), a.source, a.steps, c);
        Dev::DevelopmentEvidence ev{};
        Dev::DevelopmentRequest req{};
        const auto facts = Sem::adaptP0Preservation(a.source, c, ev, req, e);
        assert(e.lineageSummary != SemStatus::Fail);
        assert(facts.lineage != Sem::LineageStatus::NewIdea);
        const bool pass = e.r1BarExtent == SemStatus::Pass && e.r2BassOnsetTopology == SemStatus::Pass &&
                          e.r3PitchClassAtOnset == SemStatus::Pass;
        assert((facts.lineage == Sem::LineageStatus::Continues) == pass);
        ++cases;
      }
    }
    auto shrunk = a.source;
    shrunk.count = static_cast<uint16_t>(i);
    const auto e = Sem::evaluateP0Preservation(f.origin(), a.source, a.steps, shrunk);
    assert(e.lineageSummary != SemStatus::Fail && e.r2BassOnsetTopology != SemStatus::Pass);
  }
  std::printf("  D1-C1 NEW_IDEA sweep cases=%zu\n", cases);
  std::puts("D1-C1 H no NEW_IDEA / no Fail summary anywhere: PASS");
}

struct PublicationSnapshot {
  MiniAcid::NextPrepareResult result{};
  Dev::DevelopmentResult dev{};
  bool queued = false, lifecycleBound = false;
  GroovePuterMaterial::IdeaClassification idea{};
  MaterialReference preparedFor{};
  MaterialKind basisKind{};
  GroovePuterMaterial::MaterialVersionToken acceptedVersion{};
  uint32_t generation = 0;
  bool goQueued = false, hasAnchor = false;
  PhraseRuntime::RuntimeSynthEventBuffer pendingMelody{};
  PhraseRuntime::RuntimeSynthEventBuffer anchor{};
  GroovePuterMaterial::DevelopmentLineage lineage{};
};

PublicationSnapshot snap(MiniAcid& e, MiniAcid::NextPrepareResult r, const Dev::DevelopmentResult& dev) {
  PublicationSnapshot s;
  s.result = r;
  s.dev = dev;
  const auto& p = e.pendingMaterial_[0];
  s.queued = p.queued; s.lifecycleBound = p.lifecycleBound; s.idea = p.ideaClassification;
  s.preparedFor = p.preparedFor; s.basisKind = p.basisKind; s.acceptedVersion = p.acceptedVersion;
  s.generation = e.pendingGeneration_[0]; s.goQueued = e.goQueued_[0];
  s.hasAnchor = e.hasSourceAnchorSnapshot_[0];
  if (p.melody != nullptr) s.pendingMelody = *p.melody;
  s.anchor = e.sourceAnchorSnapshot_[0];
  s.lineage = e.developmentLineage_[0];
  return s;
}

bool sameSnap(const PublicationSnapshot& a, const PublicationSnapshot& b) {
  return a.result == b.result && a.dev.success == b.dev.success &&
         RuntimePhraseEdit::same(a.dev.candidate, b.dev.candidate) &&
         a.dev.classification.idea == b.dev.classification.idea &&
         a.dev.classification.genre == b.dev.classification.genre &&
         a.dev.disposition == b.dev.disposition && a.queued == b.queued &&
         a.lifecycleBound == b.lifecycleBound && a.idea == b.idea &&
         a.preparedFor.id == b.preparedFor.id && a.preparedFor.address == b.preparedFor.address &&
         a.basisKind == b.basisKind && a.acceptedVersion == b.acceptedVersion &&
         a.goQueued == b.goQueued && a.hasAnchor == b.hasAnchor &&
         RuntimePhraseEdit::same(a.pendingMelody, b.pendingMelody) &&
         RuntimePhraseEdit::same(a.anchor, b.anchor) &&
         a.lineage.predecessorBasis == b.lineage.predecessorBasis &&
         a.lineage.sourceAnchorBasis == b.lineage.sourceAnchorBasis;
}


// I. Publication remains byte/functionally neutral on a continuation bar.
void test_d1c1_publication_neutral() {
  auto fx = makeContinuationFixture();
  Fixture& f = *fx;
  for (auto kind : {Dev::TransformationKind::Revoice, Dev::TransformationKind::Displace,
                    Dev::TransformationKind::Thin, Dev::TransformationKind::Hold,
                    Dev::TransformationKind::Connect}) {
    Dev::DevelopmentRequest req{};
    req.transformation = kind;
    req.octaveShift = 1;
    req.genreId = static_cast<uint8_t>(GenerativeMode::Acid);
    Dev::DevelopmentResult warm{};
    (void)f.engine.cancelNextMaterial(0);
    (void)f.engine.developWorkingMaterial(0, req, &warm);
    Dev::DevelopmentResult plain{}, observed{};
    (void)f.engine.cancelNextMaterial(0);
    const auto r1 = f.engine.developWorkingMaterial(0, req, &plain);
    const auto s1 = snap(f.engine, r1, plain);
    Sem::DevelopmentSemanticObservation obs{};
    (void)f.engine.cancelNextMaterial(0);
    const auto r2 = f.engine.developWorkingMaterial(0, req, &observed, &obs);
    const auto s2 = snap(f.engine, r2, observed);
    assert(sameSnap(s1, s2));
  }
  std::puts("D1-C1 I publication neutrality on continuation bar: PASS");
}

}  // namespace

int main() {
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");
  test_d1c1_witnesses();
  test_d1c1_never_new_idea();
  test_d1c1_publication_neutral();
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");
  std::filesystem::remove("grooveputer_scene_name.txt");
  std::puts("0.9.14 D1-C1 attack/continuation alignment: ALL PASS");
  return 0;
}
