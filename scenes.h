#ifndef SCENES_H
#define SCENES_H


#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include "ArduinoJson-v7.4.2.h"
#include "src/dsp/mini_dsp_params.h"
#include "src/dsp/genre_manager.h"
#include "json_evented.h"

namespace scene_json_detail {
inline bool writeChunk(std::string& writer, const char* data, size_t len) {
  writer.append(data, len);
  return true;
}

template <typename Writer>
auto writeChunkImpl(Writer& writer, const char* data, size_t len, int)
    -> decltype(writer.write(reinterpret_cast<const uint8_t*>(data), len), bool()) {
  size_t written = writer.write(reinterpret_cast<const uint8_t*>(data), len);
  return written == len;
}

template <typename Writer>
auto writeChunkImpl(Writer& writer, const char* data, size_t len, long)
    -> decltype(writer.write(data, len), bool()) {
  auto written = writer.write(data, len);
  return static_cast<size_t>(written) == len;
}

template <typename Writer>
auto writeChunkImpl(Writer& writer, const char* data, size_t len, ...)
    -> decltype(writer.write(static_cast<uint8_t>(0)), bool()) {
  for (size_t i = 0; i < len; ++i) {
    if (writer.write(static_cast<uint8_t>(data[i])) != 1) return false;
  }
  return true;
}

template <typename Writer>
bool writeChunk(Writer& writer, const char* data, size_t len) {
  return writeChunkImpl(writer, data, len, 0);
}
} // namespace scene_json_detail

struct DrumStep {
  uint8_t hit : 1 {0};
  uint8_t accent : 1 {0};
  uint8_t unused : 6 {0};
  uint8_t velocity = 100;
  int8_t timing = 0;
  uint8_t fx = 0;
  uint8_t fxParam = 0;
  uint8_t probability = 100;
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
  uint8_t note = 0;
  uint8_t slide : 1 {0};
  uint8_t accent : 1 {0};
  uint8_t ghost : 1 {0};
  uint8_t unused : 5 {0};
  uint8_t velocity = 100;
  int8_t timing = 0;
  uint8_t fx = 0;
  uint8_t fxParam = 0;
  uint8_t probability = 100;
};

struct SynthPattern {
  static constexpr int kSteps = 16;
  SynthStep steps[kSteps];
};

struct SynthParameters {
  float cutoff = 800.0f;
  float resonance = 0.6f;
  float envAmount = 400.0f;
  float envDecay = 420.0f;
  int oscType = 0;
};

enum class SongTrack : uint8_t {
  SynthA = 0,
  SynthB = 1,
  Drums = 2,
  Voice = 3,
};

struct SongPosition {
  static constexpr int kTrackCount = 4;
  int16_t patterns[kTrackCount] = {-1, -1, -1, -1};
};

struct Song {
  static constexpr int kMaxPositions = 128;
  SongPosition positions[kMaxPositions];
  int length = 1;
  bool reverse = false;
};

template <typename PatternType>
struct Bank {
  static constexpr int kPatterns = 8;
  PatternType patterns[kPatterns];
};

enum class LedMode : uint8_t {
  Off,
  StepTrig,
  Beat,
  MuteState
};

enum class VoiceId : uint8_t {
  SynthA,
  SynthB,
  DrumKick,
  DrumSnare,
  DrumHatC,
  DrumHatO,
  DrumTomM,
  DrumTomH,
  DrumRim,
  DrumClap,
  Count
};

using LedSource = VoiceId;

struct Rgb8 {
  uint8_t r, g, b;
};

struct LedSettings {
  LedMode mode = LedMode::Off;
  LedSource source = LedSource::SynthA;
  Rgb8 color = {255, 128, 0}; // Amber
  uint8_t brightness = 40;
  uint16_t flashMs = 40;
};

static constexpr int kBankCount = 2;
static constexpr int kPatternsPerPage = kBankCount * Bank<SynthPattern>::kPatterns; // 16
static constexpr int kMaxPages = 16;
static constexpr int kMaxPatterns = kMaxPages * kPatternsPerPage; // e.g. 16 * 16 = 256
static constexpr int kMaxGlobalPatterns = kMaxPatterns; // Alias for compatibility

inline int clampSongPatternIndex(int idx) {
  if (idx < -1) return -1;
  int max = kMaxGlobalPatterns - 1;
  if (idx > max) return max;
  return idx;
}

// Global ID Helpers
inline int songPatternPage(int globalIdx) {
  if (globalIdx < 0) return 0;
  return globalIdx / kPatternsPerPage;
}

inline int songPatternBank(int globalIdx) {
  if (globalIdx < 0) return -1;
  return (globalIdx % kPatternsPerPage) / Bank<SynthPattern>::kPatterns;
}

inline int songPatternIndexInBank(int globalIdx) {
  if (globalIdx < 0) return -1;
  return globalIdx % Bank<SynthPattern>::kPatterns;
}

inline int songPatternFromPageBankIndex(int page, int bank, int idx) {
  if (page < 0 || bank < 0 || idx < 0) return -1;
  return (page * kPatternsPerPage) + (bank * Bank<SynthPattern>::kPatterns) + idx;
}

// Legacy helper compatibility (default to page 0)
inline int songPatternFromBank(int bankIndex, int patternIndex) {
  return songPatternFromPageBankIndex(0, bankIndex, patternIndex);
}

struct SamplerPadState {
  uint32_t sampleId = 0;
  float volume = 1.0f;
  float pitch = 1.0f;
  uint32_t startFrame = 0;
  uint32_t endFrame = 0;
  uint8_t chokeGroup = 0;
  bool reverse = false;
  bool loop = false;
};

// FX Types
enum class StepFx : uint8_t { 
  None = 0, 
  Retrig = 1, 
  Reverse = 2 
};

// Tape mode enum for looper state machine
#include "src/dsp/tape_defs.h"

struct TapeState {
    // Mode & control
    TapeMode mode = TapeMode::Stop;
    TapePreset preset = TapePreset::Warm;
    uint8_t speed = 1;       // 0=0.5x, 1=1.0x, 2=2.0x
    bool fxEnabled = false;   // Master tape FX on/off
    
    // Macro controls (the 5 knobs)
    TapeMacro macro;
    
    // Looper volume (mix level of loop playback)
    float looperVolume = 0.55f;
    
    // Minimal extensions
    uint8_t space = 0;    // 0..100
    uint8_t movement = 0; // 0..100
    uint8_t groove = 0;   // 0..100
};

enum ScaleType {
    MINOR,
    MAJOR,
    DORIAN,
    PHRYGIAN,
    LYDIAN,
    MIXOLYDIAN,
    LOCRIAN,
    PENTATONIC_MJ,   // Major pentatonic
    PENTATONIC_MN,   // Minor pentatonic
    CHROMATIC        // All 12 notes
};

struct GeneratorParams {
    // Basic
    int minNotes = 4;
    int maxNotes = 12;
    int minOctave = 36;
    int maxOctave = 60;
    
    // Variations
    float swingAmount = 0.0f;        // 0-0.66 (0% - 66% swing)
    float velocityRange = 0.3f;      // 0-1 (variation amount)
    float ghostNoteProbability = 0.1f;// 0-1
    float microTimingAmount = 0.2f;  // 0-1
    
    // Musicality
    bool preferDownbeats = false;     
    bool scaleQuantize = true;      
    int scaleRoot = 0;               // 0=C, 1=C#, etc.
    ScaleType scale = DORIAN;         
};

struct VocalSettings {
    float pitch = 120.0f;
    float speed = 1.0f;
    float robotness = 0.8f;
    float volume = 1.0f;
};

struct FeelSettings {
    uint8_t gridSteps = 16;   // 8,16,32
    uint8_t timebase = 1;     // 0=Half, 1=Normal, 2=Double
    uint8_t patternBars = 1;  // 1,2,4,8
    bool lofiEnabled = false;
    uint8_t lofiAmount = 50;  // 0..100
    bool driveEnabled = false;
    uint8_t driveAmount = 70; // 0..100
    bool tapeEnabled = false;
};

struct GenreSettings {
    uint8_t generativeMode = 0;   // GenerativeMode enum value
    uint8_t textureMode = 0;      // TextureMode enum value
    uint8_t textureAmount = 70;   // 0..100 intensity
    bool regenerateOnApply = true; // true: SOUND+PATTERN, false: SOUND ONLY
    bool applyTempoOnApply = false; // true: SOUND+PATTERN+TEMPO
    bool curatedMode = true;      // true: only allowed Genre x Texture combos
    bool applySoundMacros = false; // true: Flavor change overwrites 303/Tape
};

struct DrumFX {
    float compression = 0.0f;
    float transientAttack = 0.0f;
    float transientSustain = 0.0f;
    float reverbMix = 0.0f;
    float reverbDecay = 0.5f;
};

struct Scene {
  Bank<DrumPatternSet> drumBanks[kBankCount];
  Bank<SynthPattern> synthABanks[kBankCount];
  Bank<SynthPattern> synthBBanks[kBankCount];
  SamplerPadState samplerPads[16];
  TapeState tape;
  FeelSettings feel;
  GenreSettings genre;
  DrumFX drumFX;
  Song songs[2];
  int activeSongSlot = 0;
  GrooveboxMode mode = GrooveboxMode::Minimal;
  uint8_t grooveFlavor = 0;
  float masterVolume = 0.6f;  // Default volume
  GeneratorParams generatorParams; 
  VocalSettings vocal;
  
  static constexpr int kMaxCustomPhrases = 16;
  static constexpr int kMaxPhraseLength = 32;
  char customPhrases[kMaxCustomPhrases][kMaxPhraseLength];
    
  LedSettings led;

  float trackVolumes[(int)VoiceId::Count] = {
      1.0f, 1.0f, // Synth A, B
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f // Drums
  };
};

class SceneJsonObserver : public JsonObserver {
public:
  explicit SceneJsonObserver(Scene& scene, float defaultBpm = 100.0f);

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
  bool drumMute(int idx) const;
  bool synthMute(int idx) const;
  bool synthDistortionEnabled(int idx) const;
  bool synthDelayEnabled(int idx) const;
  const SynthParameters& synthParameters(int synthIdx) const;
  float bpm() const;
  const Song& song() const;
  bool hasSong() const;
  bool songMode() const;
  int songPosition() const;
  bool loopMode() const;
  int loopStartRow() const;
  int loopEndRow() const;
  const std::string& drumEngineName() const;
  GrooveboxMode mode() const;

private:
  enum class Path {
    Root,
    DrumBanks,
    DrumBank,
    DrumPatternSet,
    DrumVoice,
    DrumHitArray,
    DrumAccentArray,
    DrumFxArray,
    DrumFxParamArray,
    DrumProbabilityArray,
    SynthABanks,
    SynthABank,
    SynthBBanks,
    SynthBBank,
    SynthPattern,
    SynthStep,
    State,
    SynthPatternIndex,
    SynthBankIndex,
    Mute,
    MuteDrums,
    MuteSynth,
    SynthDistortion,
    GeneratorParams,
    SynthDelay,
    SynthParams,
    SynthParam,
    SamplerPads,
    SamplerPad,
    Tape,
    Feel,
    Genre,
    Led,
    LedColorArray,
    Songs,
    Song,
    SongPositions,
    SongPosition,
    CustomPhrases,
    CustomPhrase,
    Vocal,
    TrackVolumes,
    DrumFX,
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
  void handlePrimitiveNumber(double value, bool isInteger);
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
  bool drumMute_[DrumPatternSet::kVoices] = {false, false, false, false, false, false, false, false};
  bool synthMute_[2] = {false, false};
  bool synthDistortion_[2] = {false, false};
  bool synthDelay_[2] = {false, false};
  SynthParameters synthParameters_[2];
  float bpm_ = 100.0f;
  Song song_;
  bool hasSong_ = false;
  bool songMode_ = false;
  int songPosition_ = 0;
  bool loopMode_ = false;
  int loopStartRow_ = 0;
  int loopEndRow_ = 0;
  std::string drumEngineName_ = "808";
};

class SceneManager {
public:
  SceneManager();
  void loadDefaultScene();
  
  // Spotlight Paging
  void setPage(int pageIndex);
  int currentPageIndex() const { return currentPageIndex_; }
  bool saveCurrentPage() const;
  bool loadCurrentPage();
  
  Scene& currentScene();
  const Scene& currentScene() const;

  const DrumPatternSet& getCurrentDrumPattern() const;
  DrumPatternSet& editCurrentDrumPattern();

  const SynthPattern& getCurrentSynthPattern(int synthIndex) const;
  SynthPattern& editCurrentSynthPattern(int synthIndex);
  const SynthPattern& getSynthPattern(int synthIndex, int patternIndex) const;
  SynthPattern& editSynthPattern(int synthIndex, int patternIndex);
  const DrumPatternSet& getDrumPatternSet(int patternIndex) const;
  DrumPatternSet& editDrumPatternSet(int patternIndex);

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

  void setDrumMute(int voiceIdx, bool mute);
  bool getDrumMute(int voiceIdx) const;
  void setSynthMute(int synthIdx, bool mute);
  bool getSynthMute(int synthIdx) const;
  void setSynthDistortionEnabled(int synthIdx, bool enabled);
  bool getSynthDistortionEnabled(int synthIdx) const;
  void setSynthDelayEnabled(int synthIdx, bool enabled);
  bool getSynthDelayEnabled(int synthIdx) const;
  void setSynthParameters(int synthIdx, const SynthParameters& params);
  const SynthParameters& getSynthParameters(int synthIdx) const;
  void setDrumEngineName(const std::string& name);
  const std::string& getDrumEngineName() const;
  void setMode(GrooveboxMode mode);
  GrooveboxMode getMode() const;
  void setGrooveFlavor(int flavor);
  int getGrooveFlavor() const;
  void setBpm(float bpm);
  float getBpm() const;

  const Song& song() const;
  Song& editSong();
  void setSongPattern(int position, SongTrack track, int patternIndex);
  void clearSongPattern(int position, SongTrack track);
  int songPattern(int position, SongTrack track) const;
  int songPatternAtSlot(int slot, int position, SongTrack track) const;
  int songLengthAtSlot(int slot) const;
  void setSongLength(int length);
  int songLength() const;
  void setSongPosition(int position);
  int getSongPosition() const;
  void setSongMode(bool enabled);
  bool songMode() const;
  void setLoopMode(bool enabled);
  bool loopMode() const;
  void setLoopRange(int startRow, int endRow);
  int loopStartRow() const;
  int loopEndRow() const;
  
  void setSongReverse(bool reverse);
  bool isSongReverse() const;
  bool isSongReverseAtSlot(int slot) const;
  void mergeSongs();
  void alternateSongs();
  
  int activeSongSlot() const;
  void setActiveSongSlot(int slot);

  void setTrackVolume(int voiceIdx, float volume);
  float getTrackVolume(int voiceIdx) const;

  template <typename TWriter>
  bool writeSceneJson(TWriter&& writer) const;
  template <typename TReader>
  bool loadSceneJson(TReader&& reader);
  template <typename TReader>
  bool loadSceneEvented(TReader&& reader);

  // static constexpr size_t sceneJsonCapacity();

private:
  int clampPatternIndex(int idx) const;
  int clampBankIndex(int idx) const;
  int clampSynthIndex(int idx) const;
  int clampSongPosition(int position) const;
  int clampSongLength(int length) const;
  int songTrackToIndex(SongTrack track) const;
  void trimSongLength();
  void clampLoopRange();
  void clearSongData(Song& song) const;
  void buildSceneDocument(ArduinoJson::JsonDocument& doc) const;
  bool applySceneDocument(const ArduinoJson::JsonDocument& doc);
  bool loadSceneEventedWithReader(JsonVisitor::NextChar nextChar);

  Scene* scene_;
  int drumPatternIndex_ = 0;
  int synthPatternIndex_[2] = {0, 0};
  int drumBankIndex_ = 0;
  int synthBankIndex_[2] = {0, 0};
  bool drumMute_[DrumPatternSet::kVoices] = {false, false, false, false, false, false, false, false};
  bool synthMute_[2] = {false, false};
  bool synthDistortion_[2] = {false, false};
  bool synthDelay_[2] = {false, false};
  SynthParameters synthParameters_[2];
  float bpm_ = 100.0f;
  bool songMode_ = false;
  int songPosition_ = 0;
  bool loopMode_ = false;
  int loopStartRow_ = 0;
  int loopEndRow_ = 0;
  std::string drumEngineName_ = "808";
  GrooveboxMode mode_ = GrooveboxMode::Minimal;
  int grooveFlavor_ = 0;
  int currentPageIndex_ = 0;
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
  using WriterType = typename std::remove_reference<TWriter>::type;
  WriterType& out = writer;

  auto writeChunk = [&](const char* data, size_t len) -> bool {
    return scene_json_detail::writeChunk(out, data, len);
  };
  auto writeLiteral = [&](const char* literal) -> bool {
    return writeChunk(literal, std::strlen(literal));
  };
  auto writeChar = [&](char c) -> bool {
    return writeChunk(&c, 1);
  };
  auto writeBool = [&](bool value) -> bool {
    return writeLiteral(value ? "true" : "false");
  };
  auto writeInt = [&](int value) -> bool {
    char buffer[16];
    int written = std::snprintf(buffer, sizeof(buffer), "%d", value);
    if (written < 0 || written >= static_cast<int>(sizeof(buffer))) return false;
    return writeChunk(buffer, static_cast<size_t>(written));
  };
  auto writeFloat = [&](float value) -> bool {
    char buffer[24];
    int written = std::snprintf(buffer, sizeof(buffer), "%.6g", static_cast<double>(value));
    if (written < 0 || written >= static_cast<int>(sizeof(buffer))) return false;
    return writeChunk(buffer, static_cast<size_t>(written));
  };
  auto writeBoolArray = [&](const bool* values, int count) -> bool {
    for (int i = 0; i < count; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeBool(values[i])) return false;
    }
    return true;
  };
  auto writeString = [&](const std::string& value) -> bool {
    if (!writeChar('"')) return false;
    for (char ch : value) {
      if (ch == '"' || ch == '\\') {
        if (!writeChar('\\')) return false;
      }
      if (!writeChar(ch)) return false;
    }
    return writeChar('"');
  };
  auto writeDrumPattern = [&](const DrumPattern& pattern) -> bool {
    if (!writeLiteral("{\"hit\":[")) return false;
    for (int i = 0; i < DrumPattern::kSteps; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeBool(pattern.steps[i].hit)) return false;
    }
    if (!writeLiteral("],\"accent\":[")) return false;
    for (int i = 0; i < DrumPattern::kSteps; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeBool(pattern.steps[i].accent)) return false;
    }
    // New FX arrays (only write if needed to save space? strict JSON usually requires consistency, but we can make them optional in parser)
    // For now, let's write them to be safe
    if (!writeLiteral("],\"fx\":[")) return false;
    for (int i = 0; i < DrumPattern::kSteps; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeInt(pattern.steps[i].fx)) return false;
    }
    if (!writeLiteral("],\"fxp\":[")) return false;
    for (int i = 0; i < DrumPattern::kSteps; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeInt(pattern.steps[i].fxParam)) return false;
    }
    if (!writeLiteral("],\"prb\":[")) return false;
    for (int i = 0; i < DrumPattern::kSteps; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeInt(pattern.steps[i].probability)) return false;
    }
    return writeLiteral("]}");
  };
  auto writeDrumBank = [&](const Bank<DrumPatternSet>& bank) -> bool {
    if (!writeChar('[')) return false;
    for (int p = 0; p < Bank<DrumPatternSet>::kPatterns; ++p) {
      if (p > 0 && !writeChar(',')) return false;
      if (!writeChar('[')) return false;
      for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
        if (v > 0 && !writeChar(',')) return false;
        if (!writeDrumPattern(bank.patterns[p].voices[v])) return false;
      }
      if (!writeChar(']')) return false;
    }
    return writeChar(']');
  };
  auto writeDrumBanks = [&](const Bank<DrumPatternSet>* banks) -> bool {
    if (!writeChar('[')) return false;
    for (int b = 0; b < kBankCount; ++b) {
      if (b > 0 && !writeChar(',')) return false;
      if (!writeDrumBank(banks[b])) return false;
    }
    return writeChar(']');
  };
  auto writeSynthPattern = [&](const SynthPattern& pattern) -> bool {
    if (!writeChar('[')) return false;
    for (int i = 0; i < SynthPattern::kSteps; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeLiteral("{\"note\":")) return false;
      if (!writeInt(pattern.steps[i].note)) return false;
      if (!writeLiteral(",\"slide\":")) return false;
      if (!writeBool(pattern.steps[i].slide)) return false;
      if (!writeLiteral(",\"accent\":")) return false;
      if (!writeBool(pattern.steps[i].accent)) return false;
      if (!writeLiteral(",\"fx\":")) return false;
      if (!writeInt(pattern.steps[i].fx)) return false;
      if (!writeLiteral(",\"fxp\":")) return false;
      if (!writeInt(pattern.steps[i].fxParam)) return false;
      if (!writeLiteral(",\"prb\":")) return false;
      if (!writeInt(pattern.steps[i].probability)) return false;
      if (!writeChar('}')) return false;
    }
    return writeChar(']');
  };
  auto writeSynthBank = [&](const Bank<SynthPattern>& bank) -> bool {
    if (!writeChar('[')) return false;
    for (int p = 0; p < Bank<SynthPattern>::kPatterns; ++p) {
      if (p > 0 && !writeChar(',')) return false;
      if (!writeSynthPattern(bank.patterns[p])) return false;
    }
    return writeChar(']');
  };
  auto writeSynthBanks = [&](const Bank<SynthPattern>* banks) -> bool {
    if (!writeChar('[')) return false;
    for (int b = 0; b < kBankCount; ++b) {
      if (b > 0 && !writeChar(',')) return false;
      if (!writeSynthBank(banks[b])) return false;
    }
    return writeChar(']');
  };

  if (!writeChar('{')) return false;
  if (!writeLiteral("\"drumBanks\":")) return false;
  if (!writeDrumBanks(scene_->drumBanks)) return false;

  if (!writeLiteral(",\"synthABanks\":")) return false;
  if (!writeSynthBanks(scene_->synthABanks)) return false;

  if (!writeLiteral(",\"synthBBanks\":")) return false;
  if (!writeSynthBanks(scene_->synthBBanks)) return false;

  auto writeSong = [&](const Song& s) -> bool {
    if (!writeChar('{')) return false;
    int songLen = s.length;
    if (songLen < 1) songLen = 1;
    if (songLen > Song::kMaxPositions) songLen = Song::kMaxPositions;
    
    if (!writeLiteral("\"length\":")) return false;
    if (!writeInt(songLen)) return false;
    if (!writeLiteral(",\"positions\":[")) return false;
    for (int i = 0; i < songLen; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeChar('{')) return false;
      if (!writeLiteral("\"a\":")) return false;
      if (!writeInt(s.positions[i].patterns[0])) return false;
      if (!writeLiteral(",\"b\":")) return false;
      if (!writeInt(s.positions[i].patterns[1])) return false;
      if (!writeLiteral(",\"drums\":")) return false;
      if (!writeInt(s.positions[i].patterns[2])) return false;
      if (!writeLiteral(",\"voice\":")) return false;
      if (!writeInt(s.positions[i].patterns[3])) return false;
      if (!writeChar('}')) return false;
    }
    if (!writeChar(']')) return false;
    // Add reverse flag
    if (!writeLiteral(",\"reverse\":")) return false;
    if (!writeBool(s.reverse)) return false;
    
    return writeChar('}');
  };

  if (!writeLiteral(",\"songs\":[")) return false;
  if (!writeSong(scene_->songs[0])) return false;
  if (!writeChar(',')) return false;
  if (!writeSong(scene_->songs[1])) return false;
  if (!writeChar(']')) return false;

  if (!writeLiteral(",\"state\":{")) return false;
  if (!writeLiteral("\"drumPatternIndex\":")) return false;
  if (!writeInt(drumPatternIndex_)) return false;
  if (!writeLiteral(",\"bpm\":")) return false;
  if (!writeFloat(bpm_)) return false;
  if (!writeLiteral(",\"songMode\":")) return false;
  if (!writeBool(songMode_)) return false;
  if (!writeLiteral(",\"songPosition\":")) return false;
  if (!writeInt(clampSongPosition(songPosition_))) return false;
  if (!writeLiteral(",\"activeSongSlot\":")) return false;
  if (!writeInt(scene_->activeSongSlot)) return false;
  if (!writeLiteral(",\"loopMode\":")) return false;
  if (!writeBool(loopMode_)) return false;
  if (!writeLiteral(",\"loopStart\":")) return false;
  if (!writeInt(loopStartRow_)) return false;
  if (!writeLiteral(",\"loopEnd\":")) return false;
  if (!writeInt(loopEndRow_)) return false;
  if (!writeLiteral(",\"synthPatternIndex\":[")) return false;
  if (!writeInt(synthPatternIndex_[0])) return false;
  if (!writeChar(',')) return false;
  if (!writeInt(synthPatternIndex_[1])) return false;
  if (!writeChar(']')) return false;
  if (!writeLiteral(",\"drumBankIndex\":")) return false;
  if (!writeInt(drumBankIndex_)) return false;
  if (!writeLiteral(",\"drumEngine\":")) return false;
  if (!writeString(drumEngineName_)) return false;
  if (!writeLiteral(",\"synthBankIndex\":[")) return false;
  if (!writeInt(synthBankIndex_[0])) return false;
  if (!writeChar(',')) return false;
  if (!writeInt(synthBankIndex_[1])) return false;
  if (!writeChar(']')) return false;
  if (!writeLiteral(",\"mute\":{")) return false;
  if (!writeLiteral("\"drums\":[")) return false;
  if (!writeBoolArray(drumMute_, DrumPatternSet::kVoices)) return false;
  if (!writeLiteral("],\"synth\":[")) return false;
  if (!writeBoolArray(synthMute_, 2)) return false;
  if (!writeChar(']')) return false;
  if (!writeChar('}')) return false;
  if (!writeLiteral(",\"synthParams\":[")) return false;
  for (int i = 0; i < 2; ++i) {
    if (i > 0 && !writeChar(',')) return false;
    if (!writeLiteral("{\"cutoff\":")) return false;
    if (!writeFloat(synthParameters_[i].cutoff)) return false;
    if (!writeLiteral(",\"resonance\":")) return false;
    if (!writeFloat(synthParameters_[i].resonance)) return false;
    if (!writeLiteral(",\"envAmount\":")) return false;
    if (!writeFloat(synthParameters_[i].envAmount)) return false;
    if (!writeLiteral(",\"envDecay\":")) return false;
    if (!writeFloat(synthParameters_[i].envDecay)) return false;
    if (!writeLiteral(",\"oscType\":")) return false;
    if (!writeInt(synthParameters_[i].oscType)) return false;
    if (!writeChar('}')) return false;
  }
  if (!writeChar(']')) return false;
  if (!writeLiteral(",\"synthDistortion\":[")) return false;
  if (!writeBool(synthDistortion_[0])) return false;
  if (!writeChar(',')) return false;
  if (!writeBool(synthDistortion_[1])) return false;
  if (!writeChar(']')) return false;
  if (!writeLiteral(",\"synthDelay\":[")) return false;
  if (!writeBool(synthDelay_[0])) return false;
  if (!writeChar(',')) return false;
  if (!writeBool(synthDelay_[1])) return false;
  if (!writeChar(']')) return false;
  if (!writeLiteral(",\"masterVolume\":")) return false;
  if (!writeFloat(scene_->masterVolume)) return false;

  if (!writeLiteral(",\"feel\":{\"grid\":")) return false;
  if (!writeInt(scene_->feel.gridSteps)) return false;
  if (!writeLiteral(",\"tb\":")) return false;
  if (!writeInt(scene_->feel.timebase)) return false;
  if (!writeLiteral(",\"bars\":")) return false;
  if (!writeInt(scene_->feel.patternBars)) return false;
  if (!writeLiteral(",\"lofi\":")) return false;
  if (!writeBool(scene_->feel.lofiEnabled)) return false;
  if (!writeLiteral(",\"lofiAmt\":")) return false;
  if (!writeInt(scene_->feel.lofiAmount)) return false;
  if (!writeLiteral(",\"drive\":")) return false;
  if (!writeBool(scene_->feel.driveEnabled)) return false;
  if (!writeLiteral(",\"driveAmt\":")) return false;
  if (!writeInt(scene_->feel.driveAmount)) return false;
  if (!writeLiteral(",\"tape\":")) return false;
  if (!writeBool(scene_->feel.tapeEnabled)) return false;
  if (!writeChar('}')) return false;

  if (!writeLiteral(",\"genre\":{\"gen\":")) return false;
  if (!writeInt(scene_->genre.generativeMode)) return false;
  if (!writeLiteral(",\"tex\":")) return false;
  if (!writeInt(scene_->genre.textureMode)) return false;
  if (!writeLiteral(",\"amt\":")) return false;
  if (!writeInt(scene_->genre.textureAmount)) return false;
  if (!writeLiteral(",\"regen\":")) return false;
  if (!writeBool(scene_->genre.regenerateOnApply)) return false;
  if (!writeLiteral(",\"tempo\":")) return false;
  if (!writeBool(scene_->genre.applyTempoOnApply)) return false;
  if (!writeLiteral(",\"cur\":")) return false;
  if (!writeBool(scene_->genre.curatedMode)) return false;
  if (!writeChar('}')) return false;

  if (!writeLiteral(",\"trackVolumes\":[")) return false;
  for (int i = 0; i < (int)VoiceId::Count; ++i) {
    if (i > 0 && !writeChar(',')) return false;
    if (!writeFloat(scene_->trackVolumes[i])) return false;
  }
  if (!writeChar(']')) return false;

  if (!writeLiteral(",\"samplerPads\":[")) return false;
  for (int i = 0; i < 16; ++i) {
    if (i > 0 && !writeChar(',')) return false;
    const auto& p = scene_->samplerPads[i];
    if (!writeLiteral("{\"id\":")) return false;
    if (!writeInt(p.sampleId)) return false;
    if (!writeLiteral(",\"vol\":")) return false;
    if (!writeFloat(p.volume)) return false;
    if (!writeLiteral(",\"pch\":")) return false;
    if (!writeFloat(p.pitch)) return false;
    if (!writeLiteral(",\"str\":")) return false;
    if (!writeInt(p.startFrame)) return false;
    if (!writeLiteral(",\"end\":")) return false;
    if (!writeInt(p.endFrame)) return false;
    if (!writeLiteral(",\"chk\":")) return false;
    if (!writeInt(p.chokeGroup)) return false;
    if (!writeLiteral(",\"rev\":")) return false;
    if (!writeBool(p.reverse)) return false;
    if (!writeLiteral(",\"lop\":")) return false;
    if (!writeBool(p.loop)) return false;
    if (!writeChar('}')) return false;
  }
  if (!writeChar(']')) return false;
  // Serial.print(".");

  if (!writeLiteral(",\"tape\":{\"mode\":")) return false;
  if (!writeInt(static_cast<int>(scene_->tape.mode))) return false;
  if (!writeLiteral(",\"preset\":")) return false;
  if (!writeInt(static_cast<int>(scene_->tape.preset))) return false;
  if (!writeLiteral(",\"speed\":")) return false;
  if (!writeInt(scene_->tape.speed)) return false;
  if (!writeLiteral(",\"fxEnabled\":")) return false;
  if (!writeBool(scene_->tape.fxEnabled)) return false;
  if (!writeLiteral(",\"wow\":")) return false;
  if (!writeInt(scene_->tape.macro.wow)) return false;
  if (!writeLiteral(",\"age\":")) return false;
  if (!writeInt(scene_->tape.macro.age)) return false;
  if (!writeLiteral(",\"sat\":")) return false;
  if (!writeInt(scene_->tape.macro.sat)) return false;
  if (!writeLiteral(",\"tone\":")) return false;
  if (!writeInt(scene_->tape.macro.tone)) return false;
  if (!writeLiteral(",\"crush\":")) return false;
  if (!writeInt(scene_->tape.macro.crush)) return false;
  if (!writeLiteral(",\"vol\":")) return false;
  if (!writeFloat(scene_->tape.looperVolume)) return false;
  if (!writeLiteral(",\"space\":")) return false;
  if (!writeInt(scene_->tape.space)) return false;
  if (!writeLiteral(",\"movement\":")) return false;
  if (!writeInt(scene_->tape.movement)) return false;
  if (!writeLiteral(",\"groove\":")) return false;
  if (!writeInt(scene_->tape.groove)) return false;
  if (!writeLiteral("},\"led\":{\"mode\":")) return false;
  if (!writeInt(static_cast<int>(scene_->led.mode))) return false;
  if (!writeLiteral(",\"src\":")) return false;
  if (!writeInt(static_cast<int>(scene_->led.source))) return false;
  if (!writeLiteral(",\"clr\":[")) return false;
  if (!writeInt(scene_->led.color.r)) return false;
  if (!writeChar(',')) return false;
  if (!writeInt(scene_->led.color.g)) return false;
  if (!writeChar(',')) return false;
  if (!writeInt(scene_->led.color.b)) return false;
  if (!writeLiteral("],\"bri\":")) return false;
  if (!writeInt(scene_->led.brightness)) return false;
  if (!writeLiteral(",\"fls\":")) return false;
  if (!writeInt(scene_->led.flashMs)) return false;
  if (!writeChar('}')) return false;  // Close led object
  // Serial.print(".");
  
  if (!writeLiteral(",\"vocal\":{\"pch\":")) return false;
  if (!writeFloat(scene_->vocal.pitch)) return false;
  if (!writeLiteral(",\"spd\":")) return false;
  if (!writeFloat(scene_->vocal.speed)) return false;
  if (!writeLiteral(",\"rob\":")) return false;
  if (!writeFloat(scene_->vocal.robotness)) return false;
  if (!writeLiteral(",\"vol\":")) return false;
  if (!writeFloat(scene_->vocal.volume)) return false;
  if (!writeChar('}')) return false;  // Close vocal object
  
  if (!writeLiteral(",\"mode\":")) return false;
  if (!writeInt(static_cast<int>(mode_))) return false;
  if (!writeLiteral(",\"flv\":")) return false;
  if (!writeInt(grooveFlavor_)) return false;

  if (!writeLiteral(",\"drumFX\":{\"comp\":")) return false;
  if (!writeFloat(scene_->drumFX.compression)) return false;
  if (!writeLiteral(",\"tAtt\":")) return false;
  if (!writeFloat(scene_->drumFX.transientAttack)) return false;
  if (!writeLiteral(",\"tSus\":")) return false;
  if (!writeFloat(scene_->drumFX.transientSustain)) return false;
  if (!writeLiteral(",\"rMix\":")) return false;
  if (!writeFloat(scene_->drumFX.reverbMix)) return false;
  if (!writeLiteral(",\"rDec\":")) return false;
  if (!writeFloat(scene_->drumFX.reverbDecay)) return false;
  if (!writeChar('}')) return false;

  if (!writeLiteral(",\"customPhrases\":[")) return false;
  for (int i = 0; i < Scene::kMaxCustomPhrases; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeString(scene_->customPhrases[i])) return false;
  }
  if (!writeChar(']')) return false;
  
  bool finalOk = writeLiteral("}}");  // Close state and root
  // if (finalOk) Serial.println(" Done.");
  // else Serial.println(" FAILED at final literal.");
  return finalOk;
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
  static size_t bytesRead = 0;
  bytesRead = 0;
  JsonVisitor::NextChar nextChar = [&reader]() -> int { 
    int c = reader.read();
    if (c >= 0) bytesRead++;
    return c; 
  };
  return loadSceneEventedWithReader(nextChar);
}
#endif // SCENES_H
