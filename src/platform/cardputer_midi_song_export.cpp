#include "cardputer_midi_song_export.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#include "cardputer_sd.h"
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "src/audio/pattern_paging.h"
#include "src/midi/midi_device_profile_runtime.h"
#include "src/midi/midi_output_route_projection.h"
#include "src/phrase/runtime_synth_events.h"
#include "src/state/melody_promotion.h"
#include "src/state/scene_revision.h"
#include "src/storage/data_root.h"

namespace {

class SdSongExportStorage final : public GroovePuterMidi::ISongExportStorage {
 public:
  bool ensureDirectory(const char* path) override {
#if defined(ARDUINO)
    if (!GroovePuterPlatform::ensureCardputerSdMounted()) return false;
#endif
    return SD.exists(path) || SD.mkdir(path);
  }

  bool exists(const char* path) const override { return SD.exists(path); }

  bool remove(const char* path) override {
    return !SD.exists(path) || SD.remove(path);
  }

  bool rename(const char* from, const char* to) override {
    return !SD.exists(to) && SD.rename(from, to);
  }

  bool beginWrite(const char* path) override {
    if (writeFile_) return false;
    if (SD.exists(path) && !SD.remove(path)) return false;
    writeFile_ = SD.open(path, FILE_WRITE);
    return static_cast<bool>(writeFile_);
  }

  std::size_t write(const uint8_t* data, std::size_t length) override {
    if (!writeFile_) return 0;
    return writeFile_.write(data, length);
  }

  bool closeWrite() override {
    if (!writeFile_) return false;
    const bool wasOpen = static_cast<bool>(writeFile_);
    writeFile_.flush();
    writeFile_.close();
    return wasOpen;
  }

  bool openRead(const char* path) override {
    if (readFile_) return false;
    readFile_ = SD.open(path, FILE_READ);
    return static_cast<bool>(readFile_);
  }

  uint32_t readSize() const override {
    if (!readFile_) return 0;
    const std::size_t value = readFile_.size();
    return value > std::numeric_limits<uint32_t>::max()
        ? 0
        : static_cast<uint32_t>(value);
  }

  bool readAt(uint32_t offset, uint8_t* data,
              std::size_t length) override {
    if (!readFile_ || !readFile_.seek(offset)) return false;
    return readFile_.read(data, length) == length;
  }

  bool closeRead() override {
    if (!readFile_) return false;
    readFile_.close();
    return true;
  }

  uint32_t size(const char* path) const override {
    File file = SD.open(path, FILE_READ);
    if (!file) return 0;
    const std::size_t value = file.size();
    file.close();
    return value > std::numeric_limits<uint32_t>::max()
        ? 0
        : static_cast<uint32_t>(value);
  }

 private:
  File writeFile_;
  File readFile_;
};

class SongExportMaterials final
    : public GroovePuterMidi::ISongExportMaterialSource {
 public:
  explicit SongExportMaterials(std::string projectName, MiniAcid& engine)
      : projectName_(std::move(projectName)), engine_(engine) {}

  bool loadPage(uint8_t page, const Scene*& out) override {
    Scene& scratch = sceneTransactionScratch();
    if (!PatternPagingService::loadPage(page, scratch, false)) return false;
    out = &scratch;
    return true;
  }

  bool loadMelody(uint8_t voice, uint8_t globalSlot,
                  PhraseRuntime::RuntimeSynthEventBuffer& out) override {
    const GroovePuterMaterial::MaterialAddress address{voice, globalSlot};
    return MelodyPromotion::loadMaterial(
        MelodyPromotion::defaultFileSystem(), projectName_, address, out);
  }

  bool snapshotStillCurrent(uint32_t revision) const override {
    return !engine_.isPlaying() &&
        GroovePuterState::sceneRevisionSnapshot().currentRevision == revision;
  }

 private:
  std::string projectName_;
  MiniAcid& engine_;
};

bool captureSnapshot(MiniAcid& engine,
                     GroovePuterMidi::MidiSongExportSnapshot& snapshot) {
  if (engine.isPlaying()) return false;
  const Scene& scene = engine.sceneManager().currentScene();
  const int songSlot = scene.activeSongSlot;
  if (songSlot < 0 || songSlot > 1) return false;
  const GroovePuterState::SceneRevisionState before =
      GroovePuterState::sceneRevisionSnapshot();
  const float bpm = engine.sceneManager().getBpm();
  if (!std::isfinite(bpm) || bpm <= 0.0f) return false;
  const long long tempo = std::llround(60000000.0 / static_cast<double>(bpm));
  if (tempo < 1 || tempo > 0xFFFFFF) return false;

  snapshot = GroovePuterMidi::MidiSongExportSnapshot{};
  snapshot.song = scene.songs[songSlot];
  snapshot.songSlot = static_cast<uint8_t>(songSlot);
  snapshot.patternBars = scene.feel.patternBars;
  snapshot.swingPercent = scene.feel.swingPct;
  snapshot.swingMask = scene.feel.swingMask;
  snapshot.tempoMicrosecondsPerQuarter = static_cast<uint32_t>(tempo);
  const GrooveRecipe recipe = engine.genreManager().getGrooveRecipe();
  for (uint8_t voice = 0; voice < 2; ++voice) {
    PhraseRuntime::PatternProjectionSettings& settings =
        snapshot.synthSettings[voice];
    settings.synthIndex = voice;
    settings.gateLengthRatio = recipe.gateLengthRatio;
    settings.swingPercent = static_cast<uint8_t>(
        std::max(50, std::min(75, static_cast<int>(scene.feel.swingPct))));
    const VoiceId voiceId = voice == 0 ? VoiceId::SynthA : VoiceId::SynthB;
    settings.swingEnabled =
        (scene.feel.swingMask & (1u << static_cast<int>(voiceId))) != 0;
  }
  snapshot.routes = GroovePuterMidi::projectMidiOutputRoutes(
      GroovePuterMidi::midiDeviceProfileRuntime().settings());
  const GroovePuterState::SceneRevisionState after =
      GroovePuterState::sceneRevisionSnapshot();
  if (before.currentRevision != after.currentRevision || engine.isPlaying()) {
    return false;
  }
  snapshot.sceneRevision = before.currentRevision;
  return GroovePuterMidi::validSongExportSnapshot(snapshot);
}

}  // namespace

bool exportCurrentSongAsMidi(
    MiniAcid& engine,
    GroovePuterMidi::MidiSongExportFileResult& result) {
  result = GroovePuterMidi::MidiSongExportFileResult{};
  if (engine.isPlaying()) {
    result.error = GroovePuterMidi::SongExportFileError::PlaybackActive;
    return false;
  }
  GroovePuterMidi::MidiSongExportSnapshot snapshot{};
  if (!captureSnapshot(engine, snapshot)) {
    result.error = GroovePuterMidi::SongExportFileError::InvalidSnapshot;
    return false;
  }
  SdSongExportStorage storage;
  if (!storage.ensureDirectory(GroovePuterStorage::kMidiDirectory)) {
    result.error = GroovePuterMidi::SongExportFileError::StorageFailure;
    return false;
  }
  SongExportMaterials materials(engine.currentSceneName(), engine);
  return GroovePuterMidi::exportSongAsSmf(
      snapshot, materials, storage, engine.currentSceneName().c_str(), result);
}
