#include "ArduinoJson-v7.4.2.h"
#include "scenes.h"

#include <memory>

namespace {
int clampIndex(int value, int maxExclusive) {
  if (value < 0) return 0;
  if (value >= maxExclusive) return maxExclusive - 1;
  return value;
}

void clearDrumPattern(DrumPattern& pattern) {
  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    pattern.steps[i].hit = false;
    pattern.steps[i].accent = false;
    pattern.steps[i].fx = 0;
    pattern.steps[i].fxParam = 0;
  }
}

void clearSynthPattern(SynthPattern& pattern) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i].note = -1;
    pattern.steps[i].slide = false;
    pattern.steps[i].accent = false;
    pattern.steps[i].fx = 0;
    pattern.steps[i].fxParam = 0;
  }
}

void clearSong(Song& song) {
  for (int i = 0; i < Song::kMaxPositions; ++i) {
    for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      song.positions[i].patterns[t] = -1;
    }
  }
}

void clearCustomPhrases(Scene& scene) {
  for (int i = 0; i < Scene::kMaxCustomPhrases; ++i) {
    scene.customPhrases[i][0] = '\0';
  }
}

void clearSceneData(Scene& scene) {
  for (int b = 0; b < kBankCount; ++b) {
    for (int p = 0; p < Bank<DrumPatternSet>::kPatterns; ++p) {
      for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
        clearDrumPattern(scene.drumBanks[b].patterns[p].voices[v]);
      }
    }
    for (int p = 0; p < Bank<SynthPattern>::kPatterns; ++p) {
      clearSynthPattern(scene.synthABanks[b].patterns[p]);
      clearSynthPattern(scene.synthBBanks[b].patterns[p]);
    }
  }
  clearSong(scene.song);
  clearCustomPhrases(scene);
  scene.masterVolume = 0.6f;
  scene.generatorParams = GeneratorParams();
  scene.led = LedSettings();
  scene.tape = TapeState();
}

void serializeDrumPattern(const DrumPattern& pattern, ArduinoJson::JsonObject obj) {
  ArduinoJson::JsonArray hit = obj["hit"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray accent = obj["accent"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray fx = obj["fx"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray fxp = obj["fxp"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    hit.add(pattern.steps[i].hit);
    accent.add(pattern.steps[i].accent);
    fx.add(pattern.steps[i].fx);
    fxp.add(pattern.steps[i].fxParam);
  }
}

void serializeDrumBank(const Bank<DrumPatternSet>& bank, ArduinoJson::JsonArray patterns) {
  for (int p = 0; p < Bank<DrumPatternSet>::kPatterns; ++p) {
    ArduinoJson::JsonArray voices = patterns.add<ArduinoJson::JsonArray>();
    for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
      ArduinoJson::JsonObject voice = voices.add<ArduinoJson::JsonObject>();
      serializeDrumPattern(bank.patterns[p].voices[v], voice);
    }
  }
}

void serializeDrumBanks(const Bank<DrumPatternSet>* banks, ArduinoJson::JsonArray banksArr) {
  for (int b = 0; b < kBankCount; ++b) {
    ArduinoJson::JsonArray patterns = banksArr.add<ArduinoJson::JsonArray>();
    serializeDrumBank(banks[b], patterns);
  }
}

void serializeSynthPattern(const SynthPattern& pattern, ArduinoJson::JsonArray steps) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    ArduinoJson::JsonObject step = steps.add<ArduinoJson::JsonObject>();
    step["note"] = pattern.steps[i].note;
    step["slide"] = pattern.steps[i].slide;
    step["accent"] = pattern.steps[i].accent;
    step["fx"] = pattern.steps[i].fx;
    step["fxp"] = pattern.steps[i].fxParam;
  }
}

void serializeSynthBank(const Bank<SynthPattern>& bank, ArduinoJson::JsonArray patterns) {
  for (int p = 0; p < Bank<SynthPattern>::kPatterns; ++p) {
    ArduinoJson::JsonArray steps = patterns.add<ArduinoJson::JsonArray>();
    serializeSynthPattern(bank.patterns[p], steps);
  }
}

void serializeSynthBanks(const Bank<SynthPattern>* banks, ArduinoJson::JsonArray banksArr) {
  for (int b = 0; b < kBankCount; ++b) {
    ArduinoJson::JsonArray patterns = banksArr.add<ArduinoJson::JsonArray>();
    serializeSynthBank(banks[b], patterns);
  }
}

bool deserializeBoolArray(ArduinoJson::JsonArrayConst arr, bool* dst, int expectedSize) {
  if (static_cast<int>(arr.size()) != expectedSize) return false;
  int idx = 0;
  for (ArduinoJson::JsonVariantConst value : arr) {
    if (!value.is<bool>()) return false;
    dst[idx++] = value.as<bool>();
  }
  return true;
}

bool deserializeDrumPattern(ArduinoJson::JsonVariantConst value, DrumPattern& pattern) {
  ArduinoJson::JsonObjectConst obj = value.as<ArduinoJson::JsonObjectConst>();
  if (obj.isNull()) return false;
  ArduinoJson::JsonArrayConst hit = obj["hit"].as<ArduinoJson::JsonArrayConst>();
  ArduinoJson::JsonArrayConst accent = obj["accent"].as<ArduinoJson::JsonArrayConst>();
  ArduinoJson::JsonArrayConst fx = obj["fx"].as<ArduinoJson::JsonArrayConst>();
  ArduinoJson::JsonArrayConst fxp = obj["fxp"].as<ArduinoJson::JsonArrayConst>();
  if (hit.isNull() || accent.isNull()) return false;
  bool hits[DrumPattern::kSteps];
  bool accents[DrumPattern::kSteps];
  if (!deserializeBoolArray(hit, hits, DrumPattern::kSteps)) return false;
  if (!deserializeBoolArray(accent, accents, DrumPattern::kSteps)) return false;
  
  // Optional FX arrays
  int fxs[DrumPattern::kSteps] = {0};
  int fxps[DrumPattern::kSteps] = {0};
  if (!fx.isNull()) {
      int idx = 0;
      for (ArduinoJson::JsonVariantConst v : fx) { if (idx < DrumPattern::kSteps) fxs[idx++] = v.as<int>(); }
  }
  if (!fxp.isNull()) {
      int idx = 0;
      for (ArduinoJson::JsonVariantConst v : fxp) { if (idx < DrumPattern::kSteps) fxps[idx++] = v.as<int>(); }
  }

  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    pattern.steps[i].hit = hits[i];
    pattern.steps[i].accent = accents[i];
    pattern.steps[i].fx = static_cast<uint8_t>(fxs[i]);
    pattern.steps[i].fxParam = static_cast<uint8_t>(fxps[i]);
  }
  return true;
}

bool deserializeDrumPatternSet(ArduinoJson::JsonVariantConst value, DrumPatternSet& patternSet) {
  ArduinoJson::JsonArrayConst voices = value.as<ArduinoJson::JsonArrayConst>();
  if (voices.isNull() || static_cast<int>(voices.size()) != DrumPatternSet::kVoices) return false;
  int v = 0;
  for (ArduinoJson::JsonVariantConst voice : voices) {
    if (!deserializeDrumPattern(voice, patternSet.voices[v])) return false;
    ++v;
  }
  return true;
}

bool deserializeDrumBank(ArduinoJson::JsonVariantConst value, Bank<DrumPatternSet>& bank) {
  ArduinoJson::JsonArrayConst patterns = value.as<ArduinoJson::JsonArrayConst>();
  if (patterns.isNull() || static_cast<int>(patterns.size()) != Bank<DrumPatternSet>::kPatterns) return false;
  int p = 0;
  for (ArduinoJson::JsonVariantConst pattern : patterns) {
    if (!deserializeDrumPatternSet(pattern, bank.patterns[p])) return false;
    ++p;
  }
  return true;
}

bool deserializeDrumBanks(ArduinoJson::JsonVariantConst value, Bank<DrumPatternSet>* banks) {
  ArduinoJson::JsonArrayConst banksArr = value.as<ArduinoJson::JsonArrayConst>();
  if (banksArr.isNull()) return false;
  if (static_cast<int>(banksArr.size()) == Bank<DrumPatternSet>::kPatterns) {
    return deserializeDrumBank(value, banks[0]);
  }
  if (static_cast<int>(banksArr.size()) != kBankCount) return false;
  int b = 0;
  for (ArduinoJson::JsonVariantConst bankVal : banksArr) {
    if (!deserializeDrumBank(bankVal, banks[b])) return false;
    ++b;
  }
  return true;
}

bool deserializeSynthPattern(ArduinoJson::JsonVariantConst value, SynthPattern& pattern) {
  ArduinoJson::JsonArrayConst steps = value.as<ArduinoJson::JsonArrayConst>();
  if (steps.isNull() || static_cast<int>(steps.size()) != SynthPattern::kSteps) return false;
  int i = 0;
  for (ArduinoJson::JsonVariantConst stepValue : steps) {
    ArduinoJson::JsonObjectConst obj = stepValue.as<ArduinoJson::JsonObjectConst>();
    if (obj.isNull()) return false;
    auto note = obj["note"];
    auto slide = obj["slide"];
    auto accent = obj["accent"];
    auto fx = obj["fx"];
    auto fxp = obj["fxp"];
    if (!note.is<int>() || !slide.is<bool>() || !accent.is<bool>()) return false;
    pattern.steps[i].note = note.as<int>();
    pattern.steps[i].slide = slide.as<bool>();
    pattern.steps[i].accent = accent.as<bool>();
    if (!fx.isNull()) pattern.steps[i].fx = static_cast<uint8_t>(fx.as<int>());
    else pattern.steps[i].fx = 0;
    if (!fxp.isNull()) pattern.steps[i].fxParam = static_cast<uint8_t>(fxp.as<int>());
    else pattern.steps[i].fxParam = 0;
    ++i;
  }
  return true;
}

bool deserializeSynthBank(ArduinoJson::JsonVariantConst value, Bank<SynthPattern>& bank) {
  ArduinoJson::JsonArrayConst patterns = value.as<ArduinoJson::JsonArrayConst>();
  if (patterns.isNull() || static_cast<int>(patterns.size()) != Bank<SynthPattern>::kPatterns) return false;
  int p = 0;
  for (ArduinoJson::JsonVariantConst pattern : patterns) {
    if (!deserializeSynthPattern(pattern, bank.patterns[p])) return false;
    ++p;
  }
  return true;
}

bool deserializeSynthBanks(ArduinoJson::JsonVariantConst value, Bank<SynthPattern>* banks) {
  ArduinoJson::JsonArrayConst banksArr = value.as<ArduinoJson::JsonArrayConst>();
  if (banksArr.isNull()) return false;
  if (static_cast<int>(banksArr.size()) == Bank<SynthPattern>::kPatterns) {
    return deserializeSynthBank(value, banks[0]);
  }
  if (static_cast<int>(banksArr.size()) != kBankCount) return false;
  int b = 0;
  for (ArduinoJson::JsonVariantConst bankVal : banksArr) {
    if (!deserializeSynthBank(bankVal, banks[b])) return false;
    ++b;
  }
  return true;
}
int valueToInt(ArduinoJson::JsonVariantConst value, int defaultValue) {
  if (value.is<int>()) {
    return value.as<int>();
  }
  return defaultValue;
}

float valueToFloat(ArduinoJson::JsonVariantConst value, float defaultValue) {
  if (value.is<float>() || value.is<int>()) {
    return value.as<float>();
  }
  return defaultValue;
}

bool deserializeSynthParameters(ArduinoJson::JsonVariantConst value, SynthParameters& params) {
  ArduinoJson::JsonObjectConst obj = value.as<ArduinoJson::JsonObjectConst>();
  if (obj.isNull()) return false;

  auto cutoff = obj["cutoff"];
  auto resonance = obj["resonance"];
  auto envAmount = obj["envAmount"];
  auto envDecay = obj["envDecay"];
  auto oscType = obj["oscType"];

  if (!cutoff.isNull()) {
    if (!cutoff.is<float>() && !cutoff.is<int>()) return false;
    params.cutoff = valueToFloat(cutoff, params.cutoff);
  }
  if (!resonance.isNull()) {
    if (!resonance.is<float>() && !resonance.is<int>()) return false;
    params.resonance = valueToFloat(resonance, params.resonance);
  }
  if (!envAmount.isNull()) {
    if (!envAmount.is<float>() && !envAmount.is<int>()) return false;
    params.envAmount = valueToFloat(envAmount, params.envAmount);
  }
  if (!envDecay.isNull()) {
    if (!envDecay.is<float>() && !envDecay.is<int>()) return false;
    params.envDecay = valueToFloat(envDecay, params.envDecay);
  }
  if (!oscType.isNull()) {
    if (!oscType.is<int>()) return false;
    params.oscType = oscType.as<int>();
  }

  return true;
}

void serializeGeneratorParams(const GeneratorParams& params, ArduinoJson::JsonObject obj) {
  obj["minNotes"] = params.minNotes;
  obj["maxNotes"] = params.maxNotes;
  obj["minOctave"] = params.minOctave;
  obj["maxOctave"] = params.maxOctave;
  obj["swingAmount"] = params.swingAmount;
  obj["velocityRange"] = params.velocityRange;
  obj["ghostNoteProbability"] = params.ghostNoteProbability;
  obj["microTimingAmount"] = params.microTimingAmount;
  obj["preferDownbeats"] = params.preferDownbeats;
  obj["scaleQuantize"] = params.scaleQuantize;
  obj["scaleRoot"] = params.scaleRoot;
  obj["scale"] = static_cast<int>(params.scale);
}

bool deserializeGeneratorParams(ArduinoJson::JsonObjectConst obj, GeneratorParams& params) {
  if (obj.isNull()) return false;
  params.minNotes = valueToInt(obj["minNotes"], params.minNotes);
  params.maxNotes = valueToInt(obj["maxNotes"], params.maxNotes);
  params.minOctave = valueToInt(obj["minOctave"], params.minOctave);
  params.maxOctave = valueToInt(obj["maxOctave"], params.maxOctave);
  params.swingAmount = valueToFloat(obj["swingAmount"], params.swingAmount);
  params.velocityRange = valueToFloat(obj["velocityRange"], params.velocityRange);
  params.ghostNoteProbability = valueToFloat(obj["ghostNoteProbability"], params.ghostNoteProbability);
  params.microTimingAmount = valueToFloat(obj["microTimingAmount"], params.microTimingAmount);
  if (obj["preferDownbeats"].is<bool>()) params.preferDownbeats = obj["preferDownbeats"].as<bool>();
  if (obj["scaleQuantize"].is<bool>()) params.scaleQuantize = obj["scaleQuantize"].as<bool>();
  params.scaleRoot = valueToInt(obj["scaleRoot"], params.scaleRoot);
  params.scale = static_cast<ScaleType>(valueToInt(obj["scale"], static_cast<int>(params.scale)));
  return true;
}

void serializeLedSettings(const LedSettings& led, ArduinoJson::JsonObject obj) {
  obj["mode"] = static_cast<int>(led.mode);
  obj["src"] = static_cast<int>(led.source);
  ArduinoJson::JsonArray clr = obj["clr"].to<ArduinoJson::JsonArray>();
  clr.add(led.color.r);
  clr.add(led.color.g);
  clr.add(led.color.b);
  obj["bri"] = led.brightness;
  obj["fls"] = led.flashMs;
}

bool deserializeLedSettings(ArduinoJson::JsonObjectConst obj, LedSettings& led) {
  if (obj.isNull()) return false;
  led.mode = static_cast<LedMode>(valueToInt(obj["mode"], static_cast<int>(led.mode)));
  led.source = static_cast<LedSource>(valueToInt(obj["src"], static_cast<int>(led.source)));
  ArduinoJson::JsonArrayConst clr = obj["clr"].as<ArduinoJson::JsonArrayConst>();
  if (!clr.isNull() && clr.size() >= 3) {
    led.color.r = clr[0].as<uint8_t>();
    led.color.g = clr[1].as<uint8_t>();
    led.color.b = clr[2].as<uint8_t>();
  }
  led.brightness = static_cast<uint8_t>(valueToInt(obj["bri"], led.brightness));
  led.flashMs = static_cast<uint16_t>(valueToInt(obj["fls"], led.flashMs));
  return true;
}

}

SceneJsonObserver::SceneJsonObserver(Scene& scene, float defaultBpm)
    : target_(scene), bpm_(defaultBpm) {
  clearSong(song_);
  for (int i = 0; i < (int)VoiceId::Count; ++i) {
    target_.trackVolumes[i] = 1.0f;
  }
}

SceneJsonObserver::Path SceneJsonObserver::deduceArrayPath(const Context& parent) const {
  switch (parent.path) {
  case Path::DrumBanks:
    return Path::DrumBank;
  case Path::DrumBank:
    return Path::DrumPatternSet;
  case Path::SynthABanks:
    return Path::SynthABank;
  case Path::SynthABank:
    return Path::SynthPattern;
  case Path::SynthBBanks:
    return Path::SynthBBank;
  case Path::SynthBBank:
    return Path::SynthPattern;
  case Path::SynthParams:
    return Path::SynthParam;
  case Path::SynthDistortion:
    return Path::SynthDistortion;
  case Path::SynthDelay:
    return Path::SynthDelay;
  case Path::SamplerPads:
    return Path::SamplerPad;
  case Path::Song:
    return Path::SongPosition;
  case Path::Root:
    return Path::CustomPhrase;
  default:
    return Path::Unknown;
  }
}

SceneJsonObserver::Path SceneJsonObserver::deduceObjectPath(const Context& parent) const {
  switch (parent.path) {
  case Path::DrumPatternSet:
    return Path::DrumVoice;
  case Path::SynthPattern:
    return Path::SynthStep;
  case Path::SynthParams:
    return Path::SynthParam;
  case Path::SongPositions:
    return Path::SongPosition;
  case Path::SamplerPads:
    return Path::SamplerPad;
  default:
    return Path::Unknown;
  }
}

int SceneJsonObserver::currentIndexFor(Path path) const {
  for (int i = stackSize_ - 1; i >= 0; --i) {
    if (stack_[i].path == path && stack_[i].type == Context::Type::Array) {
      return stack_[i].index;
    }
  }
  return -1;
}

bool SceneJsonObserver::inSynthBankA() const {
  for (int i = stackSize_ - 1; i >= 0; --i) {
    if (stack_[i].path == Path::SynthABanks || stack_[i].path == Path::SynthABank) return true;
    if (stack_[i].path == Path::SynthBBanks || stack_[i].path == Path::SynthBBank) return false;
  }
  return true;
}

bool SceneJsonObserver::inSynthBankB() const {
  for (int i = stackSize_ - 1; i >= 0; --i) {
    if (stack_[i].path == Path::SynthBBanks || stack_[i].path == Path::SynthBBank) return true;
    if (stack_[i].path == Path::SynthABanks || stack_[i].path == Path::SynthABank) return false;
  }
  return false;
}

void SceneJsonObserver::pushContext(Context::Type type, Path path) {
  if (stackSize_ >= kMaxStack) {
    error_ = true;
    return;
  }
  stack_[stackSize_++] = Context{type, path, 0};
}

void SceneJsonObserver::popContext() {
  if (stackSize_ == 0) {
    error_ = true;
    return;
  }
  --stackSize_;
}

void SceneJsonObserver::onObjectStart() {
  if (error_) return;
  Path path = Path::Unknown;
  if (stackSize_ == 0) {
    path = Path::Root;
  } else {
    const Context& parent = stack_[stackSize_ - 1];
    if (parent.type == Context::Type::Array) {
      path = deduceObjectPath(parent);
    } else if (parent.path == Path::Root || parent.path == Path::State) {
      if (lastKey_ == "state") path = Path::State;
      else if (lastKey_ == "song") path = Path::Song;
      else if (lastKey_ == "tape") path = Path::Tape;
      else if (lastKey_ == "led") path = Path::Led;
      else if (lastKey_ == "generatorParams") path = Path::GeneratorParams;
      else if (lastKey_ == "vocal") path = Path::Vocal;
      else if (lastKey_ == "mute") path = Path::Mute;
    } else if (parent.path == Path::Led && lastKey_ == "vocal") {
      path = Path::Vocal;  // vocal is nested inside led in current format
    } else if (parent.path == Path::Led && lastKey_ == "samplerPads") {
      path = Path::SamplerPads; // samplerPads can be inside led in some versions
    }
  }
  pushContext(Context::Type::Object, path);
  if (path == Path::Unknown) {
    Serial.printf("[Parser] WARNING: Unknown object path, lastKey='%s', parent_path=%d, stackSize=%d (skipping)\n", 
                  lastKey_.c_str(), stackSize_ > 1 ? static_cast<int>(stack_[stackSize_-2].path) : -1, stackSize_);
    // Don't set error_ = true, just skip this unknown object
  }
}

void SceneJsonObserver::onObjectEnd() {
  if (error_) return;
  popContext();
}

void SceneJsonObserver::onArrayStart() {
  if (error_) return;
  Path path = Path::Unknown;
  if (stackSize_ > 0) {
    const Context& parent = stack_[stackSize_ - 1];
    if (parent.type == Context::Type::Object) {
      if (parent.path == Path::Root || parent.path == Path::State) {
        if (lastKey_ == "drumBanks") path = Path::DrumBanks;
        else if (lastKey_ == "synthABanks") path = Path::SynthABanks;
        else if (lastKey_ == "synthBBanks") path = Path::SynthBBanks;
        else if (lastKey_ == "samplerPads") path = Path::SamplerPads;
        else if (lastKey_ == "customPhrases") path = Path::CustomPhrases;
        else if (lastKey_ == "synthPatternIndex") path = Path::SynthPatternIndex;
        else if (lastKey_ == "synthBankIndex") path = Path::SynthBankIndex;
        else if (lastKey_ == "synthDistortion") path = Path::SynthDistortion;
        else if (lastKey_ == "synthDelay") path = Path::SynthDelay;
        else if (lastKey_ == "synthParams") path = Path::SynthParams;
        else if (lastKey_ == "bpm") path = Path::Unknown;
      } else if (parent.path == Path::Song) {
        if (lastKey_ == "positions") path = Path::SongPositions;
      } else if (parent.path == Path::Led) {
        if (lastKey_ == "clr") {
            path = Path::LedColorArray;
        }
        else if (lastKey_ == "customPhrases") path = Path::CustomPhrases;
      } else if (parent.path == Path::DrumVoice) {
        if (lastKey_ == "hit") path = Path::DrumHitArray;
        else if (lastKey_ == "accent") path = Path::DrumAccentArray;
        else if (lastKey_ == "fx") path = Path::DrumFxArray;
        else if (lastKey_ == "fxp") path = Path::DrumFxParamArray;
      } else if (parent.path == Path::Mute) {
        if (lastKey_ == "drums") path = Path::MuteDrums;
        else if (lastKey_ == "synth") path = Path::MuteSynth;
      }
    } else if (parent.type == Context::Type::Array) {
      path = deduceArrayPath(parent);
    }
  }
  pushContext(Context::Type::Array, path);
  if (path == Path::Unknown) {
    Serial.printf("[Parser] WARNING: Unknown array path, lastKey='%s', parent_path=%d, stackSize=%d (skipping)\n", 
                  lastKey_.c_str(), stackSize_ > 1 ? static_cast<int>(stack_[stackSize_-2].path) : -1, stackSize_);
    // Don't set error_ = true, just skip this unknown array
  }
}


void SceneJsonObserver::onArrayEnd() {
  if (error_) return;
  popContext();
}

void SceneJsonObserver::handlePrimitiveNumber(double value, bool isInteger) {
  (void)isInteger;
  if (error_ || stackSize_ == 0) return;
  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::Song) {
    if (lastKey_ == "length") {
      int len = static_cast<int>(value);
      if (len < 1) len = 1;
      if (len > Song::kMaxPositions) len = Song::kMaxPositions;
      song_.length = len;
      hasSong_ = true;
    }
    return;
  }
  if (path == Path::SongPosition) {
    int posIdx = currentIndexFor(Path::SongPositions);
    if (posIdx < 0 || posIdx >= Song::kMaxPositions) {
      error_ = true;
      return;
    }
    int trackIdx = -1;
    if (lastKey_ == "a") trackIdx = 0;
    else if (lastKey_ == "b") trackIdx = 1;
    else if (lastKey_ == "drums") trackIdx = 2;
    else if (lastKey_ == "voice") trackIdx = 3;
    if (trackIdx >= 0 && trackIdx < SongPosition::kTrackCount) {
      song_.positions[posIdx].patterns[trackIdx] = clampSongPatternIndex(static_cast<int>(value));
      if (posIdx + 1 > song_.length) song_.length = posIdx + 1;
      hasSong_ = true;
    }
    return;
  }
  if (path == Path::DrumHitArray || path == Path::DrumAccentArray ||
      path == Path::MuteDrums || path == Path::MuteSynth || path == Path::SynthDistortion ||
      path == Path::SynthDelay || path == Path::LedColorArray) {
    handlePrimitiveBool(value != 0);
    return;
  }
  if (path == Path::DrumFxArray || path == Path::DrumFxParamArray) {
    int bankIdx = currentIndexFor(Path::DrumBanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(Path::DrumBank);
    int voiceIdx = currentIndexFor(Path::DrumPatternSet);
    int stepIdx = stack_[stackSize_ - 1].index;
    if (patternIdx >= 0 && patternIdx < Bank<DrumPatternSet>::kPatterns &&
        voiceIdx >= 0 && voiceIdx < DrumPatternSet::kVoices &&
        stepIdx >= 0 && stepIdx < DrumPattern::kSteps &&
        bankIdx >= 0 && bankIdx < kBankCount) {
        DrumStep& step = target_.drumBanks[bankIdx].patterns[patternIdx].voices[voiceIdx].steps[stepIdx];
        if (path == Path::DrumFxArray) step.fx = static_cast<uint8_t>(value);
        else step.fxParam = static_cast<uint8_t>(value);
    }
    return;
  }
  if (path == Path::SynthPatternIndex) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx >= 0 && idx < 2) synthPatternIndex_[idx] = static_cast<int>(value);
    return;
  }
  if (path == Path::SynthBankIndex) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx >= 0 && idx < 2) synthBankIndex_[idx] = static_cast<int>(value);
    return;
  }
  if (path == Path::SynthStep) {
    int stepIdx = currentIndexFor(Path::SynthPattern);
    bool useBankB = inSynthBankB();
    int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
    if (stepIdx < 0 || stepIdx >= SynthPattern::kSteps ||
        patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns ||
        bankIdx < 0 || bankIdx >= kBankCount) {
      error_ = true;
      return;
    }
    SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                     : target_.synthABanks[bankIdx].patterns[patternIdx];
    if (lastKey_ == "note") {
      pattern.steps[stepIdx].note = static_cast<int>(value);
    } else if (lastKey_ == "slide") {
      pattern.steps[stepIdx].slide = value != 0;
    } else if (lastKey_ == "accent") {
      pattern.steps[stepIdx].accent = value != 0;
    } else if (lastKey_ == "fx") {
      pattern.steps[stepIdx].fx = static_cast<uint8_t>(value);
    } else if (lastKey_ == "fxp") {
      pattern.steps[stepIdx].fxParam = static_cast<uint8_t>(value);
    }
    return;
  }
  if (path == Path::GeneratorParams) {
    if (lastKey_ == "minNotes") target_.generatorParams.minNotes = static_cast<int>(value);
    else if (lastKey_ == "maxNotes") target_.generatorParams.maxNotes = static_cast<int>(value);
    else if (lastKey_ == "minOctave") target_.generatorParams.minOctave = static_cast<int>(value);
    else if (lastKey_ == "maxOctave") target_.generatorParams.maxOctave = static_cast<int>(value);
    else if (lastKey_ == "swingAmount") target_.generatorParams.swingAmount = static_cast<float>(value);
    else if (lastKey_ == "velocityRange") target_.generatorParams.velocityRange = static_cast<float>(value);
    else if (lastKey_ == "ghostNoteProbability") target_.generatorParams.ghostNoteProbability = static_cast<float>(value);
    else if (lastKey_ == "microTimingAmount") target_.generatorParams.microTimingAmount = static_cast<float>(value);
    else if (lastKey_ == "scaleRoot") target_.generatorParams.scaleRoot = static_cast<int>(value);
    else if (lastKey_ == "scale") target_.generatorParams.scale = static_cast<ScaleType>(static_cast<int>(value));
    return;
  }
  if (path == Path::SynthParam) {
    int synthIdx = currentIndexFor(Path::SynthParams);
    if (synthIdx < 0 || synthIdx >= 2) {
      error_ = true;
      return;
    }
    float fval = static_cast<float>(value);
    if (lastKey_ == "cutoff") {
      synthParameters_[synthIdx].cutoff = fval;
    } else if (lastKey_ == "resonance") {
      synthParameters_[synthIdx].resonance = fval;
    } else if (lastKey_ == "envAmount") {
      synthParameters_[synthIdx].envAmount = fval;
    } else if (lastKey_ == "envDecay") {
      synthParameters_[synthIdx].envDecay = fval;
    } else if (lastKey_ == "oscType") {
      synthParameters_[synthIdx].oscType = static_cast<int>(value);
    }
    return;
  }
  if (path == Path::TrackVolumes) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx >= 0 && idx < (int)VoiceId::Count) {
      target_.trackVolumes[idx] = static_cast<float>(value);
    }
    return;
  }
  if (path == Path::State) {
    if (lastKey_ == "bpm") {
      bpm_ = static_cast<float>(value);
      return;
    }
    if (lastKey_ == "songPosition") {
      songPosition_ = static_cast<int>(value);
      return;
    }
    if (lastKey_ == "songMode") {
      songMode_ = value != 0;
      return;
    }
    if (lastKey_ == "loopStart") {
      loopStartRow_ = static_cast<int>(value);
      return;
    }
    if (lastKey_ == "loopEnd") {
      loopEndRow_ = static_cast<int>(value);
      return;
    }
    if (lastKey_ == "masterVolume") {
      target_.masterVolume = static_cast<float>(value);
      return;
    }
    int intValue = static_cast<int>(value);
    if (lastKey_ == "drumPatternIndex") {
      drumPatternIndex_ = intValue;
    } else if (lastKey_ == "drumBankIndex") {
      drumBankIndex_ = intValue;
    } else if (lastKey_ == "synthPatternIndex") {
      synthPatternIndex_[0] = intValue;
    } else if (lastKey_ == "synthBankIndex") {
      synthBankIndex_[0] = intValue;
    }
    return;
  }
  if (path == Path::Vocal) {
    if (lastKey_ == "pch") target_.vocal.pitch = static_cast<float>(value);
    else if (lastKey_ == "spd") target_.vocal.speed = static_cast<float>(value);
    else if (lastKey_ == "rob") target_.vocal.robotness = static_cast<float>(value);
    else if (lastKey_ == "vol") target_.vocal.volume = static_cast<float>(value);
    return;
  }
  if (path == Path::SamplerPad) {
    int padIdx = currentIndexFor(Path::SamplerPads);
    if (padIdx >= 0 && padIdx < 16) {
      if (lastKey_ == "id") target_.samplerPads[padIdx].sampleId = static_cast<uint32_t>(value);
      else if (lastKey_ == "vol") target_.samplerPads[padIdx].volume = static_cast<float>(value);
      else if (lastKey_ == "pch") target_.samplerPads[padIdx].pitch = static_cast<float>(value);
      else if (lastKey_ == "str") target_.samplerPads[padIdx].startFrame = static_cast<uint32_t>(value);
      else if (lastKey_ == "end") target_.samplerPads[padIdx].endFrame = static_cast<uint32_t>(value);
      else if (lastKey_ == "chk") target_.samplerPads[padIdx].chokeGroup = static_cast<uint8_t>(value);
    }
    return;
  }
  if (path == Path::Tape) {
    if (lastKey_ == "mode") {
      int m = static_cast<int>(value);
      if (m >= 0 && m <= 3) target_.tape.mode = static_cast<TapeMode>(m);
    } else if (lastKey_ == "preset") {
      int p = static_cast<int>(value);
      if (p >= 0 && p < static_cast<int>(TapePreset::Count)) {
        target_.tape.preset = static_cast<TapePreset>(p);
      }
    } else if (lastKey_ == "speed") {
      int s = static_cast<int>(value);
      if (s >= 0 && s <= 2) target_.tape.speed = static_cast<uint8_t>(s);
    } else if (lastKey_ == "wow") {
      int v = static_cast<int>(value);
      target_.tape.macro.wow = static_cast<uint8_t>(v < 0 ? 0 : v > 100 ? 100 : v);
    } else if (lastKey_ == "age") {
      int v = static_cast<int>(value);
      target_.tape.macro.age = static_cast<uint8_t>(v < 0 ? 0 : v > 100 ? 100 : v);
    } else if (lastKey_ == "sat") {
      int v = static_cast<int>(value);
      target_.tape.macro.sat = static_cast<uint8_t>(v < 0 ? 0 : v > 100 ? 100 : v);
    } else if (lastKey_ == "tone") {
      int v = static_cast<int>(value);
      target_.tape.macro.tone = static_cast<uint8_t>(v < 0 ? 0 : v > 100 ? 100 : v);
    } else if (lastKey_ == "crush") {
      int v = static_cast<int>(value);
      target_.tape.macro.crush = static_cast<uint8_t>(v < 0 ? 0 : v > 3 ? 3 : v);
    } else if (lastKey_ == "vol") {
      target_.tape.looperVolume = static_cast<float>(value);
    } else if (lastKey_ == "space") {
      target_.tape.space = static_cast<uint8_t>(value);
    } else if (lastKey_ == "movement") {
      target_.tape.movement = static_cast<uint8_t>(value);
    } else if (lastKey_ == "groove") {
      target_.tape.groove = static_cast<uint8_t>(value);
    }
    return;
  }
  if (path == Path::LedColorArray) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx == 0) target_.led.color.r = static_cast<uint8_t>(value);
    else if (idx == 1) target_.led.color.g = static_cast<uint8_t>(value);
    else if (idx == 2) target_.led.color.b = static_cast<uint8_t>(value);
    return;
  }
  if (path == Path::Led) {
    if (lastKey_ == "mode") target_.led.mode = static_cast<LedMode>(static_cast<int>(value));
    else if (lastKey_ == "src") target_.led.source = static_cast<LedSource>(static_cast<int>(value));
    else if (lastKey_ == "bri") target_.led.brightness = static_cast<uint8_t>(value);
    else if (lastKey_ == "fls") target_.led.flashMs = static_cast<uint16_t>(value);
  } else if (path == Path::Vocal) {
    if (lastKey_ == "pch") target_.vocal.pitch = value;
    else if (lastKey_ == "spd") target_.vocal.speed = value;
    else if (lastKey_ == "rob") target_.vocal.robotness = value;
    else if (lastKey_ == "vol") target_.vocal.volume = value;
  } else if (path == Path::Root) {
    if (lastKey_ == "mode") {
      int m = static_cast<int>(value);
      target_.mode = static_cast<GrooveboxMode>(m);
    }
    return;
  }
}

void SceneJsonObserver::handlePrimitiveBool(bool value) {
  if (error_ || stackSize_ == 0) return;
  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::GeneratorParams) {
    if (lastKey_ == "preferDownbeats") target_.generatorParams.preferDownbeats = value;
    else if (lastKey_ == "scaleQuantize") target_.generatorParams.scaleQuantize = value;
    return;
  }
  if (path == Path::DrumHitArray || path == Path::DrumAccentArray) {
    int bankIdx = currentIndexFor(Path::DrumBanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(Path::DrumBank);
    int voiceIdx = currentIndexFor(Path::DrumPatternSet);
    int stepIdx = stack_[stackSize_ - 1].index;
    if (patternIdx < 0 || patternIdx >= Bank<DrumPatternSet>::kPatterns ||
        voiceIdx < 0 || voiceIdx >= DrumPatternSet::kVoices ||
        stepIdx < 0 || stepIdx >= DrumPattern::kSteps ||
        bankIdx < 0 || bankIdx >= kBankCount) {
      error_ = true;
      return;
    }
    DrumStep& step = target_.drumBanks[bankIdx].patterns[patternIdx].voices[voiceIdx].steps[stepIdx];
    if (path == Path::DrumHitArray) {
      step.hit = value;
    } else {
      step.accent = value;
    }
    return;
  }

  if (path == Path::MuteDrums) {
    int muteIdx = stack_[stackSize_ - 1].index;
    if (muteIdx < 0 || muteIdx >= DrumPatternSet::kVoices) {
      error_ = true;
      return;
    }
    drumMute_[muteIdx] = value;
    return;
  }

  if (path == Path::MuteSynth) {
    int muteIdx = stack_[stackSize_ - 1].index;
    if (muteIdx < 0 || muteIdx >= 2) {
      error_ = true;
      return;
    }
    synthMute_[muteIdx] = value;
    return;
  }
  if (path == Path::SynthDistortion) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx < 0 || idx >= 2) {
      error_ = true;
      return;
    }
    synthDistortion_[idx] = value;
    return;
  }
  if (path == Path::SynthDelay) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx < 0 || idx >= 2) {
      error_ = true;
      return;
    }
    synthDelay_[idx] = value;
    return;
  }

  if (path == Path::SynthStep) {
    int stepIdx = currentIndexFor(Path::SynthPattern);
    bool useBankB = inSynthBankB();
    int bankIdx = currentIndexFor(useBankB ? Path::SynthBBanks : Path::SynthABanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
    if (patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns ||
        stepIdx < 0 || stepIdx >= SynthPattern::kSteps ||
        bankIdx < 0 || bankIdx >= kBankCount) {
      error_ = true;
      return;
    }
    SynthPattern& pattern = useBankB ? target_.synthBBanks[bankIdx].patterns[patternIdx]
                                     : target_.synthABanks[bankIdx].patterns[patternIdx];
    if (lastKey_ == "slide") {
      pattern.steps[stepIdx].slide = value;
    } else if (lastKey_ == "accent") {
      pattern.steps[stepIdx].accent = value;
    }
    return;
  }

  if (path == Path::State && lastKey_ == "songMode") {
    songMode_ = value;
  } else if (path == Path::State && lastKey_ == "loopMode") {
    loopMode_ = value;
  }

  if (path == Path::SamplerPad) {
    int padIdx = currentIndexFor(Path::SamplerPads);
    if (padIdx >= 0 && padIdx < 16) {
      if (lastKey_ == "rev") target_.samplerPads[padIdx].reverse = value;
      else if (lastKey_ == "lop") target_.samplerPads[padIdx].loop = value;
    }
  }
}

void SceneJsonObserver::onNumber(int value) { handlePrimitiveNumber(static_cast<double>(value), true); }

void SceneJsonObserver::onNumber(double value) { handlePrimitiveNumber(value, false); }

void SceneJsonObserver::onBool(bool value) { handlePrimitiveBool(value); }

void SceneJsonObserver::onNull() {}

void SceneJsonObserver::onString(const std::string& value) {
  if (error_ || stackSize_ == 0) return;
  const Context& context = stack_[stackSize_ - 1];
  if (context.type == Context::Type::Object) {
    // Handle object keys that expect string values
    if (context.path == Path::State && lastKey_ == "drumEngine") {
      drumEngineName_ = value;
    } else if (context.path == Path::CustomPhrase) { // This path is for individual custom phrases, not an array
      int idx = context.index; // This index would be from a parent array, if CustomPhrase was an array of objects
      if (idx >= 0 && idx < Scene::kMaxCustomPhrases) {
        std::strncpy(target_.customPhrases[idx], value.c_str(), Scene::kMaxPhraseLength - 1);
        target_.customPhrases[idx][Scene::kMaxPhraseLength - 1] = '\0';
      }
    }
  } else if (context.type == Context::Type::Array) {
    // Handle array elements that expect string values
    if (context.path == Path::CustomPhrases) { // This is the array of custom phrases
      int idx = context.index;
      if (idx >= 0 && idx < Scene::kMaxCustomPhrases) {
        std::strncpy(target_.customPhrases[idx], value.c_str(), Scene::kMaxPhraseLength - 1);
        target_.customPhrases[idx][Scene::kMaxPhraseLength - 1] = '\0';
      }
    }
  }
}

void SceneJsonObserver::onObjectKey(const std::string& key) { lastKey_ = key; }

void SceneJsonObserver::onObjectValueStart() {}

void SceneJsonObserver::onObjectValueEnd() {
  if (error_) return;
  if (stackSize_ > 0 && stack_[stackSize_ - 1].type == Context::Type::Array) {
    ++stack_[stackSize_ - 1].index;
  }
}

bool SceneJsonObserver::hadError() const { return error_; }

int SceneJsonObserver::drumPatternIndex() const { return drumPatternIndex_; }

int SceneJsonObserver::synthPatternIndex(int synthIdx) const {
  int idx = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthPatternIndex_[idx];
}

int SceneJsonObserver::drumBankIndex() const { return drumBankIndex_; }

int SceneJsonObserver::synthBankIndex(int synthIdx) const {
  int idx = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthBankIndex_[idx];
}

bool SceneJsonObserver::drumMute(int idx) const {
  if (idx < 0) return drumMute_[0];
  if (idx >= DrumPatternSet::kVoices) return drumMute_[DrumPatternSet::kVoices - 1];
  return drumMute_[idx];
}

bool SceneJsonObserver::synthMute(int idx) const {
  int clamped = idx < 0 ? 0 : idx > 1 ? 1 : idx;
  return synthMute_[clamped];
}

bool SceneJsonObserver::synthDistortionEnabled(int idx) const {
  int clamped = idx < 0 ? 0 : idx > 1 ? 1 : idx;
  return synthDistortion_[clamped];
}

bool SceneJsonObserver::synthDelayEnabled(int idx) const {
  int clamped = idx < 0 ? 0 : idx > 1 ? 1 : idx;
  return synthDelay_[clamped];
}

const SynthParameters& SceneJsonObserver::synthParameters(int synthIdx) const {
  int clamped = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthParameters_[clamped];
}

float SceneJsonObserver::bpm() const { return bpm_; }

const Song& SceneJsonObserver::song() const { return song_; }

bool SceneJsonObserver::hasSong() const { return hasSong_; }

bool SceneJsonObserver::songMode() const { return songMode_; }

int SceneJsonObserver::songPosition() const { return songPosition_; }

bool SceneJsonObserver::loopMode() const { return loopMode_; }

int SceneJsonObserver::loopStartRow() const { return loopStartRow_; }

int SceneJsonObserver::loopEndRow() const { return loopEndRow_; }

const std::string& SceneJsonObserver::drumEngineName() const { return drumEngineName_; }

GrooveboxMode SceneJsonObserver::mode() const { return target_.mode; }

// Main processing scene (static to avoid heap fragmentation)
static Scene g_mainScene;

SceneManager::SceneManager() : scene_(&g_mainScene) {}

void SceneManager::loadDefaultScene() {
  drumPatternIndex_ = 0;
  drumBankIndex_ = 0;
  synthPatternIndex_[0] = 0;
  synthPatternIndex_[1] = 0;
  synthBankIndex_[0] = 0;
  synthBankIndex_[1] = 0;
  for (int i = 0; i < DrumPatternSet::kVoices; ++i) drumMute_[i] = false;
  synthMute_[0] = false;
  synthMute_[1] = false;
  synthDistortion_[0] = false;
  synthDistortion_[1] = false;
  synthDelay_[0] = false;
  synthDelay_[1] = false;
  synthParameters_[0] = SynthParameters();
  synthParameters_[1] = SynthParameters();
  drumEngineName_ = "808";
  setBpm(100.0f);
  songMode_ = false;
  songPosition_ = 0;
  loopMode_ = false;
  loopStartRow_ = 0;
  loopEndRow_ = 0;
  mode_ = GrooveboxMode::Acid;
  clearSongData(scene_->song);
  scene_->song.length = 1;
  scene_->song.positions[0].patterns[0] = 0;
  scene_->song.positions[0].patterns[1] = 0;
  scene_->song.positions[0].patterns[2] = 0;
  scene_->song.positions[0].patterns[3] = -1;

  for (int b = 0; b < kBankCount; ++b) {
    for (int i = 0; i < Bank<DrumPatternSet>::kPatterns; ++i) {
      for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
        clearDrumPattern(scene_->drumBanks[b].patterns[i].voices[v]);
      }
    }
    for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
      clearSynthPattern(scene_->synthABanks[b].patterns[i]);
      clearSynthPattern(scene_->synthBBanks[b].patterns[i]);
    }
  }

  int8_t notes[SynthPattern::kSteps] = {48, 48, 55, 55, 50, 50, 55, 55,
                                        48, 48, 55, 55, 50, 55, 50, -1};
  bool accent[SynthPattern::kSteps] = {false, true, false, true, false, true,
                                       false, true, false, true, false, true,
                                       false, true, false, false};
  bool slide[SynthPattern::kSteps] = {false, true, false, true, false, true,
                                      false, true, false, true, false, true,
                                      false, true, false, false};

  int8_t notes2[SynthPattern::kSteps] = {48, 48, 55, 55, 50, 50, 55, 55,
                                         48, 48, 55, 55, 50, 55, 50, -1};
  bool accent2[SynthPattern::kSteps] = {true,  false, true,  false, true,  false,
                                        true,  false, true,  false, true,  false,
                                        true,  false, true,  false};
  bool slide2[SynthPattern::kSteps] = {false, false, true,  false, false, false,
                                       true,  false, false, false, true,  false,
                                       false, false, true,  false};

  bool kick[DrumPattern::kSteps] = {true,  false, false, false, true,  false,
                                    false, false, true,  false, false, false,
                                    true,  false, false, false};

  bool snare[DrumPattern::kSteps] = {false, false, true,  false, false, false,
                                     true,  false, false, false, true,  false,
                                     false, false, true,  false};

  bool hat[DrumPattern::kSteps] = {true, true, true, true, true, true, true, true,
                                   true, true, true, true, true, true, true, true};

  bool openHat[DrumPattern::kSteps] = {false, false, false, true,  false, false, false, false,
                                       false, false, false, true,  false, false, false, false};

  bool midTom[DrumPattern::kSteps] = {false, false, false, false, true,  false, false, false,
                                      false, false, false, false, true,  false, false, false};

  bool highTom[DrumPattern::kSteps] = {false, false, false, false, false, false, true,  false,
                                       false, false, false, false, false, false, true,  false};

  bool rim[DrumPattern::kSteps] = {false, false, false, false, false, true,  false, false,
                                   false, false, false, false, false, true,  false, false};

  bool clap[DrumPattern::kSteps] = {false, false, false, false, false, false, false, false,
                                    false, false, false, false, true,  false, false, false};

  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    scene_->synthABanks[0].patterns[0].steps[i].note = notes[i];
    scene_->synthABanks[0].patterns[0].steps[i].accent = accent[i];
    scene_->synthABanks[0].patterns[0].steps[i].slide = slide[i];

    scene_->synthBBanks[0].patterns[0].steps[i].note = notes2[i];
    scene_->synthBBanks[0].patterns[0].steps[i].accent = accent2[i];
    scene_->synthBBanks[0].patterns[0].steps[i].slide = slide2[i];
  }

  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    bool hatVal = hat[i];
    if (openHat[i]) {
      hatVal = false;
    }
    scene_->drumBanks[0].patterns[0].voices[0].steps[i].hit = kick[i];
    scene_->drumBanks[0].patterns[0].voices[0].steps[i].accent = kick[i];

    scene_->drumBanks[0].patterns[0].voices[1].steps[i].hit = snare[i];
    scene_->drumBanks[0].patterns[0].voices[1].steps[i].accent = snare[i];

    scene_->drumBanks[0].patterns[0].voices[2].steps[i].hit = hatVal;
    scene_->drumBanks[0].patterns[0].voices[2].steps[i].accent = hatVal;

    scene_->drumBanks[0].patterns[0].voices[3].steps[i].hit = openHat[i];
    scene_->drumBanks[0].patterns[0].voices[3].steps[i].accent = openHat[i];

    scene_->drumBanks[0].patterns[0].voices[4].steps[i].hit = midTom[i];
    scene_->drumBanks[0].patterns[0].voices[4].steps[i].accent = midTom[i];

    scene_->drumBanks[0].patterns[0].voices[5].steps[i].hit = highTom[i];
    scene_->drumBanks[0].patterns[0].voices[5].steps[i].accent = highTom[i];

    scene_->drumBanks[0].patterns[0].voices[6].steps[i].hit = rim[i];
    scene_->drumBanks[0].patterns[0].voices[6].steps[i].accent = rim[i];

    scene_->drumBanks[0].patterns[0].voices[7].steps[i].hit = clap[i];
    scene_->drumBanks[0].patterns[0].voices[7].steps[i].accent = clap[i];
  }
}

Scene& SceneManager::currentScene() { return *scene_; }

const Scene& SceneManager::currentScene() const { return *scene_; }

const DrumPatternSet& SceneManager::getCurrentDrumPattern() const {
  int bank = clampBankIndex(drumBankIndex_);
  return scene_->drumBanks[bank].patterns[clampPatternIndex(drumPatternIndex_)];
}

DrumPatternSet& SceneManager::editCurrentDrumPattern() {
  int bank = clampBankIndex(drumBankIndex_);
  return scene_->drumBanks[bank].patterns[clampPatternIndex(drumPatternIndex_)];
}

const SynthPattern& SceneManager::getCurrentSynthPattern(int synthIndex) const {
  int idx = clampSynthIndex(synthIndex);
  int patternIndex = clampPatternIndex(synthPatternIndex_[idx]);
  int bank = clampBankIndex(synthBankIndex_[idx]);
  if (idx == 0) {
    return scene_->synthABanks[bank].patterns[patternIndex];
  }
  return scene_->synthBBanks[bank].patterns[patternIndex];
}

SynthPattern& SceneManager::editCurrentSynthPattern(int synthIndex) {
  int idx = clampSynthIndex(synthIndex);
  int patternIndex = clampPatternIndex(synthPatternIndex_[idx]);
  int bank = clampBankIndex(synthBankIndex_[idx]);
  if (idx == 0) {
    return scene_->synthABanks[bank].patterns[patternIndex];
  }
  return scene_->synthBBanks[bank].patterns[patternIndex];
}

const SynthPattern& SceneManager::getSynthPattern(int synthIndex, int patternIndex) const {
  int idx = clampSynthIndex(synthIndex);
  int pat = clampPatternIndex(patternIndex);
  int bank = clampBankIndex(synthBankIndex_[idx]);
  if (idx == 0) {
    return scene_->synthABanks[bank].patterns[pat];
  }
  return scene_->synthBBanks[bank].patterns[pat];
}

SynthPattern& SceneManager::editSynthPattern(int synthIndex, int patternIndex) {
  int idx = clampSynthIndex(synthIndex);
  int pat = clampPatternIndex(patternIndex);
  int bank = clampBankIndex(synthBankIndex_[idx]);
  if (idx == 0) {
    return scene_->synthABanks[bank].patterns[pat];
  }
  return scene_->synthBBanks[bank].patterns[pat];
}

const DrumPatternSet& SceneManager::getDrumPatternSet(int patternIndex) const {
  int pat = clampPatternIndex(patternIndex);
  int bank = clampBankIndex(drumBankIndex_);
  return scene_->drumBanks[bank].patterns[pat];
}

DrumPatternSet& SceneManager::editDrumPatternSet(int patternIndex) {
  int pat = clampPatternIndex(patternIndex);
  int bank = clampBankIndex(drumBankIndex_);
  return scene_->drumBanks[bank].patterns[pat];
}

void SceneManager::setCurrentDrumPatternIndex(int idx) {
  drumPatternIndex_ = clampPatternIndex(idx);
}

void SceneManager::setCurrentSynthPatternIndex(int synthIdx, int idx) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthPatternIndex_[clampedSynth] = clampPatternIndex(idx);
}

int SceneManager::getCurrentDrumPatternIndex() const { return drumPatternIndex_; }

int SceneManager::getCurrentSynthPatternIndex(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthPatternIndex_[clampedSynth];
}

void SceneManager::setDrumMute(int voiceIdx, bool mute) {
  int clampedVoice = clampIndex(voiceIdx, DrumPatternSet::kVoices);
  drumMute_[clampedVoice] = mute;
}

bool SceneManager::getDrumMute(int voiceIdx) const {
  int clampedVoice = clampIndex(voiceIdx, DrumPatternSet::kVoices);
  return drumMute_[clampedVoice];
}

void SceneManager::setSynthMute(int synthIdx, bool mute) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthMute_[clampedSynth] = mute;
}

bool SceneManager::getSynthMute(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthMute_[clampedSynth];
}

void SceneManager::setSynthDistortionEnabled(int synthIdx, bool enabled) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthDistortion_[clampedSynth] = enabled;
}

bool SceneManager::getSynthDistortionEnabled(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthDistortion_[clampedSynth];
}

void SceneManager::setSynthDelayEnabled(int synthIdx, bool enabled) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthDelay_[clampedSynth] = enabled;
}

bool SceneManager::getSynthDelayEnabled(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthDelay_[clampedSynth];
}

void SceneManager::setSynthParameters(int synthIdx, const SynthParameters& params) {
  int clampedSynth = clampSynthIndex(synthIdx);
  synthParameters_[clampedSynth] = params;
}

const SynthParameters& SceneManager::getSynthParameters(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthParameters_[clampedSynth];
}

void SceneManager::setDrumEngineName(const std::string& name) { drumEngineName_ = name; }

const std::string& SceneManager::getDrumEngineName() const { return drumEngineName_; }

void SceneManager::setMode(GrooveboxMode mode) {
  mode_ = mode;
  scene_->mode = mode;
}

GrooveboxMode SceneManager::getMode() const {
  return mode_;
}


void SceneManager::setBpm(float bpm) {
  if (bpm < 10.0f) bpm = 10.0f;
  if (bpm > 250.0f) bpm = 250.0f;
  bpm_ = bpm;
}

float SceneManager::getBpm() const { return bpm_; }

const Song& SceneManager::song() const { return scene_->song; }

Song& SceneManager::editSong() { return scene_->song; }

void SceneManager::setSongPattern(int position, SongTrack track, int patternIndex) {
  int pos = position;
  if (pos < 0) pos = 0;
  if (pos >= Song::kMaxPositions) pos = Song::kMaxPositions - 1;
  int trackIdx = songTrackToIndex(track);
  if (trackIdx < 0 || trackIdx >= SongPosition::kTrackCount) return;
  int pat = clampSongPatternIndex(patternIndex);
  if (pos >= scene_->song.length) setSongLength(pos + 1);
  scene_->song.positions[pos].patterns[trackIdx] = static_cast<int8_t>(pat);
}

void SceneManager::clearSongPattern(int position, SongTrack track) {
  int pos = clampSongPosition(position);
  int trackIdx = songTrackToIndex(track);
  if (trackIdx < 0 || trackIdx >= SongPosition::kTrackCount) return;
  scene_->song.positions[pos].patterns[trackIdx] = -1;
  trimSongLength();
}

int SceneManager::songPattern(int position, SongTrack track) const {
  if (position < 0 || position >= Song::kMaxPositions) return -1;
  int trackIdx = songTrackToIndex(track);
  if (trackIdx < 0 || trackIdx >= SongPosition::kTrackCount) return -1;
  if (position >= scene_->song.length) return -1;
  return clampSongPatternIndex(scene_->song.positions[position].patterns[trackIdx]);
}

void SceneManager::setSongLength(int length) {
  int clamped = clampSongLength(length);
  scene_->song.length = clamped;
  if (songPosition_ >= scene_->song.length) songPosition_ = scene_->song.length - 1;
  if (songPosition_ < 0) songPosition_ = 0;
  clampLoopRange();
}

int SceneManager::songLength() const {
  int len = scene_->song.length;
  if (len < 1) len = 1;
  if (len > Song::kMaxPositions) len = Song::kMaxPositions;
  return len;
}

void SceneManager::setSongPosition(int position) {
  songPosition_ = clampSongPosition(position);
}

int SceneManager::getSongPosition() const {
  return clampSongPosition(songPosition_);
}

void SceneManager::setTrackVolume(int voiceIdx, float volume) {
  if (voiceIdx >= 0 && voiceIdx < (int)VoiceId::Count) {
    scene_->trackVolumes[voiceIdx] = volume;
  }
}

float SceneManager::getTrackVolume(int voiceIdx) const {
  if (voiceIdx >= 0 && voiceIdx < (int)VoiceId::Count) {
    return scene_->trackVolumes[voiceIdx];
  }
  return 1.0f;
}

void SceneManager::setSongMode(bool enabled) { songMode_ = enabled; }

bool SceneManager::songMode() const { return songMode_; }

void SceneManager::setLoopMode(bool enabled) {
  loopMode_ = enabled;
  if (loopMode_) {
    clampLoopRange();
  }
}

bool SceneManager::loopMode() const { return loopMode_; }

void SceneManager::setLoopRange(int startRow, int endRow) {
  loopStartRow_ = startRow;
  loopEndRow_ = endRow;
  clampLoopRange();
}

int SceneManager::loopStartRow() const { return loopStartRow_; }

int SceneManager::loopEndRow() const { return loopEndRow_; }

void SceneManager::setCurrentBankIndex(int instrumentId, int bankIdx) {
  int clampedBank = clampBankIndex(bankIdx);
  if (instrumentId == 0) {
    drumBankIndex_ = clampedBank;
  } else {
    int synthIdx = clampSynthIndex(instrumentId - 1);
    synthBankIndex_[synthIdx] = clampedBank;
  }
}

int SceneManager::getCurrentBankIndex(int instrumentId) const {
  if (instrumentId == 0) return drumBankIndex_;
  int synthIdx = clampSynthIndex(instrumentId - 1);
  return synthBankIndex_[synthIdx];
}

void SceneManager::setDrumStep(int voiceIdx, int step, bool hit, bool accent) {
  DrumPatternSet& patternSet = editCurrentDrumPattern();
  int clampedVoice = clampIndex(voiceIdx, DrumPatternSet::kVoices);
  int clampedStep = clampIndex(step, DrumPattern::kSteps);
  patternSet.voices[clampedVoice].steps[clampedStep].hit = hit;
  patternSet.voices[clampedVoice].steps[clampedStep].accent = accent;
}

void SceneManager::setSynthStep(int synthIdx, int step, int note, bool slide, bool accent) {
  SynthPattern& pattern = editCurrentSynthPattern(synthIdx);
  int clampedStep = clampIndex(step, SynthPattern::kSteps);
  pattern.steps[clampedStep].note = note;
  pattern.steps[clampedStep].slide = slide;
  pattern.steps[clampedStep].accent = accent;
}

void SceneManager::buildSceneDocument(ArduinoJson::JsonDocument& doc) const {
  doc.clear();
  ArduinoJson::JsonObject root = doc.to<ArduinoJson::JsonObject>();

  ArduinoJson::JsonArray drumBanks = root["drumBanks"].to<ArduinoJson::JsonArray>();
  serializeDrumBanks(scene_->drumBanks, drumBanks);
  ArduinoJson::JsonArray synthABanks = root["synthABanks"].to<ArduinoJson::JsonArray>();
  serializeSynthBanks(scene_->synthABanks, synthABanks);
  ArduinoJson::JsonArray synthBBanks = root["synthBBanks"].to<ArduinoJson::JsonArray>();
  serializeSynthBanks(scene_->synthBBanks, synthBBanks);
  ArduinoJson::JsonObject songObj = root["song"].to<ArduinoJson::JsonObject>();
  int songLen = songLength();
  songObj["length"] = songLen;
  ArduinoJson::JsonArray songPositions = songObj["positions"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < songLen; ++i) {
    ArduinoJson::JsonObject pos = songPositions.add<ArduinoJson::JsonObject>();
    pos["a"] = scene_->song.positions[i].patterns[0];
    pos["b"] = scene_->song.positions[i].patterns[1];
    pos["drums"] = scene_->song.positions[i].patterns[2];
    pos["voice"] = scene_->song.positions[i].patterns[3];
  }
  
  ArduinoJson::JsonArray customPhrases = root["customPhrases"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < Scene::kMaxCustomPhrases; ++i) {
    customPhrases.add(std::string(scene_->customPhrases[i]));
  }

  ArduinoJson::JsonObject state = root["state"].to<ArduinoJson::JsonObject>();
  state["drumPatternIndex"] = drumPatternIndex_;
  state["bpm"] = bpm_;
  state["songMode"] = songMode_;
  state["songPosition"] = clampSongPosition(songPosition_);
  state["loopMode"] = loopMode_;
  state["loopStart"] = loopStartRow_;
  state["loopEnd"] = loopEndRow_;
  state["drumEngine"] = drumEngineName_;

  ArduinoJson::JsonArray synthPatternIndices = state["synthPatternIndex"].to<ArduinoJson::JsonArray>();
  synthPatternIndices.add(synthPatternIndex_[0]);
  synthPatternIndices.add(synthPatternIndex_[1]);

  state["drumBankIndex"] = drumBankIndex_;
  ArduinoJson::JsonArray synthBankIndices = state["synthBankIndex"].to<ArduinoJson::JsonArray>();
  synthBankIndices.add(synthBankIndex_[0]);
  synthBankIndices.add(synthBankIndex_[1]);

  ArduinoJson::JsonObject mute = state["mute"].to<ArduinoJson::JsonObject>();
  ArduinoJson::JsonArray drumMutes = mute["drums"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < DrumPatternSet::kVoices; ++i) {
    drumMutes.add(drumMute_[i]);
  }
  ArduinoJson::JsonArray synthMutes = mute["synth"].to<ArduinoJson::JsonArray>();
  synthMutes.add(synthMute_[0]);
  synthMutes.add(synthMute_[1]);

  ArduinoJson::JsonArray synthParams = state["synthParams"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < 2; ++i) {
    ArduinoJson::JsonObject param = synthParams.add<ArduinoJson::JsonObject>();
    param["cutoff"] = synthParameters_[i].cutoff;
    param["resonance"] = synthParameters_[i].resonance;
    param["envAmount"] = synthParameters_[i].envAmount;
    param["envDecay"] = synthParameters_[i].envDecay;
    param["oscType"] = synthParameters_[i].oscType;
  }
  ArduinoJson::JsonArray synthDistortion = state["synthDistortion"].to<ArduinoJson::JsonArray>();
  synthDistortion.add(synthDistortion_[0]);
  synthDistortion.add(synthDistortion_[1]);
  ArduinoJson::JsonArray synthDelay = state["synthDelay"].to<ArduinoJson::JsonArray>();
  synthDelay.add(synthDelay_[0]);
  synthDelay.add(synthDelay_[1]);

  state["masterVolume"] = scene_->masterVolume;

  ArduinoJson::JsonObject genParams = root["generatorParams"].to<ArduinoJson::JsonObject>();
  serializeGeneratorParams(scene_->generatorParams, genParams);

  ArduinoJson::JsonObject ledObj = root["led"].to<ArduinoJson::JsonObject>();
  serializeLedSettings(scene_->led, ledObj);

  ArduinoJson::JsonArray samplerPadsArr = root["samplerPads"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < 16; ++i) {
    ArduinoJson::JsonObject padObj = samplerPadsArr.add<ArduinoJson::JsonObject>();
    const auto& p = scene_->samplerPads[i];
    padObj["id"] = p.sampleId;
    padObj["vol"] = p.volume;
    padObj["pch"] = p.pitch;
    padObj["str"] = p.startFrame;
    padObj["end"] = p.endFrame;
    padObj["chk"] = p.chokeGroup;
    padObj["rev"] = p.reverse;
    padObj["lop"] = p.loop;
  }

  ArduinoJson::JsonObject tapeObj = root["tape"].to<ArduinoJson::JsonObject>();
  tapeObj["mode"] = static_cast<int>(scene_->tape.mode);
  tapeObj["preset"] = static_cast<int>(scene_->tape.preset);
  tapeObj["speed"] = scene_->tape.speed;
  tapeObj["fxEnabled"] = scene_->tape.fxEnabled;
  tapeObj["wow"] = scene_->tape.macro.wow;
  tapeObj["age"] = scene_->tape.macro.age;
  tapeObj["sat"] = scene_->tape.macro.sat;
  tapeObj["tone"] = scene_->tape.macro.tone;
  tapeObj["crush"] = scene_->tape.macro.crush;
  tapeObj["vol"] = scene_->tape.looperVolume;
  tapeObj["space"] = scene_->tape.space;
  tapeObj["movement"] = scene_->tape.movement;
  tapeObj["groove"] = scene_->tape.groove;
}

bool SceneManager::applySceneDocument(const ArduinoJson::JsonDocument& doc) {
  ArduinoJson::JsonObjectConst obj = doc.as<ArduinoJson::JsonObjectConst>();
  if (obj.isNull()) return false;

  ArduinoJson::JsonVariantConst drumBanksVal = obj["drumBanks"];
  ArduinoJson::JsonVariantConst synthABanksVal = obj["synthABanks"];
  ArduinoJson::JsonVariantConst synthBBanksVal = obj["synthBBanks"];
  if (drumBanksVal.isNull()) drumBanksVal = obj["drumBank"];
  if (synthABanksVal.isNull()) synthABanksVal = obj["synthABank"];
  if (synthBBanksVal.isNull()) synthBBanksVal = obj["synthBBank"];
  if (drumBanksVal.isNull() || synthABanksVal.isNull() || synthBBanksVal.isNull()) return false;

  auto loaded = std::make_unique<Scene>();
  clearSceneData(*loaded);

  if (!deserializeDrumBanks(drumBanksVal, loaded->drumBanks)) return false;
  if (!deserializeSynthBanks(synthABanksVal, loaded->synthABanks)) return false;
  if (!deserializeSynthBanks(synthBBanksVal, loaded->synthBBanks)) return false;

  int drumPatternIndex = 0;
  int synthPatternIndexA = 0;
  int synthPatternIndexB = 0;
  int drumBankIndex = 0;
  int synthBankIndexA = 0;
  int synthBankIndexB = 0;
  bool drumMute[DrumPatternSet::kVoices] = {false, false, false, false, false, false, false, false};
  bool synthMute[2] = {false, false};
  bool synthDistortion[2] = {false, false};
  bool synthDelay[2] = {false, false};
  SynthParameters synthParams[2] = {SynthParameters(), SynthParameters()};
  float bpm = bpm_;
  Song loadedSong{};
  clearSong(loadedSong);
  bool hasSongObj = false;
  bool songMode = songMode_;
  int songPosition = songPosition_;
  bool loopMode = false;
  int loopStartRow = 0;
  int loopEndRow = 0;
  std::string drumEngineName = drumEngineName_;

  ArduinoJson::JsonObjectConst songObj = obj["song"].as<ArduinoJson::JsonObjectConst>();
  if (!songObj.isNull()) {
    hasSongObj = true;
    int length = valueToInt(songObj["length"], loadedSong.length);
    loadedSong.length = clampSongLength(length);
    ArduinoJson::JsonArrayConst positions = songObj["positions"].as<ArduinoJson::JsonArrayConst>();
    if (!positions.isNull()) {
      int posIdx = 0;
      for (ArduinoJson::JsonVariantConst posVal : positions) {
        if (posIdx >= Song::kMaxPositions) break;
        ArduinoJson::JsonObjectConst posObj = posVal.as<ArduinoJson::JsonObjectConst>();
        if (!posObj.isNull()) {
          auto a = posObj["a"];
          auto b = posObj["b"];
          auto d = posObj["drums"];
          if (a.is<int>()) {
            loadedSong.positions[posIdx].patterns[0] = clampSongPatternIndex(a.as<int>());
          }
          if (b.is<int>()) {
            loadedSong.positions[posIdx].patterns[1] = clampSongPatternIndex(b.as<int>());
          }
          if (posObj["drums"].is<int>()) {
            loadedSong.positions[posIdx].patterns[2] = clampSongPatternIndex(posObj["drums"].as<int>());
          }
          if (posObj["voice"].is<int>()) {
            loadedSong.positions[posIdx].patterns[3] = clampSongPatternIndex(posObj["voice"].as<int>());
          }
        }
        if (posIdx + 1 > loadedSong.length) loadedSong.length = posIdx + 1;
        ++posIdx;
      }
    }
    ArduinoJson::JsonArrayConst songDistortionArr = songObj["synthDistortion"].as<ArduinoJson::JsonArrayConst>();
    if (!songDistortionArr.isNull()) {
      if (!deserializeBoolArray(songDistortionArr, synthDistortion, 2)) return false;
    }
    ArduinoJson::JsonArrayConst songDelayArr = songObj["synthDelay"].as<ArduinoJson::JsonArrayConst>();
    if (!songDelayArr.isNull()) {
      if (!deserializeBoolArray(songDelayArr, synthDelay, 2)) return false;
    }
  }

  ArduinoJson::JsonObjectConst ledObj = obj["led"].as<ArduinoJson::JsonObjectConst>();
  if (!ledObj.isNull()) {
    deserializeLedSettings(ledObj, loaded->led);
  }

  ArduinoJson::JsonObjectConst state = obj["state"].as<ArduinoJson::JsonObjectConst>();
  if (!state.isNull()) {
    drumPatternIndex = valueToInt(state["drumPatternIndex"], drumPatternIndex);
    bpm = valueToFloat(state["bpm"], bpm);
    ArduinoJson::JsonArrayConst synthPatternIndexArr = state["synthPatternIndex"].as<ArduinoJson::JsonArrayConst>();
    if (!synthPatternIndexArr.isNull()) {
      if (synthPatternIndexArr.size() > 0) synthPatternIndexA = valueToInt(synthPatternIndexArr[0], synthPatternIndexA);
      if (synthPatternIndexArr.size() > 1) synthPatternIndexB = valueToInt(synthPatternIndexArr[1], synthPatternIndexB);
    }
    drumBankIndex = valueToInt(state["drumBankIndex"], drumBankIndex);
    if (state["drumEngine"].is<const char*>()) {
      drumEngineName = state["drumEngine"].as<const char*>();
    } else if (state["drumEngine"].is<std::string>()) {
      drumEngineName = state["drumEngine"].as<std::string>();
    }
    ArduinoJson::JsonArrayConst synthBankIndexArr = state["synthBankIndex"].as<ArduinoJson::JsonArrayConst>();
    if (!synthBankIndexArr.isNull()) {
      if (synthBankIndexArr.size() > 0) synthBankIndexA = valueToInt(synthBankIndexArr[0], synthBankIndexA);
      if (synthBankIndexArr.size() > 1) synthBankIndexB = valueToInt(synthBankIndexArr[1], synthBankIndexB);
    }
    ArduinoJson::JsonObjectConst muteObj = state["mute"].as<ArduinoJson::JsonObjectConst>();
    if (!muteObj.isNull()) {
      ArduinoJson::JsonArrayConst drumMuteArr = muteObj["drums"].as<ArduinoJson::JsonArrayConst>();
      if (!drumMuteArr.isNull() && !deserializeBoolArray(drumMuteArr, drumMute, DrumPatternSet::kVoices)) return false;
      ArduinoJson::JsonArrayConst synthMuteArr = muteObj["synth"].as<ArduinoJson::JsonArrayConst>();
      if (!synthMuteArr.isNull() && !deserializeBoolArray(synthMuteArr, synthMute, 2)) return false;
    }
    ArduinoJson::JsonArrayConst synthDistortionArr = state["synthDistortion"].as<ArduinoJson::JsonArrayConst>();
    if (!synthDistortionArr.isNull() &&
        !deserializeBoolArray(synthDistortionArr, synthDistortion, 2)) {
      return false;
    }
    ArduinoJson::JsonArrayConst synthDelayArr = state["synthDelay"].as<ArduinoJson::JsonArrayConst>();
    if (!synthDelayArr.isNull() && !deserializeBoolArray(synthDelayArr, synthDelay, 2)) {
      return false;
    }
    ArduinoJson::JsonArrayConst synthParamsArr = state["synthParams"].as<ArduinoJson::JsonArrayConst>();
    if (!synthParamsArr.isNull()) {
      int idx = 0;
      for (ArduinoJson::JsonVariantConst paramValue : synthParamsArr) {
        if (idx >= 2) break;
        SynthParameters parsed = synthParams[idx];
        if (!deserializeSynthParameters(paramValue, parsed)) return false;
        synthParams[idx] = parsed;
        ++idx;
      }
    }
    songMode = state["songMode"].is<bool>() ? state["songMode"].as<bool>() : songMode;
    songPosition = valueToInt(state["songPosition"], songPosition);
    loopMode = state["loopMode"].is<bool>() ? state["loopMode"].as<bool>() : loopMode;
    loopStartRow = valueToInt(state["loopStart"], loopStartRow);
    loopEndRow = valueToInt(state["loopEnd"], loopEndRow);
    loaded->masterVolume = valueToFloat(state["masterVolume"], loaded->masterVolume);
  }

  if (obj["samplerPads"].is<ArduinoJson::JsonArrayConst>()) {
    auto padsArr = obj["samplerPads"].as<ArduinoJson::JsonArrayConst>();
    if (!padsArr.isNull() && static_cast<int>(padsArr.size()) == 16) {
      for (int i = 0; i < 16; ++i) {
        auto pObj = padsArr[i].as<ArduinoJson::JsonObjectConst>();
        if (!pObj.isNull()) {
          loaded->samplerPads[i].sampleId = pObj["id"].as<uint32_t>();
          loaded->samplerPads[i].volume = pObj["vol"].as<float>();
          loaded->samplerPads[i].pitch = pObj["pch"].as<float>();
          loaded->samplerPads[i].startFrame = pObj["str"].as<uint32_t>();
          loaded->samplerPads[i].endFrame = pObj["end"].as<uint32_t>();
          loaded->samplerPads[i].chokeGroup = pObj["chk"].as<uint8_t>();
          loaded->samplerPads[i].reverse = pObj["rev"].as<bool>();
          loaded->samplerPads[i].loop = pObj["lop"].as<bool>();
        }
      }
    }
  }

  if (obj["tape"].is<ArduinoJson::JsonObjectConst>()) {
    auto tObj = obj["tape"].as<ArduinoJson::JsonObjectConst>();
    if (!tObj.isNull()) {
      // Load mode and preset (new fields)
      if (tObj["mode"].is<int>()) {
        int m = tObj["mode"].as<int>();
        if (m >= 0 && m <= 3) loaded->tape.mode = static_cast<TapeMode>(m);
      }
      if (tObj["preset"].is<int>()) {
        int p = tObj["preset"].as<int>();
        if (p >= 0 && p < static_cast<int>(TapePreset::Count)) {
          loaded->tape.preset = static_cast<TapePreset>(p);
        }
      }
      if (tObj["speed"].is<int>()) {
        int s = tObj["speed"].as<int>();
        if (s >= 0 && s <= 2) loaded->tape.speed = static_cast<uint8_t>(s);
      }
      if (tObj["fxEnabled"].is<bool>()) {
        loaded->tape.fxEnabled = tObj["fxEnabled"].as<bool>();
      }
      // Load macro values (new structure)
      if (tObj["wow"].is<int>()) {
        loaded->tape.macro.wow = std::min(100, std::max(0, tObj["wow"].as<int>()));
      }
      if (tObj["age"].is<int>()) {
        loaded->tape.macro.age = std::min(100, std::max(0, tObj["age"].as<int>()));
      }
      if (tObj["sat"].is<int>()) {
        loaded->tape.macro.sat = std::min(100, std::max(0, tObj["sat"].as<int>()));
      }
      if (tObj["tone"].is<int>()) {
        loaded->tape.macro.tone = std::min(100, std::max(0, tObj["tone"].as<int>()));
      }
      if (tObj["crush"].is<int>()) {
        loaded->tape.macro.crush = std::min(3, std::max(0, tObj["crush"].as<int>()));
      }
      // Looper volume
      if (tObj["vol"].is<float>()) {
        loaded->tape.looperVolume = tObj["vol"].as<float>();
      }
    }
  }

  ArduinoJson::JsonVariantConst genParams = obj["generatorParams"];
  if (!genParams.isNull()) {
    deserializeGeneratorParams(genParams, loaded->generatorParams);
  }

  ArduinoJson::JsonObjectConst vocalObj = obj["vocal"].as<ArduinoJson::JsonObjectConst>();
  if (!vocalObj.isNull()) {
    loaded->vocal.pitch = valueToFloat(vocalObj["pch"], loaded->vocal.pitch);
    loaded->vocal.speed = valueToFloat(vocalObj["spd"], loaded->vocal.speed);
    loaded->vocal.robotness = valueToFloat(vocalObj["rob"], loaded->vocal.robotness);
    loaded->vocal.volume = valueToFloat(vocalObj["vol"], loaded->vocal.volume);
  }

  ArduinoJson::JsonArrayConst volArr = obj["trackVolumes"].as<ArduinoJson::JsonArrayConst>();
  if (!volArr.isNull()) {
    int idx = 0;
    for (ArduinoJson::JsonVariantConst vVal : volArr) {
      if (idx >= (int)VoiceId::Count) break;
      loaded->trackVolumes[idx++] = valueToFloat(vVal, 1.0f);
    }
  }

  if (obj["customPhrases"].is<ArduinoJson::JsonArrayConst>()) {
    auto phrasesArr = obj["customPhrases"].as<ArduinoJson::JsonArrayConst>();
    int idx = 0;
    for (ArduinoJson::JsonVariantConst phraseVal : phrasesArr) {
      if (idx >= Scene::kMaxCustomPhrases) break;
      if (phraseVal.is<const char*>()) {
        std::strncpy(loaded->customPhrases[idx], phraseVal.as<const char*>(), Scene::kMaxPhraseLength - 1);
        loaded->customPhrases[idx][Scene::kMaxPhraseLength - 1] = '\0';
      }
      ++idx;
    }
  }

  if (!hasSongObj) {
    loadedSong.length = 1;
    loadedSong.positions[0].patterns[0] = songPatternFromBank(synthBankIndexA,
                                                             clampPatternIndex(synthPatternIndexA));
    loadedSong.positions[0].patterns[1] = songPatternFromBank(synthBankIndexB,
                                                             clampPatternIndex(synthPatternIndexB));
    loadedSong.positions[0].patterns[2] = songPatternFromBank(drumBankIndex,
                                                             clampPatternIndex(drumPatternIndex));
  }

  *scene_ = *loaded;
  scene_->song = loadedSong;
  drumPatternIndex_ = clampPatternIndex(drumPatternIndex);
  synthPatternIndex_[0] = clampPatternIndex(synthPatternIndexA);
  synthPatternIndex_[1] = clampPatternIndex(synthPatternIndexB);
  drumBankIndex_ = clampIndex(drumBankIndex, kBankCount);
  synthBankIndex_[0] = clampIndex(synthBankIndexA, kBankCount);
  synthBankIndex_[1] = clampIndex(synthBankIndexB, kBankCount);
  for (int i = 0; i < DrumPatternSet::kVoices; ++i) {
    drumMute_[i] = drumMute[i];
  }
  synthMute_[0] = synthMute[0];
  synthMute_[1] = synthMute[1];
  synthDistortion_[0] = synthDistortion[0];
  synthDistortion_[1] = synthDistortion[1];
  synthDelay_[0] = synthDelay[0];
  synthDelay_[1] = synthDelay[1];
  synthParameters_[0] = synthParams[0];
  synthParameters_[1] = synthParams[1];
  drumEngineName_ = drumEngineName;
  setSongLength(scene_->song.length);
  songPosition_ = clampSongPosition(songPosition);
  songMode_ = songMode;
  loopMode_ = loopMode;
  loopStartRow_ = loopStartRow;
  loopEndRow_ = loopEndRow;
  clampLoopRange();
  setBpm(bpm);
  return true;
}

std::string SceneManager::dumpCurrentScene() const {
  std::string serialized;
  writeSceneJson(serialized);
  return serialized;
}

bool SceneManager::loadScene(const std::string& json) {
  size_t idx = 0;
  JsonVisitor::NextChar nextChar = [&json, &idx]() -> int {
    if (idx >= json.size()) return -1;
    return static_cast<unsigned char>(json[idx++]);
  };
  if (loadSceneEventedWithReader(nextChar)) return true;
  
  // DISABLED: ArduinoJson fallback causes abort() on ESP32 due to insufficient DRAM
  // return loadSceneJson(json);
#ifdef ARDUINO
  Serial.println("ERROR: Streaming JSON parse failed, skipping ArduinoJson fallback (insufficient memory)");
#endif
  return false;
}

// Static buffer to avoid heap fragmentation during loading
static Scene s_tempLoadScene;

bool SceneManager::loadSceneEventedWithReader(JsonVisitor::NextChar nextChar) {
#ifdef ARDUINO
  Serial.println("  - loadSceneEventedWithReader: Using static loading buffer...");
#endif
  
  // Reuse the static buffer
  Scene* loaded = &s_tempLoadScene;
  
  // Clear it before use
  clearSceneData(*loaded);
  
#ifdef ARDUINO
  Serial.println("  - loadSceneEventedWithReader: Starting Parse...");
#endif
  struct NextCharStream {
    JsonVisitor::NextChar next;
    int read() { return next(); }
  };

  NextCharStream stream{std::move(nextChar)};
  JsonVisitor visitor;
  SceneJsonObserver observer(*loaded, bpm_);
  bool parsed = visitor.parse(stream, observer);
#ifdef ARDUINO
  Serial.printf("  - loadSceneEventedWithReader: Parse done, result=%d, error=%d\n", (int)parsed, (int)observer.hadError());
#endif
  if (!parsed || observer.hadError()) return false;

  *scene_ = *loaded;
  scene_->song = observer.song();
  drumPatternIndex_ = clampPatternIndex(observer.drumPatternIndex());
  synthPatternIndex_[0] = clampPatternIndex(observer.synthPatternIndex(0));
  synthPatternIndex_[1] = clampPatternIndex(observer.synthPatternIndex(1));
  drumBankIndex_ = clampIndex(observer.drumBankIndex(), kBankCount);
  synthBankIndex_[0] = clampIndex(observer.synthBankIndex(0), kBankCount);
  synthBankIndex_[1] = clampIndex(observer.synthBankIndex(1), kBankCount);
  if (!observer.hasSong()) {
    scene_->song.length = 1;
    scene_->song.positions[0].patterns[0] = songPatternFromBank(synthBankIndex_[0],
                                                               synthPatternIndex_[0]);
    scene_->song.positions[0].patterns[1] = songPatternFromBank(synthBankIndex_[1],
                                                               synthPatternIndex_[1]);
    scene_->song.positions[0].patterns[2] = songPatternFromBank(drumBankIndex_,
                                                               drumPatternIndex_);
  }
  for (int i = 0; i < DrumPatternSet::kVoices; ++i) {
    drumMute_[i] = observer.drumMute(i);
  }
  synthMute_[0] = observer.synthMute(0);
  synthMute_[1] = observer.synthMute(1);
  synthDistortion_[0] = observer.synthDistortionEnabled(0);
  synthDistortion_[1] = observer.synthDistortionEnabled(1);
  synthDelay_[0] = observer.synthDelayEnabled(0);
  synthDelay_[1] = observer.synthDelayEnabled(1);
  synthParameters_[0] = observer.synthParameters(0);
  synthParameters_[1] = observer.synthParameters(1);
  drumEngineName_ = observer.drumEngineName();
  setSongLength(scene_->song.length);
  songPosition_ = clampSongPosition(observer.songPosition());
  songMode_ = observer.songMode();
  loopMode_ = observer.loopMode();
  loopStartRow_ = observer.loopStartRow();
  loopEndRow_ = observer.loopEndRow();
  clampLoopRange();
  setBpm(observer.bpm());
  setMode(observer.mode());

  // Restore Sampler/Tape from observer target (the loaded scene)
  // Observer target was 'loaded' unique_ptr, which we copied to scene_ at 1397.
  // So it's already in scene_-> We just need to make sure MiniAcid pulls it.

  return true;
}

int SceneManager::clampPatternIndex(int idx) const {
  return clampIndex(idx, Bank<DrumPatternSet>::kPatterns);
}

int SceneManager::clampBankIndex(int idx) const {
  return clampIndex(idx, kBankCount);
}

int SceneManager::clampSynthIndex(int idx) const {
  if (idx < 0) return 0;
  if (idx > 1) return 1;
  return idx;
}

int SceneManager::clampSongPosition(int position) const {
  int len = songLength();
  if (len < 1) len = 1;
  if (position < 0) return 0;
  if (position >= len) return len - 1;
  if (position >= Song::kMaxPositions) return Song::kMaxPositions - 1;
  return position;
}

int SceneManager::clampSongLength(int length) const {
  if (length < 1) return 1;
  if (length > Song::kMaxPositions) return Song::kMaxPositions;
  return length;
}

int SceneManager::songTrackToIndex(SongTrack track) const {
  switch (track) {
  case SongTrack::SynthA: return 0;
  case SongTrack::SynthB: return 1;
  case SongTrack::Drums: return 2;
  case SongTrack::Voice: return 3;
  default: return -1;
  }
}

void SceneManager::trimSongLength() {
  int lastUsed = -1;
  for (int pos = scene_->song.length - 1; pos >= 0; --pos) {
    bool hasData = false;
    for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      if (scene_->song.positions[pos].patterns[t] >= 0) {
        hasData = true;
        break;
      }
    }
    if (hasData) {
      lastUsed = pos;
      break;
    }
  }
  int newLength = lastUsed >= 0 ? lastUsed + 1 : 1;
  scene_->song.length = clampSongLength(newLength);
  if (songPosition_ >= scene_->song.length) songPosition_ = scene_->song.length - 1;
  clampLoopRange();
}

void SceneManager::clearSongData(Song& song) const {
  clearSong(song);
}

void SceneManager::clampLoopRange() {
  int len = songLength();
  int maxPos = len - 1;
  if (maxPos < 0) maxPos = 0;
  if (loopStartRow_ < 0) loopStartRow_ = 0;
  if (loopEndRow_ < 0) loopEndRow_ = 0;
  if (loopStartRow_ > maxPos) loopStartRow_ = maxPos;
  if (loopEndRow_ > maxPos) loopEndRow_ = maxPos;
  if (loopStartRow_ > loopEndRow_) {
    int tmp = loopStartRow_;
    loopStartRow_ = loopEndRow_;
    loopEndRow_ = tmp;
  }
}
