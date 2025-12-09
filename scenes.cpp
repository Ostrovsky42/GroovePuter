#include "ArduinoJson-v7.4.2.h"
#include "scenes.h"

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
  }
}

void clearSynthPattern(SynthPattern& pattern) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i].note = -1;
    pattern.steps[i].slide = false;
    pattern.steps[i].accent = false;
  }
}

void clearSceneData(Scene& scene) {
  for (int p = 0; p < Bank<DrumPatternSet>::kPatterns; ++p) {
    for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
      clearDrumPattern(scene.drumBank.patterns[p].voices[v]);
    }
  }
  for (int p = 0; p < Bank<SynthPattern>::kPatterns; ++p) {
    clearSynthPattern(scene.synthABank.patterns[p]);
    clearSynthPattern(scene.synthBBank.patterns[p]);
  }
}

void serializeDrumPattern(const DrumPattern& pattern, ArduinoJson::JsonObject obj) {
  ArduinoJson::JsonArray hit = obj["hit"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonArray accent = obj["accent"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    hit.add(pattern.steps[i].hit);
    accent.add(pattern.steps[i].accent);
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

void serializeSynthPattern(const SynthPattern& pattern, ArduinoJson::JsonArray steps) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    ArduinoJson::JsonObject step = steps.add<ArduinoJson::JsonObject>();
    step["note"] = pattern.steps[i].note;
    step["slide"] = pattern.steps[i].slide;
    step["accent"] = pattern.steps[i].accent;
  }
}

void serializeSynthBank(const Bank<SynthPattern>& bank, ArduinoJson::JsonArray patterns) {
  for (int p = 0; p < Bank<SynthPattern>::kPatterns; ++p) {
    ArduinoJson::JsonArray steps = patterns.add<ArduinoJson::JsonArray>();
    serializeSynthPattern(bank.patterns[p], steps);
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
  if (hit.isNull() || accent.isNull()) return false;
  bool hits[DrumPattern::kSteps];
  bool accents[DrumPattern::kSteps];
  if (!deserializeBoolArray(hit, hits, DrumPattern::kSteps)) return false;
  if (!deserializeBoolArray(accent, accents, DrumPattern::kSteps)) return false;
  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    pattern.steps[i].hit = hits[i];
    pattern.steps[i].accent = accents[i];
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
    if (!note.is<int>() || !slide.is<bool>() || !accent.is<bool>()) return false;
    pattern.steps[i].note = note.as<int>();
    pattern.steps[i].slide = slide.as<bool>();
    pattern.steps[i].accent = accent.as<bool>();
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

int valueToInt(ArduinoJson::JsonVariantConst value, int defaultValue) {
  if (value.is<int>()) {
    return value.as<int>();
  }
  return defaultValue;
}
}

SceneJsonObserver::SceneJsonObserver(Scene& scene) : target_(scene) {}

SceneJsonObserver::Path SceneJsonObserver::deduceArrayPath(const Context& parent) const {
  switch (parent.path) {
  case Path::DrumBank:
    return Path::DrumPatternSet;
  case Path::SynthABank:
    return Path::SynthPattern;
  case Path::SynthBBank:
    return Path::SynthPattern;
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
    if (stack_[i].path == Path::SynthABank) return true;
    if (stack_[i].path == Path::SynthBBank) return false;
  }
  return true;
}

bool SceneJsonObserver::inSynthBankB() const {
  for (int i = stackSize_ - 1; i >= 0; --i) {
    if (stack_[i].path == Path::SynthBBank) return true;
    if (stack_[i].path == Path::SynthABank) return false;
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
    } else if (parent.path == Path::Root && lastKey_ == "state") {
      path = Path::State;
    }
  }
  pushContext(Context::Type::Object, path);
  if (path == Path::Unknown) error_ = true;
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
      if (parent.path == Path::Root) {
        if (lastKey_ == "drumBank") path = Path::DrumBank;
        else if (lastKey_ == "synthABank") path = Path::SynthABank;
        else if (lastKey_ == "synthBBank") path = Path::SynthBBank;
      } else if (parent.path == Path::DrumVoice) {
        if (lastKey_ == "hit") path = Path::DrumHitArray;
        else if (lastKey_ == "accent") path = Path::DrumAccentArray;
      } else if (parent.path == Path::State) {
        if (lastKey_ == "synthPatternIndex") path = Path::SynthPatternIndex;
        else if (lastKey_ == "synthBankIndex") path = Path::SynthBankIndex;
      }
    } else if (parent.type == Context::Type::Array) {
      path = deduceArrayPath(parent);
    }
  }
  pushContext(Context::Type::Array, path);
  if (path == Path::Unknown) error_ = true;
}

void SceneJsonObserver::onArrayEnd() {
  if (error_) return;
  popContext();
}

void SceneJsonObserver::handlePrimitiveNumber(int value) {
  if (error_ || stackSize_ == 0) return;
  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::DrumHitArray || path == Path::DrumAccentArray) {
    handlePrimitiveBool(value != 0);
    return;
  }
  if (path == Path::SynthPatternIndex) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx >= 0 && idx < 2) synthPatternIndex_[idx] = value;
    return;
  }
  if (path == Path::SynthBankIndex) {
    int idx = stack_[stackSize_ - 1].index;
    if (idx >= 0 && idx < 2) synthBankIndex_[idx] = value;
    return;
  }
  if (path == Path::SynthStep) {
    int stepIdx = currentIndexFor(Path::SynthPattern);
    bool useBankB = inSynthBankB();
    int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
    if (stepIdx < 0 || stepIdx >= SynthPattern::kSteps ||
        patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns) {
      error_ = true;
      return;
    }
    SynthPattern& pattern = useBankB ? target_.synthBBank.patterns[patternIdx]
                                     : target_.synthABank.patterns[patternIdx];
    if (lastKey_ == "note") {
      pattern.steps[stepIdx].note = value;
    } else if (lastKey_ == "slide") {
      pattern.steps[stepIdx].slide = value != 0;
    } else if (lastKey_ == "accent") {
      pattern.steps[stepIdx].accent = value != 0;
    }
    return;
  }
  if (path == Path::State) {
    if (lastKey_ == "drumPatternIndex") {
      drumPatternIndex_ = value;
    } else if (lastKey_ == "drumBankIndex") {
      drumBankIndex_ = value;
    } else if (lastKey_ == "synthPatternIndex") {
      synthPatternIndex_[0] = value;
    } else if (lastKey_ == "synthBankIndex") {
      synthBankIndex_[0] = value;
    }
  }
}

void SceneJsonObserver::handlePrimitiveBool(bool value) {
  if (error_ || stackSize_ == 0) return;
  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::DrumHitArray || path == Path::DrumAccentArray) {
    int patternIdx = currentIndexFor(Path::DrumBank);
    int voiceIdx = currentIndexFor(Path::DrumPatternSet);
    int stepIdx = stack_[stackSize_ - 1].index;
    if (patternIdx < 0 || patternIdx >= Bank<DrumPatternSet>::kPatterns ||
        voiceIdx < 0 || voiceIdx >= DrumPatternSet::kVoices ||
        stepIdx < 0 || stepIdx >= DrumPattern::kSteps) {
      error_ = true;
      return;
    }
    DrumStep& step = target_.drumBank.patterns[patternIdx].voices[voiceIdx].steps[stepIdx];
    if (path == Path::DrumHitArray) {
      step.hit = value;
    } else {
      step.accent = value;
    }
    return;
  }

  if (path == Path::SynthStep) {
    int stepIdx = currentIndexFor(Path::SynthPattern);
    bool useBankB = inSynthBankB();
    int patternIdx = currentIndexFor(useBankB ? Path::SynthBBank : Path::SynthABank);
    if (patternIdx < 0 || patternIdx >= Bank<SynthPattern>::kPatterns ||
        stepIdx < 0 || stepIdx >= SynthPattern::kSteps) {
      error_ = true;
      return;
    }
    SynthPattern& pattern = useBankB ? target_.synthBBank.patterns[patternIdx]
                                     : target_.synthABank.patterns[patternIdx];
    if (lastKey_ == "slide") {
      pattern.steps[stepIdx].slide = value;
    } else if (lastKey_ == "accent") {
      pattern.steps[stepIdx].accent = value;
    }
  }
}

void SceneJsonObserver::onNumber(int value) { handlePrimitiveNumber(value); }

void SceneJsonObserver::onNumber(double value) { handlePrimitiveNumber(static_cast<int>(value)); }

void SceneJsonObserver::onBool(bool value) { handlePrimitiveBool(value); }

void SceneJsonObserver::onNull() {}

void SceneJsonObserver::onString(const std::string& value) {
  (void)value;
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

void SceneManager::loadDefaultScene() {
  drumPatternIndex_ = 0;
  drumBankIndex_ = 0;
  synthPatternIndex_[0] = 0;
  synthPatternIndex_[1] = 0;
  synthBankIndex_[0] = 0;
  synthBankIndex_[1] = 0;

  for (int i = 0; i < Bank<DrumPatternSet>::kPatterns; ++i) {
    for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
      clearDrumPattern(scene_.drumBank.patterns[i].voices[v]);
    }
  }
  for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
    clearSynthPattern(scene_.synthABank.patterns[i]);
    clearSynthPattern(scene_.synthBBank.patterns[i]);
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
    scene_.synthABank.patterns[0].steps[i].note = notes[i];
    scene_.synthABank.patterns[0].steps[i].accent = accent[i];
    scene_.synthABank.patterns[0].steps[i].slide = slide[i];

    scene_.synthBBank.patterns[0].steps[i].note = notes2[i];
    scene_.synthBBank.patterns[0].steps[i].accent = accent2[i];
    scene_.synthBBank.patterns[0].steps[i].slide = slide2[i];
  }

  for (int i = 0; i < DrumPattern::kSteps; ++i) {
    bool hatVal = hat[i];
    if (openHat[i]) {
      hatVal = false;
    }
    scene_.drumBank.patterns[0].voices[0].steps[i].hit = kick[i];
    scene_.drumBank.patterns[0].voices[0].steps[i].accent = kick[i];

    scene_.drumBank.patterns[0].voices[1].steps[i].hit = snare[i];
    scene_.drumBank.patterns[0].voices[1].steps[i].accent = snare[i];

    scene_.drumBank.patterns[0].voices[2].steps[i].hit = hatVal;
    scene_.drumBank.patterns[0].voices[2].steps[i].accent = hatVal;

    scene_.drumBank.patterns[0].voices[3].steps[i].hit = openHat[i];
    scene_.drumBank.patterns[0].voices[3].steps[i].accent = openHat[i];

    scene_.drumBank.patterns[0].voices[4].steps[i].hit = midTom[i];
    scene_.drumBank.patterns[0].voices[4].steps[i].accent = midTom[i];

    scene_.drumBank.patterns[0].voices[5].steps[i].hit = highTom[i];
    scene_.drumBank.patterns[0].voices[5].steps[i].accent = highTom[i];

    scene_.drumBank.patterns[0].voices[6].steps[i].hit = rim[i];
    scene_.drumBank.patterns[0].voices[6].steps[i].accent = rim[i];

    scene_.drumBank.patterns[0].voices[7].steps[i].hit = clap[i];
    scene_.drumBank.patterns[0].voices[7].steps[i].accent = clap[i];
  }
}

Scene& SceneManager::currentScene() { return scene_; }

const Scene& SceneManager::currentScene() const { return scene_; }

const DrumPatternSet& SceneManager::getCurrentDrumPattern() const {
  return scene_.drumBank.patterns[clampPatternIndex(drumPatternIndex_)];
}

DrumPatternSet& SceneManager::editCurrentDrumPattern() {
  return scene_.drumBank.patterns[clampPatternIndex(drumPatternIndex_)];
}

const SynthPattern& SceneManager::getCurrentSynthPattern(int synthIndex) const {
  int idx = clampSynthIndex(synthIndex);
  int patternIndex = clampPatternIndex(synthPatternIndex_[idx]);
  if (idx == 0) {
    return scene_.synthABank.patterns[patternIndex];
  }
  return scene_.synthBBank.patterns[patternIndex];
}

SynthPattern& SceneManager::editCurrentSynthPattern(int synthIndex) {
  int idx = clampSynthIndex(synthIndex);
  int patternIndex = clampPatternIndex(synthPatternIndex_[idx]);
  if (idx == 0) {
    return scene_.synthABank.patterns[patternIndex];
  }
  return scene_.synthBBank.patterns[patternIndex];
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

void SceneManager::setCurrentBankIndex(int instrumentId, int bankIdx) {
  int clampedBank = 0; // Only a single bank exists today; retain parameter for future use
  if (instrumentId == 0) {
    drumBankIndex_ = clampedBank;
  } else {
    int synthIdx = clampSynthIndex(instrumentId - 1);
    synthBankIndex_[synthIdx] = clampedBank;
  }
}

int SceneManager::getCurrentBankIndex(int instrumentId) const {
  (void)instrumentId;
  return 0;
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

  ArduinoJson::JsonArray drumBank = root["drumBank"].to<ArduinoJson::JsonArray>();
  serializeDrumBank(scene_.drumBank, drumBank);
  ArduinoJson::JsonArray synthABank = root["synthABank"].to<ArduinoJson::JsonArray>();
  serializeSynthBank(scene_.synthABank, synthABank);
  ArduinoJson::JsonArray synthBBank = root["synthBBank"].to<ArduinoJson::JsonArray>();
  serializeSynthBank(scene_.synthBBank, synthBBank);

  ArduinoJson::JsonObject state = root["state"].to<ArduinoJson::JsonObject>();
  state["drumPatternIndex"] = drumPatternIndex_;

  ArduinoJson::JsonArray synthPatternIndices = state["synthPatternIndex"].to<ArduinoJson::JsonArray>();
  synthPatternIndices.add(synthPatternIndex_[0]);
  synthPatternIndices.add(synthPatternIndex_[1]);

  state["drumBankIndex"] = drumBankIndex_;
  ArduinoJson::JsonArray synthBankIndices = state["synthBankIndex"].to<ArduinoJson::JsonArray>();
  synthBankIndices.add(synthBankIndex_[0]);
  synthBankIndices.add(synthBankIndex_[1]);
}

bool SceneManager::applySceneDocument(const ArduinoJson::JsonDocument& doc) {
  ArduinoJson::JsonObjectConst obj = doc.as<ArduinoJson::JsonObjectConst>();
  if (obj.isNull()) return false;

  ArduinoJson::JsonArrayConst drumBank = obj["drumBank"].as<ArduinoJson::JsonArrayConst>();
  ArduinoJson::JsonArrayConst synthABank = obj["synthABank"].as<ArduinoJson::JsonArrayConst>();
  ArduinoJson::JsonArrayConst synthBBank = obj["synthBBank"].as<ArduinoJson::JsonArrayConst>();
  if (drumBank.isNull() || synthABank.isNull() || synthBBank.isNull()) return false;

  Scene loaded{};
  clearSceneData(loaded);

  if (!deserializeDrumBank(drumBank, loaded.drumBank)) return false;
  if (!deserializeSynthBank(synthABank, loaded.synthABank)) return false;
  if (!deserializeSynthBank(synthBBank, loaded.synthBBank)) return false;

  int drumPatternIndex = 0;
  int synthPatternIndexA = 0;
  int synthPatternIndexB = 0;
  int drumBankIndex = 0;
  int synthBankIndexA = 0;
  int synthBankIndexB = 0;

  ArduinoJson::JsonObjectConst state = obj["state"].as<ArduinoJson::JsonObjectConst>();
  if (!state.isNull()) {
    drumPatternIndex = valueToInt(state["drumPatternIndex"], drumPatternIndex);
    ArduinoJson::JsonArrayConst synthPatternIndexArr = state["synthPatternIndex"].as<ArduinoJson::JsonArrayConst>();
    if (!synthPatternIndexArr.isNull()) {
      if (synthPatternIndexArr.size() > 0) synthPatternIndexA = valueToInt(synthPatternIndexArr[0], synthPatternIndexA);
      if (synthPatternIndexArr.size() > 1) synthPatternIndexB = valueToInt(synthPatternIndexArr[1], synthPatternIndexB);
    }
    drumBankIndex = valueToInt(state["drumBankIndex"], drumBankIndex);
    ArduinoJson::JsonArrayConst synthBankIndexArr = state["synthBankIndex"].as<ArduinoJson::JsonArrayConst>();
    if (!synthBankIndexArr.isNull()) {
      if (synthBankIndexArr.size() > 0) synthBankIndexA = valueToInt(synthBankIndexArr[0], synthBankIndexA);
      if (synthBankIndexArr.size() > 1) synthBankIndexB = valueToInt(synthBankIndexArr[1], synthBankIndexB);
    }
  }

  scene_ = loaded;
  drumPatternIndex_ = clampPatternIndex(drumPatternIndex);
  synthPatternIndex_[0] = clampPatternIndex(synthPatternIndexA);
  synthPatternIndex_[1] = clampPatternIndex(synthPatternIndexB);
  drumBankIndex_ = clampIndex(drumBankIndex, 1);
  synthBankIndex_[0] = clampIndex(synthBankIndexA, 1);
  synthBankIndex_[1] = clampIndex(synthBankIndexB, 1);
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
  return loadSceneJson(json);
}

bool SceneManager::loadSceneEventedWithReader(JsonVisitor::NextChar nextChar) {
  Scene loaded{};
  clearSceneData(loaded);

  struct NextCharStream {
    JsonVisitor::NextChar next;
    int read() { return next(); }
  };

  NextCharStream stream{std::move(nextChar)};
  JsonVisitor visitor;
  SceneJsonObserver observer(loaded);
  bool parsed = visitor.parse(stream, observer);
  if (!parsed || observer.hadError()) return false;

  scene_ = loaded;
  drumPatternIndex_ = clampPatternIndex(observer.drumPatternIndex());
  synthPatternIndex_[0] = clampPatternIndex(observer.synthPatternIndex(0));
  synthPatternIndex_[1] = clampPatternIndex(observer.synthPatternIndex(1));
  drumBankIndex_ = clampIndex(observer.drumBankIndex(), 1);
  synthBankIndex_[0] = clampIndex(observer.synthBankIndex(0), 1);
  synthBankIndex_[1] = clampIndex(observer.synthBankIndex(1), 1);
  return true;
}

int SceneManager::clampPatternIndex(int idx) const {
  return clampIndex(idx, Bank<DrumPatternSet>::kPatterns);
}

int SceneManager::clampSynthIndex(int idx) const {
  if (idx < 0) return 0;
  if (idx > 1) return 1;
  return idx;
}
