#pragma once

#include <stdint.h>
#include <string>
#include <utility>
#include "ArduinoJson-v7.4.2.h"
#include "json_evented.h"

struct DrumStep {
  bool hit;
  bool accent;
};

struct DrumPattern {
  static constexpr int kSteps = 16;
  DrumStep steps[kSteps];
};

struct DrumPatternSet {
  static constexpr int kVoices = 8;
  DrumPattern voices[kVoices];
};

struct SynthStep {
  int note; 
  bool slide;
  bool accent;
};

struct SynthPattern {
  static constexpr int kSteps = 16;
  SynthStep steps[kSteps];
};

template <typename PatternType>
struct Bank {
  static constexpr int kPatterns = 8;
  PatternType patterns[kPatterns];
};

struct Scene {
  Bank<DrumPatternSet> drumBank;
  Bank<SynthPattern> synthABank;
  Bank<SynthPattern> synthBBank;
};

class SceneJsonObserver : public JsonObserver {
public:
  explicit SceneJsonObserver(Scene& scene);

  void onObjectStart() override;
  void onObjectEnd() override;
  void onArrayStart() override;
  void onArrayEnd() override;
  void onNumber(int value) override;
  void onNumber(double value) override;
  void onBool(bool value) override;
  void onNull() override;
  void onString(const std::string& value) override;
  void onObjectKey(const std::string& key) override;
  void onObjectValueStart() override;
  void onObjectValueEnd() override;

  bool hadError() const;
  int drumPatternIndex() const;
  int synthPatternIndex(int synthIdx) const;
  int drumBankIndex() const;
  int synthBankIndex(int synthIdx) const;

private:
  enum class Path {
    Root,
    DrumBank,
    DrumPatternSet,
    DrumVoice,
    DrumHitArray,
    DrumAccentArray,
    SynthABank,
    SynthBBank,
    SynthPattern,
    SynthStep,
    State,
    SynthPatternIndex,
    SynthBankIndex,
    Unknown,
  };

  struct Context {
    enum class Type { Object, Array };
    Type type;
    Path path;
    int index;
  };

  Path deduceArrayPath(const Context& parent) const;
  Path deduceObjectPath(const Context& parent) const;
  int currentIndexFor(Path path) const;
  bool inSynthBankA() const;
  bool inSynthBankB() const;
  void pushContext(Context::Type type, Path path);
  void popContext();
  void handlePrimitiveNumber(int value);
  void handlePrimitiveBool(bool value);

  static constexpr int kMaxStack = 16;
  Context stack_[kMaxStack];
  int stackSize_ = 0;
  std::string lastKey_;
  Scene& target_;
  bool error_ = false;
  int drumPatternIndex_ = 0;
  int synthPatternIndex_[2] = {0, 0};
  int drumBankIndex_ = 0;
  int synthBankIndex_[2] = {0, 0};
};

class SceneManager {
public:
  void loadDefaultScene();
  Scene& currentScene();
  const Scene& currentScene() const;

  const DrumPatternSet& getCurrentDrumPattern() const;
  DrumPatternSet& editCurrentDrumPattern();

  const SynthPattern& getCurrentSynthPattern(int synthIndex) const;
  SynthPattern& editCurrentSynthPattern(int synthIndex);

  void setCurrentDrumPatternIndex(int idx);
  void setCurrentSynthPatternIndex(int synthIdx, int idx);
  int getCurrentDrumPatternIndex() const;
  int getCurrentSynthPatternIndex(int synthIdx) const;

  void setCurrentBankIndex(int instrumentId, int bankIdx);
  int getCurrentBankIndex(int instrumentId) const;

  void setDrumStep(int voiceIdx, int step, bool hit, bool accent);
  void setSynthStep(int synthIdx, int step, int note, bool slide, bool accent);

  std::string dumpCurrentScene() const;
  bool loadScene(const std::string& json);

  template <typename TWriter>
  bool writeSceneJson(TWriter&& writer) const;
  template <typename TReader>
  bool loadSceneJson(TReader&& reader);
  template <typename TReader>
  bool loadSceneEvented(TReader&& reader);

  // static constexpr size_t sceneJsonCapacity();

private:
  int clampPatternIndex(int idx) const;
  int clampSynthIndex(int idx) const;
  void buildSceneDocument(ArduinoJson::JsonDocument& doc) const;
  bool applySceneDocument(const ArduinoJson::JsonDocument& doc);
  bool loadSceneEventedWithReader(JsonVisitor::NextChar nextChar);

  Scene scene_;
  int drumPatternIndex_ = 0;
  int synthPatternIndex_[2] = {0, 0};
  int drumBankIndex_ = 0;
  int synthBankIndex_[2] = {0, 0};
};

// inline constexpr size_t SceneManager::sceneJsonCapacity() {
//   constexpr size_t drumPatternJsonSize = JSON_OBJECT_SIZE(2) + 2 * JSON_ARRAY_SIZE(DrumPattern::kSteps);
//   constexpr size_t drumPatternSetJsonSize = JSON_ARRAY_SIZE(DrumPatternSet::kVoices) +
//                                             DrumPatternSet::kVoices * drumPatternJsonSize;
//   constexpr size_t drumBankJsonSize = JSON_ARRAY_SIZE(Bank<DrumPatternSet>::kPatterns) +
//                                       Bank<DrumPatternSet>::kPatterns * drumPatternSetJsonSize;

//   constexpr size_t synthStepJsonSize = JSON_OBJECT_SIZE(3);
//   constexpr size_t synthPatternJsonSize = JSON_ARRAY_SIZE(SynthPattern::kSteps) +
//                                           SynthPattern::kSteps * synthStepJsonSize;
//   constexpr size_t synthBankJsonSize = JSON_ARRAY_SIZE(Bank<SynthPattern>::kPatterns) +
//                                        Bank<SynthPattern>::kPatterns * synthPatternJsonSize;

//   constexpr size_t stateJsonSize = JSON_OBJECT_SIZE(4) + JSON_ARRAY_SIZE(2) + JSON_ARRAY_SIZE(2);

//   return JSON_OBJECT_SIZE(4) + drumBankJsonSize + synthBankJsonSize * 2 + stateJsonSize + 64; // small headroom
// }

template <typename TWriter>
bool SceneManager::writeSceneJson(TWriter&& writer) const {
  ArduinoJson::JsonDocument doc;
  buildSceneDocument(doc);
  return ArduinoJson::serializeJson(doc, std::forward<TWriter>(writer)) > 0;
}

template <typename TReader>
bool SceneManager::loadSceneJson(TReader&& reader) {
  ArduinoJson::JsonDocument doc;
  auto error = ArduinoJson::deserializeJson(doc, std::forward<TReader>(reader));
  if (error) return false;
  return applySceneDocument(doc);
}

template <typename TReader>
bool SceneManager::loadSceneEvented(TReader&& reader) {
  JsonVisitor::NextChar nextChar = [&reader]() -> int { return reader.read(); };
  return loadSceneEventedWithReader(nextChar);
}
