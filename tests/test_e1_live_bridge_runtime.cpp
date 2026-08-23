#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "platform_sdl/arduino_compat.h"
#include "src/audio/audio_config.h"
#include "src/dsp/miniacid_engine.h"
#include "src/generation/feel/feel_pattern_adapter.h"
#include "src/generation/materialization/pattern_materializer.h"
#include "src/generation/migration/strong_rhythm_live_bridge.h"
#include "src/generation/phrase/phrase_evolution.h"
#include "src/generation/rhythm/bar_evolution.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/state/generation_request_state.h"

SerialMock Serial;
SDMock SD;

using namespace GroovePuterRhythm;

namespace {

constexpr int kAuditionBank = 1;
constexpr int kAuditionSongSlot = 1;
constexpr RhythmArchetypeId kFixtureArchetype = 404;

[[noreturn]] void fail(const char* message) {
  std::fprintf(stderr, "E1B ASSERTION FAILED: %s\n", message);
  std::abort();
}

void require(bool condition, const char* message) {
  if (!condition) fail(message);
}

bool sameRole(const RoleRhythmPlan& a, const RoleRhythmPlan& b) {
  return a.structural == b.structural &&
         a.secondary == b.secondary &&
         a.ghosts == b.ghosts &&
         a.shortGate == b.shortGate &&
         a.heldGate == b.heldGate &&
         a.tieGate == b.tieGate &&
         a.accents == b.accents;
}

bool sameBar(const RhythmBarPlan& a, const RhythmBarPlan& b) {
  if (a.function != b.function) return false;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (!sameRole(a.roles[role], b.roles[role])) return false;
  }
  return true;
}

bool sameIdentity(const PhraseRhythmIdentity& a,
                  const PhraseRhythmIdentity& b) {
  if (a.archetypeId != b.archetypeId ||
      a.phraseBars != b.phraseBars ||
      a.trajectoryId != b.trajectoryId ||
      a.protectedSpaceCount != b.protectedSpaceCount) {
    return false;
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (a.structuralCore[bar][role] != b.structuralCore[bar][role] ||
          a.canonicalCore[bar][role] != b.canonicalCore[bar][role]) {
        return false;
      }
    }
  }
  for (uint8_t index = 0; index < a.protectedSpaceCount; ++index) {
    if (a.protectedSpaces[index].steps != b.protectedSpaces[index].steps ||
        a.protectedSpaces[index].affectedRoles !=
            b.protectedSpaces[index].affectedRoles) {
      return false;
    }
  }
  return true;
}

bool sameRoleIdentity(const PhraseRoleIdentity& a,
                      const PhraseRoleIdentity& b) {
  return a.bass == b.bass && a.chord == b.chord &&
         a.melodic == b.melodic && a.motif == b.motif;
}

bool sameDrumStep(const DrumStep& a, const DrumStep& b) {
  return a.hit == b.hit && a.accent == b.accent &&
         a.velocity == b.velocity && a.timing == b.timing &&
         a.fx == b.fx && a.fxParam == b.fxParam &&
         a.probability == b.probability;
}

bool sameDrums(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (!sameDrumStep(a.voices[voice].steps[step],
                        b.voices[voice].steps[step])) {
        return false;
      }
    }
  }
  for (int lane = 0; lane < DrumPatternSet::kMaxLanes; ++lane) {
    if (a.lanes[lane].targetParam != b.lanes[lane].targetParam ||
        a.lanes[lane].nodeCount != b.lanes[lane].nodeCount) {
      return false;
    }
    for (int node = 0; node < AutomationLane::kMaxNodes; ++node) {
      if (a.lanes[lane].nodes[node].step != b.lanes[lane].nodes[node].step ||
          a.lanes[lane].nodes[node].value != b.lanes[lane].nodes[node].value ||
          a.lanes[lane].nodes[node].curveType !=
              b.lanes[lane].nodes[node].curveType) {
        return false;
      }
    }
  }
  return a.groove.swing == b.groove.swing &&
         a.groove.humanize == b.groove.humanize;
}

bool sameSynth(const SynthPattern& a, const SynthPattern& b) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& left = a.steps[step];
    const SynthStep& right = b.steps[step];
    if (left.note != right.note || left.slide != right.slide ||
        left.accent != right.accent || left.ghost != right.ghost ||
        left.velocity != right.velocity || left.timing != right.timing ||
        left.fx != right.fx || left.fxParam != right.fxParam ||
        left.probability != right.probability) {
      return false;
    }
  }
  return true;
}

bool sameSong(const Song& a, const Song& b) {
  if (a.length != b.length || a.reverse != b.reverse) return false;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      if (a.positions[row].patterns[track] !=
          b.positions[row].patterns[track]) {
        return false;
      }
    }
  }
  return true;
}

uint32_t mix32(uint32_t hash, uint32_t value) {
  for (uint8_t shift = 0; shift < 32; shift += 8) {
    hash ^= static_cast<uint8_t>(value >> shift);
    hash *= 16777619u;
  }
  return hash;
}

uint32_t drumFingerprint(const DrumPatternSet& pattern) {
  uint32_t hash = 2166136261u;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& event = pattern.voices[voice].steps[step];
      hash = mix32(hash, event.hit);
      hash = mix32(hash, event.accent);
      hash = mix32(hash, event.velocity);
      hash = mix32(hash, static_cast<uint8_t>(event.timing));
      hash = mix32(hash, event.fx);
      hash = mix32(hash, event.fxParam);
      hash = mix32(hash, event.probability);
    }
  }
  return hash;
}

uint32_t synthFingerprint(const SynthPattern& pattern) {
  uint32_t hash = 2166136261u;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& event = pattern.steps[step];
    hash = mix32(hash, static_cast<uint8_t>(event.note));
    hash = mix32(hash, event.slide);
    hash = mix32(hash, event.accent);
    hash = mix32(hash, event.ghost);
    hash = mix32(hash, event.velocity);
    hash = mix32(hash, static_cast<uint8_t>(event.timing));
    hash = mix32(hash, event.fx);
    hash = mix32(hash, event.fxParam);
    hash = mix32(hash, event.probability);
  }
  return hash;
}

void printBar(uint8_t requestedBars,
              uint8_t barIndex,
              const RhythmBarPlan& bar) {
  std::printf("DIRECT-BAR requested=%u bar=%u function=%u",
              static_cast<unsigned>(requestedBars),
              static_cast<unsigned>(barIndex),
              static_cast<unsigned>(bar.function));
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    const RoleRhythmPlan& plan = bar.roles[role];
    std::printf(" r%u=%04x/%04x/%04x/%04x/%04x/%04x/%04x",
                static_cast<unsigned>(role),
                plan.structural, plan.secondary, plan.ghosts,
                plan.shortGate, plan.heldGate, plan.tieGate, plan.accents);
  }
  std::puts("");
}

uint32_t auditionSeed(const GenreSettings& settings,
                      RhythmArchetypeId archetypeId,
                      int patternAddress) {
  uint32_t value = 2166136261u;
  const uint32_t words[] = {
      settings.generativeMode,
      settings.recipe,
      settings.morphTarget,
      settings.morphAmount,
      archetypeId,
      static_cast<uint32_t>(patternAddress),
      0x41554449u,
  };
  for (uint32_t word : words) {
    value ^= word;
    value *= 16777619u;
  }
  return value;
}

GenerationContext auditionGenerationContext(const GenreSettings& settings,
                                             RhythmArchetypeId archetypeId,
                                             int patternAddress,
                                             uint16_t phraseOrdinal) {
  GenerationContext generation{};
  generation.projectSeed = auditionSeed(settings, archetypeId, patternAddress);
  generation.phraseOrdinal = phraseOrdinal;
  return generation;
}

StrongRhythmMigrationContext liveContext(const MiniAcid& engine,
                                         int patternAddress) {
  StrongRhythmMigrationContext context{};
  const Scene& scene = engine.sceneManager().currentScene();
  context.patternAddress = static_cast<int16_t>(patternAddress);
  context.level = GroovePuterState::currentGenerationLevel();
  context.feelProfile = static_cast<FeelProfileId>(scene.feel.timingProfile);

  float amount = scene.generatorParams.microTimingAmount * 100.0f;
  if (amount < 0.0f) amount = 0.0f;
  if (amount > 100.0f) amount = 100.0f;
  context.feelAmount = static_cast<uint8_t>(amount + 0.5f);

  context.tonalMaterializationEnabled = true;
  int root = scene.generatorParams.scaleRoot % 12;
  if (root < 0) root += 12;
  context.rootPitchClass = static_cast<uint8_t>(root);
  context.scaleTypeValue =
      static_cast<ScaleTypeValue>(scene.generatorParams.scale);
  return context;
}

RhythmRoleMask deferredSynthRoles() {
  return static_cast<RhythmRoleMask>(
      rhythmRoleBit(RhythmRole::BassRhythm) |
      rhythmRoleBit(RhythmRole::ChordRhythm) |
      rhythmRoleBit(RhythmRole::MelodicRhythm));
}

DrumPatternSet materializeExpectedDrums(
    const RhythmBarPlan& bar,
    RealizationLevel level,
    FeelProfileId profile,
    uint8_t feelAmount,
    const GenerationContext& generation) {
  RhythmPhrasePlan oneBar{};
  oneBar.barCount = 1;
  oneBar.level = level;
  oneBar.bars[0] = bar;
  oneBar.bars[0].function = BarFunction::Statement;

  const PatternMaterializerBinding binding =
      standardDrumPatternBinding(deferredSynthRoles());
  MaterializedPatterns candidate{};
  require(materializeRhythmPattern(oneBar, binding, candidate) ==
              PatternMaterializeStatus::Ok,
          "direct evolved bar materialization failed");
  require(applyFeelToMaterializedPattern(
              oneBar, binding, profile, feelAmount, generation, candidate) ==
              FeelPatternApplyStatus::Ok,
          "direct evolved bar FEEL materialization failed");
  return candidate.drums;
}

void configureFixture(MiniAcid& engine, uint8_t requestedBars) {
  SceneManager& manager = engine.sceneManager();
  manager.wipeToZero();

  engine.setCurrentPage(0);
  engine.setSongMode(false);
  engine.setActiveSongSlot(0);
  engine.setSongPlaybackSlot(0);
  engine.setSongPosition(0);
  engine.setLoopRange(0, 0);
  engine.setLoopMode(false);
  engine.setDrumBankIndex(0);
  engine.setDrumPatternIndex(0);
  engine.set303BankIndex(0, 0);
  engine.set303BankIndex(1, 0);
  engine.set303PatternIndex(0, 0);
  engine.set303PatternIndex(1, 0);

  Scene& scene = manager.currentScene();
  scene.genre = GenreSettings{};
  scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
  scene.genre.recipe = kBaseRecipeId;
  scene.genre.morphTarget = 0;
  scene.genre.morphAmount = 0;
  scene.genre.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Manual);
  scene.genre.rhythmArchetypeId = kFixtureArchetype;

  scene.feel = FeelSettings{};
  scene.feel.patternBars = requestedBars;
  scene.feel.timingProfile = static_cast<uint8_t>(FeelProfileId::Straight);
  scene.generatorParams = GeneratorParams{};
  scene.generatorParams.microTimingAmount = 0.0f;
  scene.generatorParams.scaleRoot = 0;
  scene.generatorParams.scale = DORIAN;

  GroovePuterState::setGenerationLevel(RealizationLevel::P2Variation);
  GroovePuterState::resetGenerationAttemptState();
}

PhraseEvolutionResult directPhraseFor(MiniAcid& engine,
                                      uint8_t requestedBars,
                                      StrongRhythmMigrationContext& baseContext,
                                      GenreSettings& lockedSettings,
                                      int& basePatternAddress) {
  Scene& scene = engine.sceneManager().currentScene();
  const int page = engine.currentPageIndex();
  basePatternAddress = songPatternFromPageBankIndex(page, kAuditionBank, 0);
  baseContext = liveContext(engine, basePatternAddress);

  DrumPatternSet selectionScratch{};
  const StrongRhythmMigrationResult selection = migrateStrongRhythmDrums(
      scene.genre, baseContext, selectionScratch);
  require(selection.status == StrongRhythmMigrationStatus::Applied,
          "direct production selection did not apply");

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(selection.archetype);
  require(definition != nullptr, "selected archetype has no definition");
  require(definition->archetypeId == kFixtureArchetype,
          "manual fixture did not select archetype 404");
  require(ReferenceVocabulary::phraseEvolutionEnabled(selection.archetype),
          "fixture archetype is not phrase-evolution enabled");

  lockedSettings = scene.genre;
  lockedSettings.morphTarget = 0;
  lockedSettings.morphAmount = 0;
  lockedSettings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Manual);
  lockedSettings.rhythmArchetypeId = definition->archetypeId;

  PhraseEvolutionRequest request{};
  request.catalog = &ReferenceVocabulary::phraseEvolutionCatalog();
  request.archetypeId = definition->archetypeId;
  request.phraseBars = requestedBars;
  request.level = baseContext.level;
  request.generation = auditionGenerationContext(
      lockedSettings,
      definition->archetypeId,
      basePatternAddress,
      static_cast<uint16_t>(basePatternAddress));

  const PhraseEvolutionResult direct = evolveMultiBarPhrase(request);
  require(direct.status == PhraseEvolutionStatus::Ok,
          "direct evolveMultiBarPhrase failed");
  require(direct.barCount == requestedBars, "direct barCount mismatch");
  require(direct.segmentCount == (requestedBars == 8 ? 2 : 1),
          "direct segmentCount mismatch");
  require(direct.rhythmIdentity.archetypeId == definition->archetypeId,
          "direct rhythm identity archetype mismatch");
  require(sameRoleIdentity(direct.roleIdentity, PhraseRoleIdentity{}),
          "production caller role identity is not the default identity");

  BarEvolutionRequest firstRequest{};
  firstRequest.catalog = request.catalog;
  firstRequest.archetypeId = request.archetypeId;
  firstRequest.phraseBars = requestedBars == 8 ? 4 : requestedBars;
  firstRequest.level = request.level;
  firstRequest.generation = request.generation;
  firstRequest.requestedTrajectoryId = request.requestedTrajectoryId;
  firstRequest.reuseIdentity = request.reuseIdentity;
  const BarEvolutionResult first = evolveRhythmPhrase(firstRequest);
  require(first.status == BarEvolutionStatus::Ok,
          "independent first segment evolution failed");
  require(first.trajectoryId == direct.segmentTrajectories[0],
          "first segment trajectory mismatch");
  require(sameIdentity(first.identity, direct.rhythmIdentity),
          "direct multi-bar identity differs from first core segment identity");
  for (uint8_t bar = 0; bar < firstRequest.phraseBars; ++bar) {
    require(sameBar(first.plan.bars[bar], direct.bars[bar]),
            "first semantic segment does not match direct multi-bar result");
  }

  if (requestedBars == 8) {
    GenerationContext secondGeneration = request.generation;
    secondGeneration.phraseOrdinal = static_cast<uint16_t>(
        secondGeneration.phraseOrdinal + 1u);

    BarEvolutionRequest secondRequest = firstRequest;
    secondRequest.generation = secondGeneration;
    secondRequest.reuseIdentity = &first.identity;
    const BarEvolutionResult second = evolveRhythmPhrase(secondRequest);
    require(second.status == BarEvolutionStatus::Ok,
            "independent second segment evolution failed");
    require(second.trajectoryId == direct.segmentTrajectories[1],
            "second segment trajectory mismatch");
    require(sameIdentity(second.identity, first.identity),
            "second 4-bar segment did not reuse the first rhythm identity");
    for (uint8_t bar = 0; bar < 4; ++bar) {
      require(sameBar(second.plan.bars[bar], direct.bars[bar + 4]),
              "second semantic segment does not match direct multi-bar result");
    }

    std::printf(
        "EIGHT-BAR-SEMANTIC firstBars=4 secondBars=4 firstOrdinal=%u "
        "secondOrdinal=%u identity_reused=true traj=%u/%u\n",
        static_cast<unsigned>(request.generation.phraseOrdinal),
        static_cast<unsigned>(secondGeneration.phraseOrdinal),
        static_cast<unsigned>(first.trajectoryId),
        static_cast<unsigned>(second.trajectoryId));
  }

  std::printf(
      "DIRECT requested=%u barCount=%u segmentCount=%u traj=%u/%u "
      "identityArchetype=%u identityBars=%u role=%u/%u/%u/%u "
      "basePatternAddress=%d phraseOrdinal=%u\n",
      static_cast<unsigned>(requestedBars),
      static_cast<unsigned>(direct.barCount),
      static_cast<unsigned>(direct.segmentCount),
      static_cast<unsigned>(direct.segmentTrajectories[0]),
      static_cast<unsigned>(direct.segmentTrajectories[1]),
      static_cast<unsigned>(direct.rhythmIdentity.archetypeId),
      static_cast<unsigned>(direct.rhythmIdentity.phraseBars),
      static_cast<unsigned>(direct.roleIdentity.bass),
      static_cast<unsigned>(direct.roleIdentity.chord),
      static_cast<unsigned>(direct.roleIdentity.melodic),
      static_cast<unsigned>(direct.roleIdentity.motif),
      basePatternAddress,
      static_cast<unsigned>(request.generation.phraseOrdinal));
  for (uint8_t bar = 0; bar < direct.barCount; ++bar) {
    printBar(requestedBars, bar, direct.bars[bar]);
  }

  return direct;
}

void characterizeLiveSuccess(MiniAcid& engine, uint8_t requestedBars) {
  configureFixture(engine, requestedBars);

  StrongRhythmMigrationContext baseContext{};
  GenreSettings lockedSettings{};
  int basePatternAddress = -1;
  const PhraseEvolutionResult direct = directPhraseFor(
      engine, requestedBars, baseContext, lockedSettings, basePatternAddress);

  DrumPatternSet expectedDrums[kMaxProductionPhraseBars]{};
  for (uint8_t bar = 0; bar < requestedBars; ++bar) {
    const int patternAddress = songPatternFromPageBankIndex(
        engine.currentPageIndex(), kAuditionBank, bar);
    const GenerationContext feelGeneration = auditionGenerationContext(
        lockedSettings,
        direct.rhythmIdentity.archetypeId,
        patternAddress,
        static_cast<uint16_t>(basePatternAddress + bar));
    expectedDrums[bar] = materializeExpectedDrums(
        direct.bars[bar],
        baseContext.level,
        baseContext.feelProfile,
        baseContext.feelAmount,
        feelGeneration);
  }

  const PhraseAuditionResult live = regeneratePhraseAuditionWithProbe(engine);
  require(live.status == PhraseAuditionStatus::AppliedEvolved,
          "real live bridge did not return EVOLVED");
  require(live.selectionStatus == StrongRhythmMigrationStatus::Applied,
          "real live bridge selection did not apply");
  require(live.requestedBars == requestedBars,
          "real live bridge requestedBars mismatch");
  require(live.archetypeId == direct.rhythmIdentity.archetypeId,
          "real live bridge archetype differs from direct result");
  require(live.firstTrajectoryId == direct.segmentTrajectories[0] &&
              live.secondTrajectoryId == direct.segmentTrajectories[1],
          "real live bridge trajectory metadata differs from direct result");

  const Scene& scene = engine.sceneManager().currentScene();
  for (uint8_t bar = 0; bar < requestedBars; ++bar) {
    const int patternAddress = songPatternFromPageBankIndex(
        engine.currentPageIndex(), kAuditionBank, bar);
    const DrumPatternSet& liveDrums = scene.drumBanks[kAuditionBank].patterns[bar];
    const SynthPattern& liveSynthA = scene.synthABanks[kAuditionBank].patterns[bar];
    const SynthPattern& liveSynthB = scene.synthBBanks[kAuditionBank].patterns[bar];

    require(sameDrums(liveDrums, expectedDrums[bar]),
            "live Bank B drums do not correspond to direct evolved bar");
    require(engine.songPatternAtSlot(
                kAuditionSongSlot, bar, SongTrack::SynthA) == patternAddress,
            "Song B SynthA reference mismatch");
    require(engine.songPatternAtSlot(
                kAuditionSongSlot, bar, SongTrack::SynthB) == patternAddress,
            "Song B SynthB reference mismatch");
    require(engine.songPatternAtSlot(
                kAuditionSongSlot, bar, SongTrack::Drums) == patternAddress,
            "Song B Drums reference mismatch");

    const unsigned semanticSegment =
        requestedBars == 8 && bar >= 4 ? 2u : 1u;
    std::printf(
        "LIVE-BAR requested=%u bar=%u semanticSegment=%u pattern=%d "
        "function=%u drums=%08x synthA=%08x synthB=%08x refs=%d/%d/%d\n",
        static_cast<unsigned>(requestedBars),
        static_cast<unsigned>(bar),
        semanticSegment,
        patternAddress,
        static_cast<unsigned>(direct.bars[bar].function),
        drumFingerprint(liveDrums),
        synthFingerprint(liveSynthA),
        synthFingerprint(liveSynthB),
        engine.songPatternAtSlot(kAuditionSongSlot, bar, SongTrack::SynthA),
        engine.songPatternAtSlot(kAuditionSongSlot, bar, SongTrack::SynthB),
        engine.songPatternAtSlot(kAuditionSongSlot, bar, SongTrack::Drums));
  }

  if (requestedBars < Song::kMaxPositions) {
    require(engine.songPatternAtSlot(
                kAuditionSongSlot, requestedBars, SongTrack::SynthA) == -1,
            "Song B exposes a half-written SynthA row after phrase end");
    require(engine.songPatternAtSlot(
                kAuditionSongSlot, requestedBars, SongTrack::SynthB) == -1,
            "Song B exposes a half-written SynthB row after phrase end");
    require(engine.songPatternAtSlot(
                kAuditionSongSlot, requestedBars, SongTrack::Drums) == -1,
            "Song B exposes a half-written Drums row after phrase end");
  }

  require(engine.songLength() == requestedBars, "live song length mismatch");
  require(scene.songs[kAuditionSongSlot].length == requestedBars,
          "Song B stored length mismatch");
  require(engine.currentSongPosition() == 0, "live song position is not zero");
  require(engine.loopModeEnabled(), "live loop mode is not enabled");
  require(engine.loopStartRow() == 0, "live loop start mismatch");
  require(engine.loopEndRow() == requestedBars - 1, "live loop end mismatch");
  require(engine.activeSongSlot() == kAuditionSongSlot,
          "live active song slot is not Song B");
  require(engine.songPlaybackSlot() == kAuditionSongSlot,
          "live playback slot is not Song B");
  require(engine.songModeEnabled(), "live song mode is not enabled");

  if (requestedBars == 8) {
    for (uint8_t bar = 0; bar < 4; ++bar) {
      require(sameDrums(scene.drumBanks[kAuditionBank].patterns[bar],
                        expectedDrums[bar]),
              "8-bar first semantic segment lost physical linkage");
    }
    for (uint8_t bar = 4; bar < 8; ++bar) {
      require(sameDrums(scene.drumBanks[kAuditionBank].patterns[bar],
                        expectedDrums[bar]),
              "8-bar second semantic segment lost physical linkage");
    }
    std::puts(
        "EIGHT-BAR-PHYSICAL firstRows=0..3 secondRows=4..7 songBRows=8 "
        "semantic_to_live=true");
  }

  std::printf(
      "LIVE requested=%u status=%s selection=%u archetype=%u "
      "songLength=%d loop=%d..%d position=%d activeSlot=%d playbackSlot=%d "
      "songMode=%s fallback=false\n",
      static_cast<unsigned>(requestedBars),
      phraseAuditionStatusName(live.status),
      static_cast<unsigned>(live.selectionStatus),
      static_cast<unsigned>(live.archetypeId),
      engine.songLength(),
      engine.loopStartRow(),
      engine.loopEndRow(),
      engine.currentSongPosition(),
      engine.activeSongSlot(),
      engine.songPlaybackSlot(),
      engine.songModeEnabled() ? "true" : "false");
}

void characterizeSelectionFailureRestore(MiniAcid& engine) {
  configureFixture(engine, 4);
  Scene& scene = engine.sceneManager().currentScene();

  DrumPatternSet& sentinelDrums = scene.drumBanks[kAuditionBank].patterns[0];
  sentinelDrums.voices[0].steps[0].hit = true;
  sentinelDrums.voices[0].steps[0].accent = true;
  sentinelDrums.voices[0].steps[0].velocity = 77;
  SynthPattern& sentinelSynthA = scene.synthABanks[kAuditionBank].patterns[0];
  sentinelSynthA.steps[0].note = 42;
  sentinelSynthA.steps[0].velocity = 66;
  SynthPattern& sentinelSynthB = scene.synthBBanks[kAuditionBank].patterns[0];
  sentinelSynthB.steps[4].note = 47;
  sentinelSynthB.steps[4].velocity = 65;

  scene.songs[kAuditionSongSlot].length = 3;
  scene.songs[kAuditionSongSlot].positions[0].patterns[0] = 91;
  scene.songs[kAuditionSongSlot].positions[0].patterns[1] = 92;
  scene.songs[kAuditionSongSlot].positions[0].patterns[2] = 93;

  engine.setActiveSongSlot(0);
  engine.setSongLength(6);
  engine.setSongPosition(3);
  engine.setLoopRange(1, 4);
  engine.setLoopMode(true);
  engine.setSongPlaybackSlot(0);
  engine.setSongMode(true);
  engine.setDrumBankIndex(0);
  engine.setDrumPatternIndex(2);
  engine.set303BankIndex(0, 0);
  engine.set303BankIndex(1, 0);
  engine.set303PatternIndex(0, 3);
  engine.set303PatternIndex(1, 4);

  const DrumPatternSet beforeDrums = sentinelDrums;
  const SynthPattern beforeSynthA = sentinelSynthA;
  const SynthPattern beforeSynthB = sentinelSynthB;
  const Song beforeSongB = scene.songs[kAuditionSongSlot];
  const bool beforeSongMode = engine.songModeEnabled();
  const bool beforeLoopMode = engine.loopModeEnabled();
  const int beforeLoopStart = engine.loopStartRow();
  const int beforeLoopEnd = engine.loopEndRow();
  const int beforeActiveSlot = engine.activeSongSlot();
  const int beforePlaybackSlot = engine.songPlaybackSlot();
  const int beforePosition = engine.currentSongPosition();
  const int beforeDrumBank = engine.currentDrumBankIndex();
  const int beforeDrumPattern = engine.currentDrumPatternIndex();
  const int beforeSynthBankA = engine.current303BankIndex(0);
  const int beforeSynthBankB = engine.current303BankIndex(1);
  const int beforeSynthPatternA = engine.current303PatternIndex(0);
  const int beforeSynthPatternB = engine.current303PatternIndex(1);

  scene.genre.generativeMode = 0xFFu;
  const PhraseAuditionResult result = regeneratePhraseAuditionWithProbe(engine);
  require(result.status == PhraseAuditionStatus::SelectionFailed,
          "invalid route did not report SelectionFailed");
  require(result.selectionStatus == StrongRhythmMigrationStatus::Legacy,
          "invalid route selection status is not Legacy");

  require(sameDrums(beforeDrums, sentinelDrums),
          "selection failure mutated Bank B drums");
  require(sameSynth(beforeSynthA, sentinelSynthA),
          "selection failure mutated Bank B SynthA");
  require(sameSynth(beforeSynthB, sentinelSynthB),
          "selection failure mutated Bank B SynthB");
  require(sameSong(beforeSongB, scene.songs[kAuditionSongSlot]),
          "selection failure mutated Song B");
  require(engine.songModeEnabled() == beforeSongMode &&
              engine.loopModeEnabled() == beforeLoopMode &&
              engine.loopStartRow() == beforeLoopStart &&
              engine.loopEndRow() == beforeLoopEnd &&
              engine.activeSongSlot() == beforeActiveSlot &&
              engine.songPlaybackSlot() == beforePlaybackSlot &&
              engine.currentSongPosition() == beforePosition &&
              engine.currentDrumBankIndex() == beforeDrumBank &&
              engine.currentDrumPatternIndex() == beforeDrumPattern &&
              engine.current303BankIndex(0) == beforeSynthBankA &&
              engine.current303BankIndex(1) == beforeSynthBankB &&
              engine.current303PatternIndex(0) == beforeSynthPatternA &&
              engine.current303PatternIndex(1) == beforeSynthPatternB,
          "selection failure changed bounded playback/selection state");

  std::puts(
      "LIVE-FAILURE status=SELECT_FAIL bankB_unchanged=true songB_unchanged=true "
      "transport_unchanged=true");
  std::puts(
      "MATERIALIZATION-FAILURE not_forced=true reason=no_existing_fault_injection_seam");
}

}  // namespace

int main() {
  MiniAcid engine(kSampleRate, nullptr);
  engine.init();

  characterizeLiveSuccess(engine, 2);
  characterizeLiveSuccess(engine, 4);
  characterizeLiveSuccess(engine, 8);
  characterizeSelectionFailureRestore(engine);

  std::puts("E1B LIVE-BRIDGE CHARACTERIZATION PASS");
  return 0;
}
