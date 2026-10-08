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

// ---- A..E : positive matrix -------------------------------------------------
void test_positive_matrix() {
  {  // A: exact / no-op. No-op candidate: use the pure evaluator on the real source.
    Fixture f("d1c-a", GenerativeMode::Acid, 0, 2, 0);
    PhraseRuntime::RuntimeSynthEventBuffer source{};
    uint8_t steps[16];
    assert(f.engine.acquireWorkingMelodySourceImpl_(0, source, &steps));
    const auto a = Sem::evaluateP0Preservation(f.origin(), source, steps, source);
    assert(a.available);
    assert(a.r1BarExtent == SemStatus::Pass && a.r2BassOnsetTopology == SemStatus::Pass &&
           a.r3PitchClassAtOnset == SemStatus::Pass && a.lineageSummary == SemStatus::Pass);
    assert(a.predecessorRelation == Sem::StateRelation::Exact);
    Dev::DevelopmentEvidence ev{};
    Dev::DevelopmentRequest req{};
    const auto facts = Sem::adaptP0Preservation(source, source, ev, req, a);
    assert(facts.lineage == Sem::LineageStatus::Continues);
    assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::Predecessor) == Sem::StateRelation::Exact);
    assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::Source) == Sem::StateRelation::Unknown);
    assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::ReturnTarget) == Sem::StateRelation::NotApplicable);
    assert(facts.genre == Sem::GenreStatus::Unknown);
    assert(facts.trajectory == Sem::TrajectoryRole::Unknown);
  }
  {  // B: REVOICE octave.  C: HOLD.  D: CONNECT.  E: octave MOVE.
    Fixture f("d1c-bcde", GenerativeMode::Acid, 0, 2, 0);
    auto revoice = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared);
    expectContinues(revoice);
    assert(revoice.preservation.predecessorRelation == Sem::StateRelation::Variation);
    assert(Sem::stateRelationFor(revoice.facts, Sem::ReferenceRole::Predecessor) ==
           Sem::StateRelation::Variation);
    assert(Sem::stateRelationFor(revoice.facts, Sem::ReferenceRole::Source) ==
           Sem::StateRelation::Unknown);

    auto hold = f.develop(Dev::TransformationKind::Hold);
    if (f.lastResult.success) expectContinues(hold);
    auto connect = f.develop(Dev::TransformationKind::Connect);
    if (f.lastResult.success) expectContinues(connect);
    auto move = f.develop(Dev::TransformationKind::Move, 1);
    assert(f.lastResult.success);
    expectContinues(move);
    std::printf("  positive ops: revoice=Variation hold(success=%d) connect(success=%d) move=CONTINUES\n",
                (int)hold.available, (int)connect.available);
  }
}

// ---- F, G : R2 failures ---------------------------------------------------
void test_displace_and_thin() {
  Fixture f("d1c-fg", GenerativeMode::Acid, 0, 2, 0);
  auto displace = f.develop(Dev::TransformationKind::Displace, 0, 12);
  assert(f.lastResult.success);
  assert(displace.available);
  assert(displace.preservation.r2BassOnsetTopology == SemStatus::Fail);
  assert(displace.preservation.r3PitchClassAtOnset == SemStatus::Unknown);
  expectUnknownLineage(displace);
  assert(Sem::stateRelationFor(displace.facts, Sem::ReferenceRole::Predecessor) ==
         Sem::StateRelation::Unknown);

  auto thin = f.develop(Dev::TransformationKind::Thin);
  assert(f.lastResult.success);
  assert(thin.available);
  // D1-C1: THIN changes the event count, so no attack correspondence exists ->
  // R2 is UNKNOWN (never an invented FAIL).
  assert(thin.preservation.r2BassOnsetTopology == SemStatus::Unknown);
  assert(thin.preservation.r3PitchClassAtOnset == SemStatus::Unknown);
  expectUnknownLineage(thin);
}

// ---- H : chromatic CANDIDATE pitch change (pure evaluator on the real source) --
void test_chromatic_candidate() {
  Fixture f("d1c-h", GenerativeMode::Acid, 0, 2, 0);
  PhraseRuntime::RuntimeSynthEventBuffer source{};
  uint8_t steps[16];
  assert(f.engine.acquireWorkingMelodySourceImpl_(0, source, &steps));
  auto candidate = source;
  // The second runtime event may be a continuation. R3 compares pitch at
  // authoritative bass attacks, so mutate an actual attack after the anchor.
  uint16_t attack = source.count;
  for (uint16_t i = 1; i < source.count; ++i) {
    if ((f.origin().bassRhythm.onsets & stepBit(steps[i])) == 0) continue;
    attack = i;
    break;
  }
  assert(attack < source.count);
  candidate.events[attack].note = static_cast<uint8_t>(candidate.events[attack].note + 1);
  const auto a = Sem::evaluateP0Preservation(f.origin(), source, steps, candidate);
  assert(a.r2BassOnsetTopology == SemStatus::Pass);
  assert(a.r3PitchClassAtOnset == SemStatus::Fail);
  assert(a.lineageSummary == SemStatus::Unknown);
  Dev::DevelopmentEvidence ev{};
  Dev::DevelopmentRequest req{};
  const auto facts = Sem::adaptP0Preservation(source, candidate, ev, req, a);
  assert(facts.lineage == Sem::LineageStatus::Unknown);
  assert(facts.lineage != Sem::LineageStatus::NewIdea);
  // R1 alone: growth beyond one bar -> R1 Fail, lineage Unknown.
  auto grown = source;
  grown.lengthTicks = static_cast<uint16_t>(PhraseRuntime::kTicksPerBar * 2u);
  const auto g = Sem::evaluateP0Preservation(f.origin(), source, steps, grown);
  assert(g.r1BarExtent == SemStatus::Fail && g.lineageSummary == SemStatus::Unknown);
}

// ---- I, J, K, L + version adversarial (23) ----------------------------------
void test_current_edits_and_version_witness() {
  {  // I: manual octave edit before D. Version differs, R2/R3 still PASS.
    Fixture f("d1c-i", GenerativeMode::Acid, 0, 2, 0);
    const auto before = f.engine.captureCurrentPreparationBasis(0).version;
    const int step = firstNoteStep(f.current());
    f.current().steps[step].note = static_cast<int8_t>(f.current().steps[step].note + 12);
    assert(f.engine.captureCurrentPreparationBasis(0).version != before);
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared);
    assert(obs.currentChangedSinceOrigin);
    expectContinues(obs);
  }
  {  // J: manual chromatic edit -> R2 PASS, R3 FAIL, lineage UNKNOWN.
    Fixture f("d1c-j", GenerativeMode::Acid, 0, 2, 0);
    const int step = firstNoteStep(f.current());
    f.current().steps[step].note = static_cast<int8_t>(f.current().steps[step].note + 1);
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(obs.available);
    assert(obs.preservation.r2BassOnsetTopology == SemStatus::Pass);
    assert(obs.preservation.r3PitchClassAtOnset == SemStatus::Fail);
    expectUnknownLineage(obs);
  }
  {  // K: onset add -> UNKNOWN (unclassifiable event); attack remove/move -> R2 FAIL.
    for (int variant = 0; variant < 3; ++variant) {
      Fixture f("d1c-k", GenerativeMode::Acid, 0, 2, 0);
      SynthPattern& p = f.current();
      const StepMask attacks = f.origin().bassRhythm.onsets;
      int attackStep = -1;
      for (int st = 15; st >= 0; --st) if (attacks & stepBit(static_cast<uint8_t>(st))) { attackStep = st; break; }
      assert(attackStep >= 0);
      if (variant == 0) {  // add an event on a rest step (meaning unknown)
        const int rest = firstRestStep(p);
        assert(rest >= 0);
        p.steps[rest].note = 60;
      } else if (variant == 1) {  // remove an ATTACK
        p.steps[attackStep].note = -1;
      } else {  // move an ATTACK to a rest step
        const int to = firstRestStep(p);
        p.steps[to] = p.steps[attackStep];
        p.steps[attackStep] = SynthStep{};
      }
      auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
      assert(f.lastResult.success);
      assert(obs.available);
      if (variant == 0) assert(obs.preservation.r2BassOnsetTopology == SemStatus::Unknown);
      else assert(obs.preservation.r2BassOnsetTopology == SemStatus::Fail);
      assert(obs.preservation.r3PitchClassAtOnset == SemStatus::Unknown);
      expectUnknownLineage(obs);
    }
  }
  {  // L + version witness (23): velocity / FX / accent / slide-only edits.
    Fixture f("d1c-l", GenerativeMode::Acid, 0, 2, 0);
    const auto originVersion = f.origin().originPatternVersion;
    SynthPattern& p = f.current();
    const int step = firstNoteStep(p);
    p.steps[step].velocity = static_cast<uint8_t>(p.steps[step].velocity > 60 ? 40 : 120);
    p.steps[step].fx = 3;
    p.steps[step].fxParam = 9;
    p.steps[step].accent = !p.steps[step].accent;
    p.steps[lastNoteStep(p)].slide = !p.steps[lastNoteStep(p)].slide;
    const auto now = f.engine.captureCurrentPreparationBasis(0).version;
    assert(now != originVersion);  // exact version differs from origin
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared);
    assert(obs.currentChangedSinceOrigin);
    expectContinues(obs);  // ...yet all claims PASS => claim-local applicability
  }
}

// ---- M, N, O, P : unavailable scopes ------------------------------------------
void test_unavailable_scopes() {
  {  // M: missing origin.
    Fixture f("d1c-m", GenerativeMode::Acid, 0, 2, 0);
    f.engine.clearGeneratedSynthAOrigin();
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared);  // publication unaffected
    assert(!obs.available);
    expectUnknownLineage(obs);
  }
  {  // N: Synth B.
    Fixture f("d1c-n", GenerativeMode::Acid, 0, 2, 0);
    f.scene().materialSlots[1][f.slot] =
        MaterialSlotDescriptor{MaterialKind::Pattern, MaterialId{9001}};
    f.scene().synthBBanks[f.bank].patterns[f.index] = f.current();
    f.engine.set303BankIndex(1, f.bank);
    f.engine.set303PatternIndex(1, f.index);
    Dev::DevelopmentRequest req{};
    req.transformation = Dev::TransformationKind::Revoice;
    req.octaveShift = 1;
    Dev::DevelopmentResult result{};
    Sem::DevelopmentSemanticObservation obs{};
    (void)f.engine.developWorkingMaterial(1, req, &result, &obs);
    assert(!obs.available);
    expectUnknownLineage(obs);
  }
  {  // O: Legacy generated Pattern (no P1R origin).
    freshProject("d1c-o");
    SceneStorageSdl storage;
    MiniAcid engine{kSampleRate, &storage};
    engine.init();
    engine.setSongMode(false);
    configureScene(engine, GenerativeMode::Techno, 250);
    const auto r = GeneratedPhraseSong::generate(engine, 1, 0, kGuard);
    assert(r.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow && !r.p1r.usedP1r);
    engine.set303BankIndex(0, r.phrase.firstLocalSlot / Bank<SynthPattern>::kPatterns);
    engine.set303PatternIndex(0, r.phrase.firstLocalSlot % Bank<SynthPattern>::kPatterns);
    Dev::DevelopmentRequest req{};
    req.transformation = Dev::TransformationKind::Revoice;
    req.octaveShift = 1;
    Dev::DevelopmentResult result{};
    Sem::DevelopmentSemanticObservation obs{};
    (void)engine.developWorkingMaterial(0, req, &result, &obs);
    assert(!obs.available);
    expectUnknownLineage(obs);
  }
  {  // P: CURRENT Melody after a NEXT activation.
    Fixture f("d1c-p", GenerativeMode::Acid, 0, 2, 0);
    auto first = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(first.available);
    assert(f.engine.activateNextMaterialAtBoundary(0) ==
           MiniAcid::NextActivationResult::Activated);
    assert(f.engine.workingMaterial_[0].holdsMelody());
    auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
    assert(!obs.available);
    expectUnknownLineage(obs);
    assert(obs.facts.lineage != Sem::LineageStatus::NewIdea);
  }
}

// ---- Source-map adversarial (24): non-straight timing -----------------------
void test_swing_source_map() {
  // Swing only delays ODD steps. Generation varies with the attempt ordinal, so
  // regenerate (bounded) until a bar with an odd-step bass attack exists.
  std::unique_ptr<Fixture> fixture;
  int swingBar = -1;
  for (int attempt = 0; attempt < 24 && swingBar < 0; ++attempt) {
    fixture.reset(new Fixture("d1c-swing", GenerativeMode::Acid, 0, 4, 0));
    const GeneratedSynthAOrigin* o = fixture->engine.generatedSynthAOrigin();
    for (int b = 0; b < 4 && swingBar < 0; ++b)
      for (uint8_t st = 1; st < 16; st += 2)
        if (o->bars[b].bassRhythm.onsets & stepBit(st)) { swingBar = b; break; }
  }
  assert(swingBar >= 0);
  Fixture& f = *fixture;
  {
    const GeneratedSynthAOrigin* o = f.engine.generatedSynthAOrigin();
    const int gs = o->bars[swingBar].material.address.globalSlot;
    f.bank = gs / Bank<SynthPattern>::kPatterns;
    f.index = gs % Bank<SynthPattern>::kPatterns;
    f.engine.set303BankIndex(0, f.bank);
    f.engine.set303PatternIndex(0, f.index);
    assert(f.engine.captureCurrentPreparationBasis(0).valid());
  }
  f.scene().feel.swingPct = 62;
  f.scene().feel.swingMask = 0xFFFF;
  // Micro-timing pulls one attack earlier than its step boundary (does not add,
  // remove or move an onset), so a naive startTick/24 map cannot recover steps.
  {
    SynthPattern& p = f.current();
    int pulled = -1;
    for (int st = 1; st < 16 && pulled < 0; ++st) if (p.steps[st].note >= 0) pulled = st;
    assert(pulled > 0);
    p.steps[pulled].timing = -18;
  }
  PhraseRuntime::RuntimeSynthEventBuffer source{};
  uint8_t steps[16];
  assert(f.engine.acquireWorkingMelodySourceImpl_(0, source, &steps));
  bool nonStraight = false;
  for (uint16_t i = 0; i < source.count; ++i) {
    if (source.events[i].startTick != steps[i] * 24u) nonStraight = true;
  }
  assert(nonStraight);  // swing/micro-timing must make startTick != step * 24 for some event
  // Reference map from the projection owner (independent call).
  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = 0;
  settings.swingEnabled = true;
  settings.swingPercent = 62;
  settings.gateLengthRatio = f.engine.genreManager().getGrooveRecipe().gateLengthRatio;
  PhraseRuntime::RuntimeSynthEventBuffer ref{};
  uint8_t refSteps[16];
  assert(PhraseRuntime::projectPatternToRuntimeEventsWithSourceSteps(
             f.engine.activeSynthPattern(0), settings, ref, refSteps) ==
         PhraseRuntime::PatternProjectionStatus::Ready);
  for (uint16_t i = 0; i < source.count; ++i) assert(steps[i] == refSteps[i]);

  auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
  assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared);
  expectContinues(obs);
  // A tick/24 "map" would have produced a different mask and failed R2:
  StepMask tickMap = 0;
  for (uint16_t i = 0; i < source.count; ++i) {
    tickMap = static_cast<StepMask>(tickMap | stepBit(static_cast<uint8_t>(source.events[i].startTick / 24u)));
  }
  assert(tickMap != static_cast<StepMask>(f.origin().bassRhythm.onsets | f.origin().bassRhythm.continuations));
}

// ---- Continuation-bearing route (owner realizes continuations as held notes) --
void test_continuation_bar() {
  // Reggae/3, 8 bars has bars of onset + continuations (see D1-B evolving route).
  // Generation varies with the attempt ordinal: regenerate (bounded) until one
  // bar carries continuations, then select exactly that bar.
  std::unique_ptr<Fixture> fixture;
  int bar = -1;
  for (int attempt = 0; attempt < 24 && bar < 0; ++attempt) {
    fixture.reset(new Fixture("d1c-cont", GenerativeMode::Reggae, 3, 8, 0));
    const GeneratedSynthAOrigin* o = fixture->engine.generatedSynthAOrigin();
    for (int i = 0; i < 8; ++i) if (o->bars[i].bassRhythm.continuations != 0) { bar = i; break; }
  }
  assert(bar >= 0);  // the test is only meaningful if a bar has continuations
  Fixture& f = *fixture;
  {
    const int gs = f.engine.generatedSynthAOrigin()->bars[bar].material.address.globalSlot;
    f.bank = gs / Bank<SynthPattern>::kPatterns;
    f.index = gs % Bank<SynthPattern>::kPatterns;
    f.engine.set303BankIndex(0, f.bank);
    f.engine.set303PatternIndex(0, f.index);
    assert(f.engine.captureCurrentPreparationBasis(0).valid());
  }
  assert(f.origin().bassRhythm.continuations != 0);
  auto obs = f.develop(Dev::TransformationKind::Revoice, 1);
  assert(f.lastPrepare == MiniAcid::NextPrepareResult::Prepared);
  expectContinues(obs);
  // Pristine continuation bar: a pure no-op is EXACT as well.
  PhraseRuntime::RuntimeSynthEventBuffer source{};
  uint8_t steps[16];
  assert(f.engine.acquireWorkingMelodySourceImpl_(0, source, &steps));
  const auto a = Sem::evaluateP0Preservation(f.origin(), source, steps, source);
  assert(a.lineageSummary == SemStatus::Pass && a.predecessorRelation == Sem::StateRelation::Exact);
  std::printf("  continuation bar %d: onsets=%04x continuations=%04x -> CONTINUES\n", bar,
              f.origin().bassRhythm.onsets, f.origin().bassRhythm.continuations);
}

// ---- Aggregation / NEW_IDEA firewall (pure) ---------------------------------
void test_never_fail_never_new_idea() {
  Fixture f("d1c-agg", GenerativeMode::Acid, 0, 2, 0);
  PhraseRuntime::RuntimeSynthEventBuffer source{};
  uint8_t steps[16];
  assert(f.engine.acquireWorkingMelodySourceImpl_(0, source, &steps));
  // Exhaustive single-event mutations of the candidate: never Fail / NewIdea.
  for (uint16_t i = 0; i < source.count; ++i) {
    for (int dn = -13; dn <= 13; ++dn) {
      for (int dt : {-12, 0, 12}) {
        auto c = source;
        c.events[i].note = static_cast<uint8_t>(std::max(0, std::min(127, c.events[i].note + dn)));
        c.events[i].startTick = static_cast<uint16_t>(std::max(0, c.events[i].startTick + dt));
        const auto a = Sem::evaluateP0Preservation(f.origin(), source, steps, c);
        assert(a.lineageSummary != SemStatus::Fail);
        Dev::DevelopmentEvidence ev{};
        Dev::DevelopmentRequest req{};
        const auto facts = Sem::adaptP0Preservation(source, c, ev, req, a);
        assert(facts.lineage != Sem::LineageStatus::NewIdea);
        const bool pass = a.r1BarExtent == SemStatus::Pass && a.r2BassOnsetTopology == SemStatus::Pass &&
                          a.r3PitchClassAtOnset == SemStatus::Pass;
        assert((a.lineageSummary == SemStatus::Pass) == pass);
        assert((facts.lineage == Sem::LineageStatus::Continues) == pass);
      }
    }
  }
  // Hostile assessment carrying Fail is neutralised at the provider seam.
  Sem::P0PreservationAssessment hostile{};
  hostile.available = true;
  hostile.lineageSummary = SemStatus::Fail;
  Dev::DevelopmentEvidence ev{};
  Dev::DevelopmentRequest req{};
  assert(Sem::adaptP0Preservation(source, source, ev, req, hostile).lineage !=
         Sem::LineageStatus::NewIdea);
}

// ---- Publication neutrality (26) ------------------------------------------
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

void test_publication_neutrality() {
  Fixture f("d1c-neutral", GenerativeMode::Acid, 0, 2, 0);
  for (auto kind : {Dev::TransformationKind::Revoice, Dev::TransformationKind::Displace,
                    Dev::TransformationKind::Thin, Dev::TransformationKind::Move}) {
    Dev::DevelopmentRequest req{};
    req.transformation = kind;
    req.octaveShift = 1;
    req.genreId = static_cast<uint8_t>(GenerativeMode::Acid);

    Dev::DevelopmentResult warm{};
    (void)f.engine.cancelNextMaterial(0);
    (void)f.engine.developWorkingMaterial(0, req, &warm);  // warm-up (anchor snapshot side effect)

    Dev::DevelopmentResult withoutOut{}, withOut{};
    (void)f.engine.cancelNextMaterial(0);
    const auto r1 = f.engine.developWorkingMaterial(0, req, &withoutOut);
    const auto s1 = snap(f.engine, r1, withoutOut);

    Sem::DevelopmentSemanticObservation obs{};
    (void)f.engine.cancelNextMaterial(0);
    const auto r2 = f.engine.developWorkingMaterial(0, req, &withOut, &obs);
    const auto s2 = snap(f.engine, r2, withOut);
    assert(sameSnap(s1, s2));
    // Generated origin untouched by observation.
    assert(f.engine.generatedSynthAOrigin() != nullptr);
  }
  std::puts("D1-C neutrality: with/without semantic output identical for 4 operations: PASS");
}

void report_sizes() {
  std::printf("D1-C sizes: P0PreservationAssessment=%zu DevelopmentSemanticObservation=%zu "
              "SemanticFacts=%zu GeneratedSynthAOrigin=%zu\n",
              sizeof(Sem::P0PreservationAssessment), sizeof(Sem::DevelopmentSemanticObservation),
              sizeof(Sem::SemanticFacts), sizeof(GeneratedSynthAOrigin));
}

}  // namespace

int main() {
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");
  report_sizes();
  test_positive_matrix();          std::puts("D1-C A-E positive matrix: PASS");
  test_displace_and_thin();        std::puts("D1-C F/G DISPLACE/THIN -> R2 FAIL, UNKNOWN, never NEW_IDEA: PASS");
  test_chromatic_candidate();      std::puts("D1-C H chromatic candidate + R1: PASS");
  test_current_edits_and_version_witness();
                                   std::puts("D1-C I/J/K/L + version witness: PASS");
  test_unavailable_scopes();       std::puts("D1-C M/N/O/P unavailable scopes: PASS");
  test_swing_source_map();         std::puts("D1-C non-straight timing source map: PASS");
  test_continuation_bar();         std::puts("D1-C continuation-bearing bar: PASS");
  test_never_fail_never_new_idea();std::puts("D1-C one-sided aggregation / NEW_IDEA firewall: PASS");
  test_publication_neutrality();
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");
  std::filesystem::remove("grooveputer_scene_name.txt");
  std::puts("0.9.14 D1-C P0 preservation evaluator: ALL PASS");
  return 0;
}
