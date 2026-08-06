#!/usr/bin/env python3
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}")
    return text.replace(old, new, 1)


root = Path(__file__).resolve().parents[1]
h_path = root / "scenes.h"
cpp_path = root / "scenes.cpp"
h = h_path.read_text()
cpp = cpp_path.read_text()

h = replace_once(
    h,
    '#include "src/dsp/genre_manager.h"\n#include "json_evented.h"',
    '#include "src/dsp/genre_manager.h"\n#include "src/phrase/phrase_types.h"\n#include "src/phrase/phrase_persistence.h"\n#include "json_evented.h"',
    "phrase includes",
)

h = replace_once(
    h,
    '  Song songs[2];\n  int activeSongSlot = 0;',
    '  Song songs[2];\n  PhraseCore::PhraseBank phraseBank;\n  int activeSongSlot = 0;',
    "Scene PhraseBank",
)

h = replace_once(
    h,
    '    SongPosition,\n    CustomPhrases,',
    '    SongPosition,\n    PhraseCore,\n    CustomPhrases,',
    "evented PhraseCore path",
)

h = replace_once(
    h,
    '  if (!writeLiteral(",\\\"customPhrases\\\":[")) return false;',
    '''  if (!writeLiteral(",\\\"phraseCore\\\":[")) return false;
  for (int i = 0; i < PhraseCore::kPersistValueCount; ++i) {
    if (i > 0 && !writeChar(',')) return false;
    if (!writeInt(PhraseCore::persistentValueAt(scene_->phraseBank, i))) return false;
  }
  if (!writeChar(']')) return false;

  if (!writeLiteral(",\\\"customPhrases\\\":[")) return false;''',
    "streaming Phrase codec",
)

cpp = replace_once(
    cpp,
    '#include "src/audio/pattern_paging.h"',
    '#include "src/audio/pattern_paging.h"\n#include "src/phrase/phrase_core.h"',
    "Phrase core implementation include",
)

cpp = replace_once(
    cpp,
    '  clearCustomPhrases(scene);\n  for (auto& pad : scene.samplerPads)',
    '  clearCustomPhrases(scene);\n  PhraseCore::reset(scene.phraseBank);\n  for (auto& pad : scene.samplerPads)',
    "clear Scene PhraseBank",
)

cpp = replace_once(
    cpp,
    '''  currentPageIndex_ = 0;
  scene_->grooveFlavor = 0;
  scene_->activeSongSlot = 0;
  for (int i = 0; i < 2; ++i) {''',
    '''  currentPageIndex_ = 0;
  scene_->grooveFlavor = 0;
  scene_->activeSongSlot = 0;
  PhraseCore::reset(scene_->phraseBank);
  for (int i = 0; i < 2; ++i) {''',
    "default Scene PhraseBank",
)

cpp = replace_once(
    cpp,
    '''  ArduinoJson::JsonArray customPhrases = root["customPhrases"].to<ArduinoJson::JsonArray>();''',
    '''  ArduinoJson::JsonArray phraseCore = root["phraseCore"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < PhraseCore::kPersistValueCount; ++i) {
    phraseCore.add(PhraseCore::persistentValueAt(scene_->phraseBank, i));
  }

  ArduinoJson::JsonArray customPhrases = root["customPhrases"].to<ArduinoJson::JsonArray>();''',
    "document Phrase codec write",
)

cpp = replace_once(
    cpp,
    '''  auto loaded = std::make_unique<Scene>();
  clearSceneData(*loaded);

  if (!deserializeDrumBanks''',
    '''  auto loaded = std::make_unique<Scene>();
  clearSceneData(*loaded);

  ArduinoJson::JsonArrayConst phraseCoreValues =
      obj["phraseCore"].as<ArduinoJson::JsonArrayConst>();
  if (!phraseCoreValues.isNull()) {
    if (static_cast<int>(phraseCoreValues.size()) !=
        PhraseCore::kPersistValueCount) {
      return false;
    }
    PhraseCore::beginPersistentDecode(loaded->phraseBank);
    int phraseIndex = 0;
    for (ArduinoJson::JsonVariantConst item : phraseCoreValues) {
      if (!item.is<int>() ||
          !PhraseCore::applyPersistentValue(
              loaded->phraseBank, phraseIndex, item.as<int>())) {
        return false;
      }
      ++phraseIndex;
    }
    PhraseCore::sanitize(loaded->phraseBank);
  }

  if (!deserializeDrumBanks''',
    "document Phrase codec read",
)

cpp = replace_once(
    cpp,
    '        else if (lastKey_ == "customPhrases") path = Path::CustomPhrases;',
    '        else if (lastKey_ == "phraseCore") {\n          path = Path::PhraseCore;\n          PhraseCore::beginPersistentDecode(target_.phraseBank);\n        }\n        else if (lastKey_ == "customPhrases") path = Path::CustomPhrases;',
    "evented Phrase array start",
)

cpp = replace_once(
    cpp,
    '''void SceneJsonObserver::onArrayEnd() {
  if (error_) return;
  popContext();
}''',
    '''void SceneJsonObserver::onArrayEnd() {
  if (error_) return;
  if (stackSize_ > 0 && stack_[stackSize_ - 1].path == Path::PhraseCore) {
    PhraseCore::sanitize(target_.phraseBank);
  }
  popContext();
}''',
    "evented Phrase array end",
)

cpp = replace_once(
    cpp,
    '''  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::Song) {''',
    '''  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::PhraseCore) {
    const int index = stack_[stackSize_ - 1].index;
    if (!PhraseCore::applyPersistentValue(
            target_.phraseBank, index, static_cast<int32_t>(value))) {
      error_ = true;
    }
    return;
  }
  if (path == Path::Song) {''',
    "evented Phrase values",
)

h_path.write_text(h)
cpp_path.write_text(cpp)
print("Phrase Scene integration applied")
