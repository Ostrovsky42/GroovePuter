#include "../platform_sdl/arduino_compat.h"
#include "../scenes.h"
#include "../src/phrase/phrase_core.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <string>

SerialMock Serial;
SDMock SD;

namespace {

bool near(float actual, float expected, float epsilon = 0.0001f) {
  return std::fabs(actual - expected) <= epsilon;
}

std::string extractSynthState(const std::string& json) {
  const std::string beginToken = "\"synthState\":";
  const std::string endToken = ",\"synthDistortion\":";
  const size_t begin = json.find(beginToken);
  assert(begin != std::string::npos);
  const size_t end = json.find(endToken, begin);
  assert(end != std::string::npos);
  return json.substr(begin, end - begin);
}

void populateNonDefaultScene(SceneManager& manager) {
  manager.loadDefaultScene();
  manager.setBpm(137.5f);
  manager.setActiveSongSlot(1);
  manager.setTrackVolume(static_cast<int>(VoiceId::SynthA), 0.37f);

  PersistedSynthPatch synthA;
  synthA.engineName = "SH101";
  synthA.paramCount = 6;
  PersistedSynthPatch synthB;
  synthB.engineName = "WAVEMORPH";
  synthB.paramCount = 6;
  for (int i = 0; i < PersistedSynthPatch::kMaxParams; ++i) {
    synthA.params[i] = 0.10f + static_cast<float>(i) * 0.11f;
    synthB.params[i] = 0.85f - static_cast<float>(i) * 0.09f;
  }
  manager.setSynthPatch(0, synthA);
  manager.setSynthPatch(1, synthB);
  manager.setSynthEngineName(0, synthA.engineName);
  manager.setSynthEngineName(1, synthB.engineName);

  Scene& scene = manager.currentScene();
  scene.feel.gridSteps = 32;
  scene.feel.timebase = 2;
  scene.feel.patternBars = 4;
  scene.feel.swingPct = 63;
  scene.feel.swingMask = 0x0355;
  scene.feel.timingProfile = static_cast<uint8_t>(
      GroovePuterRhythm::FeelProfileId::PushPullControlled);
  scene.feel.lofiEnabled = true;
  scene.feel.lofiAmount = 74;
  scene.feel.driveEnabled = true;
  scene.feel.driveAmount = 81;
  scene.feel.tapeEnabled = true;
  scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Electro);
  scene.genre.recipe = kBaseRecipeId;
  scene.genre.rhythmSelectionMode = static_cast<uint8_t>(
      GroovePuterRhythm::RhythmSelectionMode::Manual);
  scene.genre.rhythmArchetypeId = 712;

  scene.generatorParams.minNotes = 3;
  scene.generatorParams.maxNotes = 11;
  scene.generatorParams.minOctave = 29;
  scene.generatorParams.maxOctave = 71;
  scene.generatorParams.swingAmount = 0.41f;
  scene.generatorParams.velocityRange = 0.58f;
  scene.generatorParams.ghostNoteProbability = 0.27f;
  scene.generatorParams.microTimingAmount = 0.36f;
  scene.generatorParams.preferDownbeats = true;
  scene.generatorParams.scaleQuantize = false;
  scene.generatorParams.scaleRoot = 7;
  scene.generatorParams.scale = MIXOLYDIAN;

  DrumStep& drum = scene.drumBanks[0].patterns[0].voices[0].steps[3];
  drum.hit = true;
  drum.accent = true;
  drum.velocity = 47;
  drum.timing = -11;
  drum.probability = 83;
  drum.fx = 2;
  drum.fxParam = 5;

  SynthStep& synth = scene.synthABanks[0].patterns[0].steps[5];
  synth.note = 61;
  synth.slide = true;
  synth.accent = true;
  synth.ghost = true;
  synth.velocity = 72;
  synth.timing = 13;
  synth.probability = 76;
  synth.fx = 1;
  synth.fxParam = 4;

  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    SynthStep& held = scene.synthBBanks[0].patterns[0].steps[step];
    held.note = 55;
    held.slide = step != 0;
    held.velocity = 68;
  }

  std::strncpy(scene.customPhrases[0], "line one\nline\ttwo",
               Scene::kMaxPhraseLength - 1);
  scene.customPhrases[0][Scene::kMaxPhraseLength - 1] = '\0';

  Song& phraseSource = scene.songs[0];
  phraseSource.length = 4;
  for (int row = 0; row < 4; ++row) {
    phraseSource.positions[row].patterns[0] = static_cast<int16_t>(10 + row);
    phraseSource.positions[row].patterns[1] = static_cast<int16_t>(20 + row);
    phraseSource.positions[row].patterns[2] = static_cast<int16_t>(30 + row);
    phraseSource.positions[row].patterns[3] = -1;
  }

  PhraseCore::reset(scene.phraseBank);
  const PhraseCore::Result captured = PhraseCore::captureSongRegion(
      scene.phraseBank, PhraseCore::SlotId::A, phraseSource, 0, 0, 4,
      PhraseCore::Role::Main, PhraseCore::Source::InternalPattern);
  assert(captured);
  assert(captured.phraseId == 1);

  const PhraseCore::Result derived = PhraseCore::deriveReferenceView(
      scene.phraseBank, PhraseCore::SlotId::B, PhraseCore::SlotId::A,
      PhraseCore::Role::Variation);
  assert(derived);
  assert(derived.phraseId == 2);
}

void destroyRoundTripFields(SceneManager& manager) {
  manager.setBpm(60.0f);
  manager.setActiveSongSlot(0);
  manager.setTrackVolume(static_cast<int>(VoiceId::SynthA), 1.0f);

  Scene& scene = manager.currentScene();
  scene.feel = FeelSettings();
  scene.genre = GenreSettings();
  scene.generatorParams = GeneratorParams();
  scene.drumBanks[0].patterns[0].voices[0].steps[3] = DrumStep();
  scene.synthABanks[0].patterns[0].steps[5] = SynthStep();
  scene.customPhrases[0][0] = '\0';
  PhraseCore::reset(scene.phraseBank);
}

void verifyRoundTrip(const SceneManager& manager) {
  const Scene& scene = manager.currentScene();
  assert(near(manager.getBpm(), 137.5f));
  assert(manager.activeSongSlot() == 1);
  assert(near(manager.getTrackVolume(static_cast<int>(VoiceId::SynthA)), 0.37f));
  assert(manager.hasVersionedSynthState());
  const PersistedSynthPatch& synthA = manager.getSynthPatch(0);
  const PersistedSynthPatch& synthB = manager.getSynthPatch(1);
  assert(synthA.engineName == "SH101");
  assert(synthB.engineName == "WAVEMORPH");
  assert(synthA.paramCount == 6);
  assert(synthB.paramCount == 6);
  for (int i = 0; i < PersistedSynthPatch::kMaxParams; ++i) {
    assert(near(synthA.params[i], 0.10f + static_cast<float>(i) * 0.11f));
    assert(near(synthB.params[i], 0.85f - static_cast<float>(i) * 0.09f));
  }

  assert(scene.feel.gridSteps == 32);
  assert(scene.feel.timebase == 2);
  assert(scene.feel.patternBars == 4);
  assert(scene.feel.swingPct == 63);
  assert(scene.feel.swingMask == 0x0355);
  assert(scene.feel.timingProfile == static_cast<uint8_t>(
             GroovePuterRhythm::FeelProfileId::PushPullControlled));
  assert(scene.feel.lofiEnabled);
  assert(scene.feel.lofiAmount == 74);
  assert(scene.feel.driveEnabled);
  assert(scene.feel.driveAmount == 81);
  assert(scene.feel.tapeEnabled);
  assert(scene.genre.generativeMode ==
         static_cast<uint8_t>(GenerativeMode::Electro));
  assert(scene.genre.rhythmSelectionMode == static_cast<uint8_t>(
             GroovePuterRhythm::RhythmSelectionMode::Manual));
  assert(scene.genre.rhythmArchetypeId == 712);

  assert(scene.generatorParams.minNotes == 3);
  assert(scene.generatorParams.maxNotes == 11);
  assert(scene.generatorParams.minOctave == 29);
  assert(scene.generatorParams.maxOctave == 71);
  assert(near(scene.generatorParams.swingAmount, 0.41f));
  assert(near(scene.generatorParams.velocityRange, 0.58f));
  assert(near(scene.generatorParams.ghostNoteProbability, 0.27f));
  assert(near(scene.generatorParams.microTimingAmount, 0.36f));
  assert(scene.generatorParams.preferDownbeats);
  assert(!scene.generatorParams.scaleQuantize);
  assert(scene.generatorParams.scaleRoot == 7);
  assert(scene.generatorParams.scale == MIXOLYDIAN);

  const DrumStep& drum = scene.drumBanks[0].patterns[0].voices[0].steps[3];
  assert(drum.hit);
  assert(drum.accent);
  assert(drum.velocity == 47);
  assert(drum.timing == -11);
  assert(drum.probability == 83);
  assert(drum.fx == 2);
  assert(drum.fxParam == 5);

  const SynthStep& synth = scene.synthABanks[0].patterns[0].steps[5];
  assert(synth.note == 61);
  assert(synth.slide);
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& held = scene.synthBBanks[0].patterns[0].steps[step];
    assert(held.note == 55);
    assert(held.slide == (step != 0));
  }
  assert(synth.accent);
  assert(synth.ghost);
  assert(synth.velocity == 72);
  assert(synth.timing == 13);
  assert(synth.probability == 76);
  assert(synth.fx == 1);
  assert(synth.fxParam == 4);

  assert(std::string(scene.customPhrases[0]) == "line one\nline\ttwo");

  const PhraseCore::SlotSummary phraseA =
      PhraseCore::summarize(scene.phraseBank, PhraseCore::SlotId::A);
  const PhraseCore::SlotSummary phraseB =
      PhraseCore::summarize(scene.phraseBank, PhraseCore::SlotId::B);
  assert(phraseA.valid);
  assert(phraseA.phraseId == 1);
  assert(phraseA.parentId == PhraseCore::kNoPhraseId);
  assert(phraseA.lengthBars == 4);
  assert(phraseA.role == PhraseCore::Role::Main);
  assert(phraseA.source == PhraseCore::Source::InternalPattern);
  assert(phraseA.storage == PhraseCore::StorageMode::ReferenceView);
  assert(phraseA.mutableBacking);
  assert(phraseA.trackMask == PhraseCore::kAllTracks);

  assert(phraseB.valid);
  assert(phraseB.phraseId == 2);
  assert(phraseB.parentId == phraseA.phraseId);
  assert(phraseB.lengthBars == 4);
  assert(phraseB.role == PhraseCore::Role::Variation);
  assert(phraseB.source == PhraseCore::Source::Derived);
  assert(phraseB.storage == PhraseCore::StorageMode::ReferenceView);
  assert(phraseB.mutableBacking);

  assert(PhraseCore::patternAt(scene.phraseBank.slots[0], 0, SongTrack::SynthA) == 10);
  assert(PhraseCore::patternAt(scene.phraseBank.slots[0], 3, SongTrack::SynthB) == 23);
  assert(PhraseCore::patternAt(scene.phraseBank.slots[1], 2, SongTrack::Drums) == 32);
  assert(scene.phraseBank.nextPhraseId == 3);
  assert(scene.phraseBank.version == PhraseCore::kPersistenceVersion);
}

}  // namespace

int main() {
  SceneManager manager;
  populateNonDefaultScene(manager);

  const std::string json = manager.dumpCurrentScene();
  assert(!json.empty());
  assert(json.find("\\n") != std::string::npos);
  assert(json.find("\\t") != std::string::npos);
  assert(json.find("\"vel\":[") != std::string::npos);
  assert(json.find("\"tim\":[") != std::string::npos);
  assert(json.find("\"ghost\":true") != std::string::npos);
  assert(json.find("\"swing\":63") != std::string::npos);
  assert(json.find("\"mask\":853") != std::string::npos);
  assert(json.find("\"profile\":3") != std::string::npos);
  assert(json.find("\"phraseCore\":[") != std::string::npos);
  assert(json.find("\"synthState\":{\"version\":1") != std::string::npos);
  assert(json.find("\"synthParams\"") == std::string::npos);
  assert(json.find("\"rsm\":1") != std::string::npos);
  assert(json.find("\"rid\":712") != std::string::npos);

  const std::string stableSynthState = extractSynthState(json);
  destroyRoundTripFields(manager);
  assert(manager.loadScene(json));
  verifyRoundTrip(manager);
  const std::string secondJson = manager.dumpCurrentScene();
  assert(extractSynthState(secondJson) == stableSynthState);

  // Legacy documents have no rhythm intent fields and decode as AUTO.
  std::string legacy = json;
  const std::string rhythmFields = ",\"rsm\":1,\"rid\":712";
  const size_t rhythmPos = legacy.find(rhythmFields);
  assert(rhythmPos != std::string::npos);
  legacy.erase(rhythmPos, rhythmFields.size());
  assert(manager.loadScene(legacy));
  assert(manager.currentScene().genre.rhythmSelectionMode == static_cast<uint8_t>(
             GroovePuterRhythm::RhythmSelectionMode::Auto));
  assert(manager.currentScene().genre.rhythmArchetypeId == 0);

  // Legacy documents have no Feel profile and decode as Straight.
  std::string legacyFeel = json;
  const std::string feelProfile = ",\"profile\":3";
  const size_t feelProfilePos = legacyFeel.find(feelProfile);
  assert(feelProfilePos != std::string::npos);
  legacyFeel.erase(feelProfilePos, feelProfile.size());
  assert(manager.loadScene(legacyFeel));
  assert(manager.currentScene().feel.timingProfile == static_cast<uint8_t>(
             GroovePuterRhythm::FeelProfileId::Straight));

  assert(manager.loadScene(json));
  verifyRoundTrip(manager);

  // Unknown version is transactional: current state must remain intact.
  std::string malformed = json;
  const std::string version1 = "\"synthState\":{\"version\":1";
  const size_t versionPos = malformed.find(version1);
  assert(versionPos != std::string::npos);
  malformed.replace(versionPos, version1.size(),
                    "\"synthState\":{\"version\":99");
  assert(!manager.loadScene(malformed));
  verifyRoundTrip(manager);
  assert(extractSynthState(manager.dumpCurrentScene()) == stableSynthState);
  return 0;
}
