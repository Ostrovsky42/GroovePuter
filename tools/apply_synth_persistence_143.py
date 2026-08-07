#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def rw(path):
    p = ROOT / path
    return p, p.read_text(encoding="utf-8")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, got {count}")
    return text.replace(old, new, 1)

# --- scenes.h ---------------------------------------------------------------
p, s = rw("scenes.h")
s = replace_once(s,
'''struct SynthParameters {
  float cutoff = 800.0f;
  float resonance = 0.6f;
  float envAmount = 400.0f;
  float envDecay = 420.0f;
  int oscType = 0;
};
''',
'''struct SynthParameters {
  // Legacy TB303-shaped decode-only state. New saves use PersistedSynthPatch.
  float cutoff = 800.0f;
  float resonance = 0.6f;
  float envAmount = 400.0f;
  float envDecay = 420.0f;
  int oscType = 0;
};

static constexpr uint8_t kSynthStateSchemaVersion = 1;
struct PersistedSynthPatch {
  static constexpr uint8_t kMaxParams = 6;
  std::string engineName = "TB303";
  float params[kMaxParams] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  uint8_t paramCount = 0;
};
''', "persisted synth struct")

s = replace_once(s,
'''  const SynthParameters& synthParameters(int synthIdx) const;
  float bpm() const;
''',
'''  const SynthParameters& synthParameters(int synthIdx) const;
  bool legacySynthParametersPresent(int synthIdx) const;
  bool hasVersionedSynthState() const;
  const PersistedSynthPatch& synthPatch(int synthIdx) const;
  float bpm() const;
''', "observer getters")

s = replace_once(s,
'''    SynthParams,
    SynthParam,
    SamplerPads,
''',
'''    SynthParams,
    SynthParam,
    SynthState,
    SynthStateAParams,
    SynthStateBParams,
    SamplerPads,
''', "observer paths")

s = replace_once(s,
'''  SynthParameters synthParameters_[2];
  float bpm_ = 100.0f;
''',
'''  SynthParameters synthParameters_[2];
  bool legacySynthParametersPresent_[2] = {false, false};
  bool synthStatePresent_ = false;
  int synthStateVersion_ = 0;
  PersistedSynthPatch synthPatch_[2];
  uint8_t synthPatchValueCount_[2] = {0, 0};
  float bpm_ = 100.0f;
''', "observer storage")

s = replace_once(s,
'''  void setSynthParameters(int synthIdx, const SynthParameters& params);
  const SynthParameters& getSynthParameters(int synthIdx) const;
  void setDrumEngineName(const std::string& name);
''',
'''  void setSynthParameters(int synthIdx, const SynthParameters& params);
  const SynthParameters& getSynthParameters(int synthIdx) const;
  void setLegacySynthParametersPresent(int synthIdx, bool present);
  bool legacySynthParametersPresent(int synthIdx) const;
  void setSynthPatch(int synthIdx, const PersistedSynthPatch& patch);
  const PersistedSynthPatch& getSynthPatch(int synthIdx) const;
  void clearVersionedSynthState();
  bool hasVersionedSynthState() const { return hasVersionedSynthState_; }
  void setDrumEngineName(const std::string& name);
''', "manager api")

# replace manager storage (second occurrence after private)
old = '''  SynthParameters synthParameters_[2];
  float bpm_ = 100.0f;
  bool songMode_ = false;
'''
new = '''  SynthParameters synthParameters_[2];
  bool legacySynthParametersPresent_[2] = {false, false};
  bool hasVersionedSynthState_ = false;
  PersistedSynthPatch synthPatch_[2];
  float bpm_ = 100.0f;
  bool songMode_ = false;
'''
s = replace_once(s, old, new, "manager storage")

# Streaming writer: replace old engine array with new synthState object.
s = replace_once(s,
'''  if (!writeLiteral(",\\\"synthEngines\\\":[")) return false;
  if (!writeString(synthEngineNames_[0])) return false;
  if (!writeChar(',')) return false;
  if (!writeString(synthEngineNames_[1])) return false;
  if (!writeChar(']')) return false;
''',
'''  if (!writeLiteral(",\\\"synthState\\\":{\\\"version\\\":")) return false;
  if (!writeInt(kSynthStateSchemaVersion)) return false;
  if (!writeLiteral(",\\\"aType\\\":")) return false;
  if (!writeString(synthPatch_[0].engineName)) return false;
  if (!writeLiteral(",\\\"aCount\\\":")) return false;
  if (!writeInt(synthPatch_[0].paramCount)) return false;
  if (!writeLiteral(",\\\"a\\\":[")) return false;
  for (int i = 0; i < PersistedSynthPatch::kMaxParams; ++i) {
    if (i > 0 && !writeChar(',')) return false;
    if (!writeFloat(synthPatch_[0].params[i])) return false;
  }
  if (!writeLiteral("],\\\"bType\\\":")) return false;
  if (!writeString(synthPatch_[1].engineName)) return false;
  if (!writeLiteral(",\\\"bCount\\\":")) return false;
  if (!writeInt(synthPatch_[1].paramCount)) return false;
  if (!writeLiteral(",\\\"b\\\":[")) return false;
  for (int i = 0; i < PersistedSynthPatch::kMaxParams; ++i) {
    if (i > 0 && !writeChar(',')) return false;
    if (!writeFloat(synthPatch_[1].params[i])) return false;
  }
  if (!writeLiteral("]}")) return false;
''', "streaming new synth state")

# Do not reserialize legacy raw synthParams.
start = '''  if (!writeLiteral(",\\\"synthParams\\\":[")) return false;
  for (int i = 0; i < 2; ++i) {
'''
pos = s.find(start)
if pos < 0:
    raise RuntimeError("legacy streaming synthParams start not found")
end_marker = '''  if (!writeChar(']')) return false;
  if (!writeLiteral(",\\\"synthDistortion\\\":[")) return false;
'''
end = s.find(end_marker, pos)
if end < 0:
    raise RuntimeError("legacy streaming synthParams end not found")
s = s[:pos] + '''  if (!writeLiteral(",\\\"synthDistortion\\\":[")) return false;
''' + s[end + len(end_marker):]

p.write_text(s, encoding="utf-8")

# --- scenes.cpp --------------------------------------------------------------
p, s = rw("scenes.cpp")
s = replace_once(s,
'''float valueToFloat(ArduinoJson::JsonVariantConst value, float defaultValue) {
  if (value.is<float>() || value.is<int>()) {
    return value.as<float>();
  }
  return defaultValue;
}
''',
'''float valueToFloat(ArduinoJson::JsonVariantConst value, float defaultValue) {
  if (value.is<float>() || value.is<int>()) {
    return value.as<float>();
  }
  return defaultValue;
}

bool isStableSynthEngineName(const std::string& name) {
  return name == "TB303" || name == "SID" || name == "AY" ||
         name == "SH101" || name == "SN76489" || name == "WAVEMORPH";
}
''', "engine name validator")

# object path synthState
s = replace_once(s,
'''      else if (lastKey_ == "genre") path = Path::Genre;
      else if (lastKey_ == "led") path = Path::Led;
''',
'''      else if (lastKey_ == "genre") path = Path::Genre;
      else if (lastKey_ == "synthState") path = Path::SynthState;
      else if (lastKey_ == "led") path = Path::Led;
''', "synthState object path")

# initialize and validate synth state around object lifecycle
s = replace_once(s,
'''  pushContext(Context::Type::Object, path);
  if (path == Path::Unknown) {
''',
'''  pushContext(Context::Type::Object, path);
  if (path == Path::SynthState) {
    synthStatePresent_ = true;
    synthStateVersion_ = 0;
    synthPatch_[0] = PersistedSynthPatch();
    synthPatch_[1] = PersistedSynthPatch();
    synthPatchValueCount_[0] = 0;
    synthPatchValueCount_[1] = 0;
  }
  if (path == Path::Unknown) {
''', "synthState init")

s = replace_once(s,
'''void SceneJsonObserver::onObjectEnd() {
  if (error_) return;
  popContext();
}
''',
'''void SceneJsonObserver::onObjectEnd() {
  if (error_) return;
  if (stackSize_ > 0 && stack_[stackSize_ - 1].path == Path::SynthState) {
    const bool valid =
        synthStateVersion_ == kSynthStateSchemaVersion &&
        isStableSynthEngineName(synthPatch_[0].engineName) &&
        isStableSynthEngineName(synthPatch_[1].engineName) &&
        synthPatch_[0].paramCount <= PersistedSynthPatch::kMaxParams &&
        synthPatch_[1].paramCount <= PersistedSynthPatch::kMaxParams &&
        synthPatchValueCount_[0] == PersistedSynthPatch::kMaxParams &&
        synthPatchValueCount_[1] == PersistedSynthPatch::kMaxParams;
    if (!valid) {
      error_ = true;
      return;
    }
  }
  popContext();
}
''', "synthState validation")

# array paths a/b
s = replace_once(s,
'''      } else if (parent.path == Path::Mute) {
        if (lastKey_ == "drums") path = Path::MuteDrums;
        else if (lastKey_ == "synth") path = Path::MuteSynth;
      }
''',
'''      } else if (parent.path == Path::Mute) {
        if (lastKey_ == "drums") path = Path::MuteDrums;
        else if (lastKey_ == "synth") path = Path::MuteSynth;
      } else if (parent.path == Path::SynthState) {
        if (lastKey_ == "a") path = Path::SynthStateAParams;
        else if (lastKey_ == "b") path = Path::SynthStateBParams;
      }
''', "synthState arrays")

# number handling before phrase
s = replace_once(s,
'''  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::PhraseCore) {
''',
'''  Path path = stack_[stackSize_ - 1].path;
  if (path == Path::SynthState) {
    const int ivalue = static_cast<int>(value);
    if (lastKey_ == "version") synthStateVersion_ = ivalue;
    else if (lastKey_ == "aCount") {
      if (ivalue < 0 || ivalue > PersistedSynthPatch::kMaxParams) { error_ = true; return; }
      synthPatch_[0].paramCount = static_cast<uint8_t>(ivalue);
    } else if (lastKey_ == "bCount") {
      if (ivalue < 0 || ivalue > PersistedSynthPatch::kMaxParams) { error_ = true; return; }
      synthPatch_[1].paramCount = static_cast<uint8_t>(ivalue);
    }
    return;
  }
  if (path == Path::SynthStateAParams || path == Path::SynthStateBParams) {
    const int voice = path == Path::SynthStateAParams ? 0 : 1;
    const int index = stack_[stackSize_ - 1].index;
    if (index < 0 || index >= PersistedSynthPatch::kMaxParams || value < 0.0 || value > 1.0) {
      error_ = true;
      return;
    }
    synthPatch_[voice].params[index] = static_cast<float>(value);
    synthPatchValueCount_[voice] = static_cast<uint8_t>(index + 1);
    return;
  }
  if (path == Path::PhraseCore) {
''', "synthState numeric decode")

# legacy presence marker
s = replace_once(s,
'''  if (path == Path::SynthParam) {
    int synthIdx = currentIndexFor(Path::SynthParams);
    if (synthIdx < 0 || synthIdx >= 2) return;
    float fval = static_cast<float>(value);
''',
'''  if (path == Path::SynthParam) {
    int synthIdx = currentIndexFor(Path::SynthParams);
    if (synthIdx < 0 || synthIdx >= 2) return;
    legacySynthParametersPresent_[synthIdx] = true;
    float fval = static_cast<float>(value);
''', "legacy params presence")

# synth state strings
s = replace_once(s,
'''    if (context.path == Path::State && lastKey_ == "drumEngine") {
      drumEngineName_ = value;
    } else if (context.path == Path::CustomPhrase) {
''',
'''    if (context.path == Path::State && lastKey_ == "drumEngine") {
      drumEngineName_ = value;
    } else if (context.path == Path::SynthState && lastKey_ == "aType") {
      synthPatch_[0].engineName = value;
    } else if (context.path == Path::SynthState && lastKey_ == "bType") {
      synthPatch_[1].engineName = value;
    } else if (context.path == Path::CustomPhrase) {
''', "synthState string decode")

# observer getters
s = replace_once(s,
'''const SynthParameters& SceneJsonObserver::synthParameters(int synthIdx) const {
  int clamped = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthParameters_[clamped];
}

float SceneJsonObserver::bpm() const { return bpm_; }
''',
'''const SynthParameters& SceneJsonObserver::synthParameters(int synthIdx) const {
  int clamped = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthParameters_[clamped];
}

bool SceneJsonObserver::legacySynthParametersPresent(int synthIdx) const {
  int clamped = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return legacySynthParametersPresent_[clamped];
}

bool SceneJsonObserver::hasVersionedSynthState() const {
  return synthStatePresent_ && !error_;
}

const PersistedSynthPatch& SceneJsonObserver::synthPatch(int synthIdx) const {
  int clamped = synthIdx < 0 ? 0 : synthIdx > 1 ? 1 : synthIdx;
  return synthPatch_[clamped];
}

float SceneJsonObserver::bpm() const { return bpm_; }
''', "observer getter implementations")

# default/wipe reset: occurs twice
needle = '''  synthParameters_[0] = SynthParameters();
  synthParameters_[1] = SynthParameters();
  drumEngineName_ = "808";
'''
repl = '''  synthParameters_[0] = SynthParameters();
  synthParameters_[1] = SynthParameters();
  legacySynthParametersPresent_[0] = false;
  legacySynthParametersPresent_[1] = false;
  clearVersionedSynthState();
  drumEngineName_ = "808";
'''
if s.count(needle) != 2:
    raise RuntimeError(f"default/wipe reset expected twice, got {s.count(needle)}")
s = s.replace(needle, repl)

# manager methods after legacy getter
s = replace_once(s,
'''const SynthParameters& SceneManager::getSynthParameters(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthParameters_[clampedSynth];
}

void SceneManager::setDrumEngineName(const std::string& name) { drumEngineName_ = name; }
''',
'''const SynthParameters& SceneManager::getSynthParameters(int synthIdx) const {
  int clampedSynth = clampSynthIndex(synthIdx);
  return synthParameters_[clampedSynth];
}

void SceneManager::setLegacySynthParametersPresent(int synthIdx, bool present) {
  legacySynthParametersPresent_[clampSynthIndex(synthIdx)] = present;
}

bool SceneManager::legacySynthParametersPresent(int synthIdx) const {
  return legacySynthParametersPresent_[clampSynthIndex(synthIdx)];
}

void SceneManager::setSynthPatch(int synthIdx, const PersistedSynthPatch& patch) {
  const int idx = clampSynthIndex(synthIdx);
  synthPatch_[idx] = patch;
  if (synthPatch_[idx].paramCount > PersistedSynthPatch::kMaxParams) {
    synthPatch_[idx].paramCount = PersistedSynthPatch::kMaxParams;
  }
  hasVersionedSynthState_ = true;
}

const PersistedSynthPatch& SceneManager::getSynthPatch(int synthIdx) const {
  return synthPatch_[clampSynthIndex(synthIdx)];
}

void SceneManager::clearVersionedSynthState() {
  hasVersionedSynthState_ = false;
  synthPatch_[0] = PersistedSynthPatch();
  synthPatch_[1] = PersistedSynthPatch();
}

void SceneManager::setDrumEngineName(const std::string& name) { drumEngineName_ = name; }
''', "manager synth patch methods")

# buildSceneDocument: replace legacy synthParams with synthState.
old = '''  ArduinoJson::JsonArray synthParams = state["synthParams"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < 2; ++i) {
    ArduinoJson::JsonObject param = synthParams.add<ArduinoJson::JsonObject>();
    param["cutoff"] = synthParameters_[i].cutoff;
    param["resonance"] = synthParameters_[i].resonance;
    param["envAmount"] = synthParameters_[i].envAmount;
    param["envDecay"] = synthParameters_[i].envDecay;
    param["oscType"] = synthParameters_[i].oscType;
  }
'''
new = '''  ArduinoJson::JsonObject synthState = state["synthState"].to<ArduinoJson::JsonObject>();
  synthState["version"] = kSynthStateSchemaVersion;
  synthState["aType"] = synthPatch_[0].engineName;
  synthState["aCount"] = synthPatch_[0].paramCount;
  ArduinoJson::JsonArray synthAParams = synthState["a"].to<ArduinoJson::JsonArray>();
  synthState["bType"] = synthPatch_[1].engineName;
  synthState["bCount"] = synthPatch_[1].paramCount;
  ArduinoJson::JsonArray synthBParams = synthState["b"].to<ArduinoJson::JsonArray>();
  for (int i = 0; i < PersistedSynthPatch::kMaxParams; ++i) {
    synthAParams.add(synthPatch_[0].params[i]);
    synthBParams.add(synthPatch_[1].params[i]);
  }
'''
s = replace_once(s, old, new, "document synth state")

# Evented commit new state + synth names + legacy presence.
s = replace_once(s,
'''  synthParameters_[0] = observer.synthParameters(0);
  synthParameters_[1] = observer.synthParameters(1);
  drumEngineName_ = observer.drumEngineName();
''',
'''  synthParameters_[0] = observer.synthParameters(0);
  synthParameters_[1] = observer.synthParameters(1);
  legacySynthParametersPresent_[0] = observer.legacySynthParametersPresent(0);
  legacySynthParametersPresent_[1] = observer.legacySynthParametersPresent(1);
  synthEngineNames_[0] = observer.synthEngineName(0);
  synthEngineNames_[1] = observer.synthEngineName(1);
  clearVersionedSynthState();
  if (observer.hasVersionedSynthState()) {
    setSynthPatch(0, observer.synthPatch(0));
    setSynthPatch(1, observer.synthPatch(1));
    synthEngineNames_[0] = synthPatch_[0].engineName;
    synthEngineNames_[1] = synthPatch_[1].engineName;
  }
  drumEngineName_ = observer.drumEngineName();
''', "evented synth commit")

p.write_text(s, encoding="utf-8")

# --- miniacid_engine.cpp -----------------------------------------------------
p, s = rw("src/dsp/miniacid_engine.cpp")
# Replace load parameter application section from log through applySynthParams calls.
old_start = '''  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: setting voice params...");
  const SynthParameters& paramsA = sceneManager_.getSynthParameters(0);
  const SynthParameters& paramsB = sceneManager_.getSynthParameters(1);

  auto clamp01 = [](float v) -> float {
'''
start = s.find(old_start)
if start < 0:
    raise RuntimeError("apply scene params start missing")
end_marker = '''  applySynthParams(0, paramsA);
  applySynthParams(1, paramsB);
  
  
  distortion303.setEnabled(distortion303Enabled);
'''
end = s.find(end_marker, start)
if end < 0:
    raise RuntimeError("apply scene params end missing")
replacement = '''  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: setting voice params...");
  auto clamp01 = [](float v) -> float {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  };

  if (sceneManager_.hasVersionedSynthState()) {
    for (int idx = 0; idx < 2; ++idx) {
      const PersistedSynthPatch& patch = sceneManager_.getSynthPatch(idx);
      setSynthEngine(idx, patch.engineName);
      if (!synthVoices_[idx]) continue;
      SynthVoiceState runtimeState = synthVoices_[idx]->getState();
      runtimeState.paramCount = std::min<uint8_t>(
          patch.paramCount, PersistedSynthPatch::kMaxParams);
      for (uint8_t p = 0; p < runtimeState.paramCount; ++p) {
        runtimeState.params[p] = clamp01(patch.params[p]);
      }
      synthVoices_[idx]->setState(runtimeState);
      synthEngineNames_[idx] = synthVoices_[idx]->getEngineName();
      sceneManager_.setSynthEngineName(idx, synthEngineNames_[idx]);
    }
  } else {
    // Legacy compatibility. TB303 raw values keep their historical units.
    // Non-TB engines with no legacy synthParams stay at engine-native defaults.
    for (int idx = 0; idx < 2; ++idx) {
      const SynthParameters& sp = sceneManager_.getSynthParameters(idx);
      if (TB303Voice* v303 = tb303Voice(idx)) {
        if (sceneManager_.legacySynthParametersPresent(idx)) {
          v303->setParameter(TB303ParamId::Cutoff, sp.cutoff);
          v303->setParameter(TB303ParamId::Resonance, sp.resonance);
          v303->setParameter(TB303ParamId::EnvAmount, sp.envAmount);
          v303->setParameter(TB303ParamId::EnvDecay, sp.envDecay);
          v303->setParameter(TB303ParamId::Oscillator, static_cast<float>(sp.oscType));
        }
        continue;
      }
      if (!sceneManager_.legacySynthParametersPresent(idx) || !synthVoices_[idx]) {
        continue;
      }
      // Historical non-TB scenes used these legacy field names as normalized
      // slots 0..3 and oscType/100 as slot 4. Preserve that decode-only path.
      const uint8_t count = synthVoices_[idx]->parameterCount();
      if (count > 0) synthVoices_[idx]->setParameterNormalized(0, clamp01(sp.cutoff));
      if (count > 1) synthVoices_[idx]->setParameterNormalized(1, clamp01(sp.resonance));
      if (count > 2) synthVoices_[idx]->setParameterNormalized(2, clamp01(sp.envAmount));
      if (count > 3) synthVoices_[idx]->setParameterNormalized(3, clamp01(sp.envDecay));
      if (count > 4) synthVoices_[idx]->setParameterNormalized(
          4, clamp01(static_cast<float>(sp.oscType) / 100.0f));
    }
  }
  
  
  distortion303.setEnabled(distortion303Enabled);
'''
s = s[:start] + replacement + s[end + len(end_marker):]

# Remove hidden genre patch projection on normal load, keep metadata and baseline.
s = replace_once(s,
'''  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: applyGenreTimbre...");
  // Restore genre state from scene before applying timbre/texture
''',
'''  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: restore genre metadata...");
  // Restore genre metadata. Normal Scene Load must not project genre sound over
  // the explicitly restored synth patch.
''', "genre load comment")

s = replace_once(s,
'''  // 1. Enforce Genre Timbre BASE (overwrites scene params to ensure genre identity)
  genreManager_.applyGenreTimbre(*this);
  
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: resetTextureBiasTracking...");
  // 2. Reset bias tracking so subsequent texture application is fresh delta from new base
  genreManager_.resetTextureBiasTracking();
  
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: applyTexture...");
  // 3. Apply texture (delta bias + FX)
  genreManager_.applyTexture(*this);

''',
'''  // Mark the decoded texture bias as already represented. Texture/Genre sound
  // projection remains available only through explicit user APPLY/MATERIALIZE.
  genreManager_.syncTextureBiasBaselineFromCurrentState();

''', "remove genre projection")

# Replace legacy save capture with normalized state capture.
old_start = '''  auto clamp01 = [](float v) -> float {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  };

  auto buildSynthParams = [&](int idx) -> SynthParameters {
'''
start = s.find(old_start, s.find("void MiniAcid::syncSceneStateToManager()"))
if start < 0:
    raise RuntimeError("sync legacy synth capture start missing")
end_marker = '''  SynthParameters paramsB = buildSynthParams(1);
  sceneManager_.setSynthParameters(1, paramsB);

'''
end = s.find(end_marker, start)
if end < 0:
    raise RuntimeError("sync legacy synth capture end missing")
replacement = '''  for (int idx = 0; idx < 2; ++idx) {
    PersistedSynthPatch patch;
    patch.engineName = currentSynthEngineName(idx);
    if (synthVoices_[idx]) {
      const SynthVoiceState runtimeState = synthVoices_[idx]->getState();
      patch.paramCount = std::min<uint8_t>(
          runtimeState.paramCount, PersistedSynthPatch::kMaxParams);
      for (uint8_t p = 0; p < patch.paramCount; ++p) {
        float value = runtimeState.params[p];
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        patch.params[p] = value;
      }
    }
    sceneManager_.setSynthPatch(idx, patch);
    sceneManager_.setLegacySynthParametersPresent(idx, false);
  }

'''
s = s[:start] + replacement + s[end + len(end_marker):]
p.write_text(s, encoding="utf-8")

# --- focused source regression ----------------------------------------------
(ROOT / "tests/test_synth_persistence_source_regressions.py").write_text(r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")

required = [
    "kSynthStateSchemaVersion = 1",
    "PersistedSynthPatch",
    "kMaxParams = 6",
    '"synthState"',
    '"aType"', '"bType"', '"aCount"', '"bCount"',
    "hasVersionedSynthState()",
    "legacySynthParametersPresent",
]
for token in required:
    if token not in scenes_h and token not in scenes_cpp:
        raise AssertionError(f"missing synth persistence contract: {token}")

if '\"synthParams\"' in scenes_h[scenes_h.index("bool SceneManager::writeSceneJson"):]:
    raise AssertionError("new streaming writer must not reserialize legacy synthParams")

apply_start = engine.index("void MiniAcid::applySceneStateFromManager()")
apply_end = engine.index("void MiniAcid::applyTextureFromScene_()", apply_start)
apply = engine[apply_start:apply_end]
for forbidden in ("genreManager_.applyGenreTimbre(*this)", "genreManager_.applyTexture(*this)"):
    if forbidden in apply:
        raise AssertionError(f"normal Scene Load still rewrites patch: {forbidden}")
if "syncTextureBiasBaselineFromCurrentState" not in apply:
    raise AssertionError("load must synchronize texture bias without applying it")
if "if (sceneManager_.hasVersionedSynthState())" not in apply:
    raise AssertionError("versioned synth state is not authoritative on load")
if "legacySynthParametersPresent(idx)" not in apply:
    raise AssertionError("legacy non-TB defaults cannot distinguish missing params")

sync_start = engine.index("void MiniAcid::syncSceneStateToManager()")
sync = engine[sync_start:]
if "getState()" not in sync or "setSynthPatch" not in sync:
    raise AssertionError("save path must capture normalized SwappableSynthVoice state")

print("synth persistence/load ownership source regressions: OK")
''', encoding="utf-8")

# Extend scene roundtrip with versioned A/B patch data.
p, t = rw("tests/test_scene_roundtrip.cpp")
t = replace_once(t,
'''  manager.setTrackVolume(static_cast<int>(VoiceId::SynthA), 0.37f);

  Scene& scene = manager.currentScene();
''',
'''  manager.setTrackVolume(static_cast<int>(VoiceId::SynthA), 0.37f);

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
''', "roundtrip populate patch")

t = replace_once(t,
'''  assert(near(manager.getTrackVolume(static_cast<int>(VoiceId::SynthA)),
              0.37f));

  assert(scene.feel.gridSteps == 32);
''',
'''  assert(near(manager.getTrackVolume(static_cast<int>(VoiceId::SynthA)),
              0.37f));
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
''', "roundtrip verify patch")

t = replace_once(t,
'''  assert(json.find("\\\"phraseCore\\\":[") != std::string::npos);

  destroyRoundTripFields(manager);
''',
'''  assert(json.find("\\\"phraseCore\\\":[") != std::string::npos);
  assert(json.find("\\\"synthState\\\":{\\\"version\\\":1") != std::string::npos);
  assert(json.find("\\\"synthParams\\\"") == std::string::npos);

  const std::string stableJson = json;
  destroyRoundTripFields(manager);
''', "roundtrip json assertions")

t = replace_once(t,
'''  assert(manager.loadScene(json));
  verifyRoundTrip(manager);
  return 0;
''',
'''  assert(manager.loadScene(json));
  verifyRoundTrip(manager);
  const std::string secondJson = manager.dumpCurrentScene();
  assert(secondJson == stableJson);

  // Unknown version is transactional: current state must remain intact.
  std::string malformed = stableJson;
  const std::string version1 = "\\\"synthState\\\":{\\\"version\\\":1";
  const size_t versionPos = malformed.find(version1);
  assert(versionPos != std::string::npos);
  malformed.replace(versionPos, version1.size(),
                    "\\\"synthState\\\":{\\\"version\\\":99");
  assert(!manager.loadScene(malformed));
  verifyRoundTrip(manager);
  return 0;
''', "roundtrip stable/malformed")
p.write_text(t, encoding="utf-8")

# Add source regression to standard host suite near other source checks.
p, run = rw("tests/run_host_tests.sh")
anchor = 'python3 "${ROOT_DIR}/tests/test_source_regressions.py"\n'
if anchor not in run:
    raise RuntimeError("run_host_tests source anchor missing")
run = run.replace(anchor, anchor + 'python3 "${ROOT_DIR}/tests/test_synth_persistence_source_regressions.py"\n', 1)
p.write_text(run, encoding="utf-8")

# Update contract doc with refined architecture.
p, doc = rw("docs/refactors/SYNTH_PERSISTENCE_LOAD_OWNERSHIP_0_9.md")ndoc = doc.replace(
"Close the remaining release blockers called out by PR #131 without mixing them into SID articulation (#139), TextureMode migration (#134), GenreManager ownership (#132), or TB303/DST stabilization.",
"Close the remaining release blockers called out by PR #131 as a standalone prerequisite PR against `dev_0.9`, without stacking on #131 or mixing into SID articulation (#139), TextureMode migration (#134), GenreManager ownership (#132), or TB303/DST stabilization. After merge, #131 must synchronize with `dev_0.9` and remove these closed blockers from its open list.")
doc = doc.replace(
"Malformed versioned synth state must fail the Scene transaction instead of partially applying TYPE or parameters.",
"Malformed or unknown versioned synth state must never partially apply TYPE or parameters. The complete versioned block is accepted atomically; when it is present but invalid this implementation fails the Scene transaction, leaving the current Scene/runtime untouched. Legacy/default fallback applies only when the versioned block is absent.")
doc = doc.replace(
"14. recovery/autosave leaves user dirty state unchanged.",
"14. recovery/autosave leaves user dirty state unchanged;\n15. Save -> Load -> Save is representation-stable for the versioned synth block.\n\n#134 boundary: this PR does not delete `TextureMode` or rewrite legacy `tex/amt`; #134 must not introduce synth persistence. After both land, run a combined legacy Scene regression before RC.")
p.write_text(doc, encoding="utf-8")

# Final sanity: old raw defaults remain only as legacy decode model and no
# persistence script touched TextureMode definitions.
for path in ("scenes.h", "scenes.cpp", "src/dsp/miniacid_engine.cpp"):
    text = (ROOT / path).read_text(encoding="utf-8")
    if "PersistedSynthPatch" not in text:
        raise RuntimeError(f"{path}: missing new synth patch integration")

print("#143 synth persistence transformation applied")
