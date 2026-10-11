#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/phrase_generator.h"
#include "src/state/generated_synth_a_origin.h"
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


using GroovePuterRhythm::BassPitchClassWitness;
using GroovePuterRhythm::StepMask;
using GroovePuterRhythm::stepBit;

struct Route1 { uint8_t bars; GenerativeMode mode; GenreRecipeId recipe; };
// Existing repository vocabulary, chosen by scan (see report):
//   Acid/0   4 bars : up to 3 distinct bass pitch classes per bar, bars differ
//   Outrun/0 4 bars : two harmonic events per bar, up to 4 distinct pitch classes
constexpr Route1 kAcid4{4, GenerativeMode::Acid, 0};
constexpr Route1 kOutrun4{4, GenerativeMode::Outrun, 0};
// 0.9.18: Outrun's genre idiom holds one chord per bar, so in-bar harmonic
// motion (P6) is checked on TripHop, whose generator the idioms leave alone.
constexpr Route1 kInBarHarmony4{4, GenerativeMode::TripHop, 0};

uint8_t pcOfNote(int note) { return static_cast<uint8_t>(note % 12); }

int distinctPitchClasses(const BassPitchClassWitness& w, StepMask onsets) {
  uint16_t seen = 0;
  int count = 0;
  for (uint8_t step = 0; step < 16; ++step) {
    if ((onsets & stepBit(step)) == 0) continue;
    const uint8_t pc = w.pitchClassAt(step);
    if ((seen & (1u << pc)) == 0) { seen = static_cast<uint16_t>(seen | (1u << pc)); ++count; }
  }
  return count;
}

// Compares only meaningful nibbles (steps in `onsets`).
bool sameWitnessOn(const BassPitchClassWitness& a, const BassPitchClassWitness& b,
                   StepMask onsets) {
  for (uint8_t step = 0; step < 16; ++step) {
    if ((onsets & stepBit(step)) != 0 && a.pitchClassAt(step) != b.pitchClassAt(step)) {
      return false;
    }
  }
  return true;
}

std::string witnessString(const BassPitchClassWitness& w, StepMask onsets) {
  std::string out;
  for (uint8_t step = 0; step < 16; ++step) {
    if ((onsets & stepBit(step)) == 0) continue;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%s%u@%u", out.empty() ? "" : " ", w.pitchClassAt(step), step);
    out += buf;
  }
  return out;
}

// Cross-check ONLY (publication authority is the tonal plan): witness against a
// committed Synth A.
bool witnessMatchesSynth(const BassPitchClassWitness& w, StepMask onsets,
                         const SynthPattern& synth) {
  for (uint8_t step = 0; step < 16; ++step) {
    if ((onsets & stepBit(step)) == 0) continue;
    if (synth.steps[step].note < 0) return false;  // attack must be a real note
    if (w.pitchClassAt(step) != pcOfNote(synth.steps[step].note)) return false;
  }
  return true;
}

// ---- P1 -------------------------------------------------------------------
void test_p1_packing() {
  for (uint8_t step = 0; step < 16; ++step) {
    for (uint8_t pc = 0; pc < 12; ++pc) {
      BassPitchClassWitness w{};
      assert(w.setPitchClass(step, pc));
      assert(w.pitchClassAt(step) == pc);
      for (uint8_t other = 0; other < 16; ++other) {
        if (other != step) assert(w.pitchClassAt(other) == 0);  // no bleed
      }
    }
  }
  // Full-width distinct pattern round-trips through all 16 nibbles.
  BassPitchClassWitness all{};
  for (uint8_t step = 0; step < 16; ++step) assert(all.setPitchClass(step, (step * 7) % 12));
  for (uint8_t step = 0; step < 16; ++step) assert(all.pitchClassAt(step) == (step * 7) % 12);
  // Overwrite keeps neighbours.
  assert(all.setPitchClass(9, 3));
  assert(all.pitchClassAt(8) == (8 * 7) % 12 && all.pitchClassAt(10) == (10 * 7) % 12);
  // Fail closed.
  const BassPitchClassWitness snapshot = all;
  assert(!all.setPitchClass(16, 1));
  assert(!all.setPitchClass(0, 12));
  assert(!all.setPitchClass(255, 0));
  assert(all == snapshot);
  assert(all.pitchClassAt(16) == 0xFF);

  // makeBassPitchClassWitness: pitch class = midi % 12, mask must match exactly.
  GroovePuterRhythm::TonalMaterializationPlan plan{};
  plan.onsetCount = 3;
  plan.onsetSteps[0] = 0; plan.midiNotes[0] = 36;   // C  -> 0
  plan.onsetSteps[1] = 6; plan.midiNotes[1] = 43;   // G  -> 7
  plan.onsetSteps[2] = 12; plan.midiNotes[2] = 51;  // Eb -> 3
  const StepMask mask = static_cast<StepMask>(stepBit(0) | stepBit(6) | stepBit(12));
  BassPitchClassWitness built{};
  assert(GroovePuterRhythm::makeBassPitchClassWitness(plan, mask, built));
  assert(built.pitchClassAt(0) == 0 && built.pitchClassAt(6) == 7 && built.pitchClassAt(12) == 3);
  BassPitchClassWitness untouched{};
  assert(!GroovePuterRhythm::makeBassPitchClassWitness(plan, static_cast<StepMask>(mask | stepBit(1)), untouched));
  assert(untouched == BassPitchClassWitness{});
  plan.onsetSteps[2] = 6;  // duplicate step
  assert(!GroovePuterRhythm::makeBassPitchClassWitness(plan, mask, untouched));
  plan.onsetSteps[2] = 16;  // out of range
  assert(!GroovePuterRhythm::makeBassPitchClassWitness(plan, mask, untouched));
  std::puts("D1-B1 P1: nibble packing/unpacking + fail-closed builder: PASS");
}

// ---- P2 / P3 / P7 / P11 ---------------------------------------------------
// Exact owner result: the witness exported at the migration seam equals the
// pitch classes of the very bass TonalMaterializationPlan adapted into Synth A
// (captured by the host-test probe), and the P1R seam forwards it unchanged.
void test_p2_p3_p7_p11_owner_result_and_mutants() {
  freshProject("d1b1-owner");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);

  bool sawMultiPitchBar = false;
  bool sawBarsDiffer = false;
  bool sawSynthBDiffers = false;
  bool sawTonalProfileDiffers = false;
  for (const Route1& route : {kAcid4, kOutrun4}) {
    configureScene(engine, route.mode, route.recipe);
    GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
    assert(GeneratedPhraseSong::prepare(engine, route.bars, 0, prepared));
    assert(prepared.useP1RRoute);

    BassPitchClassWitness previous{};
    StepMask previousOnsets = 0;
    for (uint8_t bar = 0; bar < route.bars; ++bar) {
      const int16_t address = static_cast<int16_t>(
          songPatternFromPageBankIndex(0, 0, prepared.firstLocalSlot + bar));
      GroovePuterRhythm::StrongRhythmBassTonalPlanProbe probe{};
      GroovePuterRhythm::setStrongRhythmBassTonalPlanProbe(&probe);
      PhraseGenerator::PhraseBar direct{};
      assert(GeneratedPhraseP1R::prepareDestinationIndependentPitchSource(
          engine, prepared.p1rExecution, direct));
      const auto migration = GroovePuterRhythm::materializePreparedPhraseBar(
          prepared.p1rExecution, bar, address, direct.drums, direct.synthA, direct.synthB);
      GroovePuterRhythm::setStrongRhythmBassTonalPlanProbe(nullptr);
      assert(migration.status == GroovePuterRhythm::StrongRhythmMigrationStatus::Applied);
      assert(probe.captured);
      assert(migration.bassPitchClassWitnessAvailable);
      const StepMask onsets = migration.bassRhythmPlan.onsets;
      const BassPitchClassWitness& witness = migration.bassPitchClassWitness;

      // P2: exact owner result -> witness (attack set and every pitch class).
      // 0.9.18: the genre idiom owns the committed Synth A of Acid and Outrun,
      // so the owner result is that pattern, read attack by attack (a held
      // step -- same pitch, slid into -- is a continuation, not an attack).
      assert(probe.plan.onsetCount > 0);
      StepMask patternAttacks = 0;
      for (uint8_t step = 0; step < GroovePuterRhythm::kStepsPerBar; ++step) {
        const SynthStep& event = direct.synthA.steps[step];
        if (event.note < 0) continue;
        if (step > 0 && event.slide && direct.synthA.steps[step - 1].note == event.note) continue;
        patternAttacks = static_cast<StepMask>(patternAttacks | stepBit(step));
        assert(witness.pitchClassAt(step) == static_cast<uint8_t>(event.note % 12));
      }
      assert(patternAttacks == onsets);

      // P3: the one-bar seam forwards the very same witness.
      PhraseGenerator::PhraseBar viaSeam{};
      GeneratedPhraseP1R::MaterializedSynthABarEvidence evidence{};
      assert(GeneratedPhraseP1R::materializeOneBar(
          engine, prepared.p1rExecution, bar, address, viaSeam, evidence));
      assert(evidence.valid);
      assert(evidence.bassPitchClasses == witness);
      assert(sameBass(evidence.bassRhythm, migration.bassRhythmPlan));

      // Mutant A (default witness): the real witness is not all-zero.
      assert(witness != BassPitchClassWitness{} || distinctPitchClasses(witness, onsets) == 1);
      const int distinct = distinctPitchClasses(witness, onsets);
      if (distinct > 1) {
        sawMultiPitchBar = true;
        assert(witness != BassPitchClassWitness{});  // a default export would fail
      }

      // Mutant B (stale previous-bar witness).
      if (bar > 0 && !sameWitnessOn(witness, previous, onsets | previousOnsets)) {
        sawBarsDiffer = true;
        assert(!sameWitnessOn(previous, witness, onsets | previousOnsets));
      }

      // Mutant C (witness taken from Synth B).
      BassPitchClassWitness fromB{};
      bool bComplete = true;
      for (uint8_t step = 0; step < 16; ++step) {
        if ((onsets & stepBit(step)) == 0) continue;
        if (direct.synthB.steps[step].note < 0) { bComplete = false; continue; }
        fromB.setPitchClass(step, pcOfNote(direct.synthB.steps[step].note));
      }
      if (!sameWitnessOn(witness, fromB, onsets) || !bComplete) sawSynthBDiffers = true;

      // Mutant D (recomputed under a different tonal profile: root + 1).
      GroovePuterRhythm::PreparedPhraseExecution shifted = prepared.p1rExecution;
      shifted.materialization.rootPitchClass =
          static_cast<uint8_t>((shifted.materialization.rootPitchClass + 1) % 12);
      PhraseGenerator::PhraseBar other{};
      assert(GeneratedPhraseP1R::prepareDestinationIndependentPitchSource(
          engine, shifted, other));
      const auto shiftedResult = GroovePuterRhythm::materializePreparedPhraseBar(
          shifted, bar, address, other.drums, other.synthA, other.synthB);
      if (shiftedResult.status == GroovePuterRhythm::StrongRhythmMigrationStatus::Applied &&
          shiftedResult.bassPitchClassWitnessAvailable &&
          !sameWitnessOn(witness, shiftedResult.bassPitchClassWitness, onsets)) {
        sawTonalProfileDiffers = true;
      }
      previous = witness;
      previousOnsets = onsets;
    }
  }
  // Discrimination conditions must hold or the test is vacuous.
  assert(sawMultiPitchBar);       // P7: distinctPitchClasses > 1
  assert(sawBarsDiffer);          // P7/B: witness(barA) != witness(barB)
  assert(sawSynthBDiffers);       // C: Synth B export would be caught
  assert(sawTonalProfileDiffers); // D: other tonal profile would be caught
  std::puts("D1-B1 P2/P3/P7/P11: owner tonal plan -> witness, forwarding, mutants discriminated: PASS");
}

// ---- P4 / P5 ---------------------------------------------------------------
void test_p4_p5_sidecar_witness_all_lengths() {
  freshProject("d1b1-lengths");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);

  for (const auto& route : kRoutes) {
    configureScene(engine, route.mode, route.recipe);
    Scene& scene = engine.sceneManager().currentScene();
    const auto result = GeneratedPhraseSong::generate(engine, route.bars, 0, kGuard);
    assert(result.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
    const GeneratedSynthAOrigin* origin = engine.generatedSynthAOrigin();
    assert(origin != nullptr && origin->common.barCount == route.bars);
    for (int bar = 0; bar < route.bars; ++bar) {
      const int slot = result.phrase.firstLocalSlot + bar;
      const SynthPattern& committed =
          scene.synthABanks[slot / Bank<SynthPattern>::kPatterns]
              .patterns[slot % Bank<SynthPattern>::kPatterns];
      const auto& entry = origin->bars[bar];
      // P5 cross-check: every attack has the committed pitch class, and every
      // witnessed attack is a real bass onset that is a real note.
      assert(witnessMatchesSynth(entry.bassPitchClasses, entry.bassRhythm.onsets, committed));
    }
  }
  std::puts("D1-B1 P4/P5: 1/2/4/8-bar witnesses cross-check against committed Synth A: PASS");
}

// ---- P6 / P7 ---------------------------------------------------------------
void test_p6_p7_multi_harmonic_and_nontrivial() {
  freshProject("d1b1-harmonic");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);

  // P7: nontrivial pitch classes on a real repository route.
  configureScene(engine, kAcid4.mode, kAcid4.recipe);
  auto acid = GeneratedPhraseSong::generate(engine, kAcid4.bars, 0, kGuard);
  assert(acid.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  const GeneratedSynthAOrigin* origin = engine.generatedSynthAOrigin();
  assert(origin != nullptr);
  int maxDistinct = 0;
  bool differ = false;
  for (int bar = 0; bar < kAcid4.bars; ++bar) {
    const auto& e = origin->bars[bar];
    maxDistinct = std::max(maxDistinct, distinctPitchClasses(e.bassPitchClasses, e.bassRhythm.onsets));
    if (bar > 0 && !sameWitnessOn(e.bassPitchClasses, origin->bars[0].bassPitchClasses,
                                  e.bassRhythm.onsets | origin->bars[0].bassRhythm.onsets)) {
      differ = true;
    }
    std::printf("  Acid/0 bar %d pitch classes: %s\n", bar,
                witnessString(e.bassPitchClasses, e.bassRhythm.onsets).c_str());
  }
  assert(maxDistinct > 1);
  assert(differ);

  // P6: bars with multiple harmonic events.
  configureScene(engine, kInBarHarmony4.mode, kInBarHarmony4.recipe);
  auto outrun = GeneratedPhraseSong::generate(engine, kInBarHarmony4.bars, 0, kGuard);
  assert(outrun.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  origin = engine.generatedSynthAOrigin();
  assert(origin != nullptr);
  bool segmentsDiffer = false;
  int multiEventBars = 0;
  for (int bar = 0; bar < kInBarHarmony4.bars; ++bar) {
    const auto& e = origin->bars[bar];
    const StepMask events = e.harmonicRhythm.onsets;
    int eventCount = 0;
    for (uint8_t step = 0; step < 16; ++step) if (events & stepBit(step)) ++eventCount;
    assert(eventCount == e.harmonicRhythm.eventCount);
    if (eventCount < 2) continue;
    ++multiEventBars;
    // Split bass attacks at the second harmonic event.
    uint8_t secondEvent = 0;
    int seen = 0;
    for (uint8_t step = 0; step < 16; ++step) {
      if (events & stepBit(step)) { if (++seen == 2) { secondEvent = step; break; } }
    }
    uint16_t firstSet = 0, secondSet = 0;
    for (uint8_t step = 0; step < 16; ++step) {
      if ((e.bassRhythm.onsets & stepBit(step)) == 0) continue;
      const uint16_t bit = static_cast<uint16_t>(1u << e.bassPitchClasses.pitchClassAt(step));
      if (step < secondEvent) firstSet = static_cast<uint16_t>(firstSet | bit);
      else secondSet = static_cast<uint16_t>(secondSet | bit);
    }
    if (firstSet != 0 && secondSet != 0 && firstSet != secondSet) segmentsDiffer = true;
    std::printf("  Outrun/0 bar %d harmonic events=%d (second at step %u): %s\n", bar,
                eventCount, secondEvent,
                witnessString(e.bassPitchClasses, e.bassRhythm.onsets).c_str());
  }
  assert(multiEventBars > 0);
  assert(segmentsDiffer);  // pitch classes follow the active harmonic event, not one root per bar
  std::puts("D1-B1 P6/P7: multi-harmonic-event and nontrivial witnesses: PASS");
}

// ---- P8 / P9 / P10 ---------------------------------------------------------
void test_p8_p9_p10_lifecycle_with_witness() {
  freshProject("d1b1-lifecycle");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  engine.sceneStorage_ = &storage;
  configureScene(engine, kAcid4.mode, kAcid4.recipe);
  Scene& scene = engine.sceneManager().currentScene();

  const auto a = GeneratedPhraseSong::generate(engine, 2, 0, kGuard);
  assert(a.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  const GeneratedSynthAOrigin sidecarA = *engine.generatedSynthAOrigin();

  // P8: failed generation (reservation I/O failure) keeps the sidecar + witness.
  const std::filesystem::path meta = newestIdentityMeta();
  assert(!meta.empty());
  const std::filesystem::path blocker = meta.string() + ".tmp";
  std::filesystem::create_directories(blocker / "blocked");
  assert(GeneratedPhraseSong::generate(engine, 2, 2, kGuard).status ==
         GeneratedPhraseSong::LifecycleStatus::Failed);
  std::filesystem::remove_all(blocker);
  assert(sameOrigin(sidecarA, *engine.generatedSynthAOrigin()));
  for (int i = 0; i < 2; ++i) {
    assert(engine.generatedSynthAOrigin()->bars[i].bassPitchClasses ==
           sidecarA.bars[i].bassPitchClasses);
  }

  // P10: manual CURRENT edit of pitch does not rewrite the origin witness.
  const int slot = a.phrase.firstLocalSlot;
  SynthPattern& current = scene.synthABanks[slot / Bank<SynthPattern>::kPatterns]
                              .patterns[slot % Bank<SynthPattern>::kPatterns];
  const StepMask onsets = sidecarA.bars[0].bassRhythm.onsets;
  uint8_t firstStep = 0;
  while ((onsets & stepBit(firstStep)) == 0) ++firstStep;
  current.steps[firstStep].note = static_cast<int8_t>(current.steps[firstStep].note + 1);
  assert(!witnessMatchesSynth(sidecarA.bars[0].bassPitchClasses, onsets, current));  // CURRENT changed
  assert(engine.generatedSynthAOrigin()->bars[0].bassPitchClasses ==
         sidecarA.bars[0].bassPitchClasses);                                         // origin did not
  assert(sameOrigin(sidecarA, *engine.generatedSynthAOrigin()));

  // P9: Undo clears.
  configureScene(engine, kAcid4.mode, kAcid4.recipe);
  assert(GeneratedPhraseSong::generate(engine, 2, 0, kGuard).status ==
         GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  assert(engine.generatedSynthAOrigin() != nullptr);
  assert(GeneratedPhraseSong::undoLastGeneratedPhrase(engine, kGuard) ==
         GroovePuterUndo::UndoResult::Restored);
  assert(engine.generatedSynthAOrigin() == nullptr);

  // P9: successful Legacy generation clears.
  configureScene(engine, kAcid4.mode, kAcid4.recipe);
  assert(GeneratedPhraseSong::generate(engine, 2, 0, kGuard).status ==
         GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  assert(engine.generatedSynthAOrigin() != nullptr);
  scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
  scene.genre.recipe = 250;
  engine.genreManager().setGenerativeMode(GenerativeMode::Techno);
  assert(GeneratedPhraseSong::generate(engine, 1, 2, kGuard).status ==
         GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  assert(engine.generatedSynthAOrigin() == nullptr);

  std::puts("D1-B1 P8/P9/P10: failed-gen / scene-load / Undo / Legacy / CURRENT-edit lifecycle: PASS");
}

// P9: scene load clears the sidecar (both entry points). Each entry point gets
// its own engine because loading replaces the whole scene/project state.
void test_p9_scene_load_clears() {
  for (int entry = 0; entry < 2; ++entry) {
    freshProject(entry == 0 ? "d1b1-load-a" : "d1b1-load-b");
    SceneStorageSdl storage;
    MiniAcid engine{kSampleRate, &storage};
    engine.init();
    engine.setSongMode(false);
    engine.sceneStorage_ = &storage;
    configureScene(engine, kAcid4.mode, kAcid4.recipe);
    assert(GeneratedPhraseSong::generate(engine, 2, 0, kGuard).status ==
           GeneratedPhraseSong::LifecycleStatus::CommittedNow);
    assert(engine.generatedSynthAOrigin() != nullptr);
    if (entry == 0) (void)engine.loadSceneByName("d1b1-nonexistent-scene");
    else engine.loadSceneFromStorage();
    assert(engine.generatedSynthAOrigin() == nullptr);
  }
  std::puts("D1-B1 P9: scene load (by name / from storage) clears sidecar: PASS");
}

void report_sizes() {
  std::printf("D1-B1 sizes: StrongRhythmMigrationResult=%zu barEvidence=%zu "
              "witness=%zu originCommon=%zu originBar=%zu originTotal=%zu "
              "candidate=%zu PreparedPhraseArrangement=%zu UndoPayload=%zu\n",
              sizeof(GroovePuterRhythm::StrongRhythmMigrationResult),
              sizeof(GeneratedPhraseP1R::MaterializedSynthABarEvidence),
              sizeof(BassPitchClassWitness),
              sizeof(GroovePuterMaterial::GeneratedSynthAOriginCommon),
              sizeof(GeneratedSynthABarOrigin), sizeof(GeneratedSynthAOrigin),
              sizeof(GroovePuterMaterial::GeneratedSynthAOriginCandidate),
              sizeof(GeneratedPhraseSong::PreparedPhraseArrangement),
              sizeof(GeneratedPhraseSong::GeneratedPhraseUndoPayload));
  assert(sizeof(GeneratedSynthAOrigin) <= 352);
  assert(sizeof(GeneratedSynthABarOrigin) == 40);
  assert(sizeof(GeneratedPhraseSong::PreparedPhraseArrangement) <= 1024);
}

}  // namespace

int main() {
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");

  report_sizes();
  test_p1_packing();
  test_p2_p3_p7_p11_owner_result_and_mutants();
  test_p4_p5_sidecar_witness_all_lengths();
  test_p6_p7_multi_harmonic_and_nontrivial();
  test_p8_p9_p10_lifecycle_with_witness();
  test_p9_scene_load_clears();

  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");
  std::filesystem::remove("grooveputer_scene_name.txt");  // scene-load test artifact
  std::puts("0.9.14 D1-B1 pitch-class origin witness: ALL PASS");
  return 0;
}
