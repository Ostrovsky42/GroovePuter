#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "src/midi/midi_song_export_file.h"

namespace {

enum class Failure {
  None,
  EnsureDirectory,
  BeginWrite,
  ShortWrite,
  CloseWrite,
  OpenRead,
  CloseRead,
  TrackRead,
  ValidationRead,
  Rename,
  MaterialPage,
  MissingMelody,
};

class FakeStorage final : public GroovePuterMidi::ISongExportStorage {
 public:
  explicit FakeStorage(Failure failure = Failure::None) : failure_(failure) {}

  bool ensureDirectory(const char*) override {
    return failure_ != Failure::EnsureDirectory;
  }
  bool exists(const char* path) const override { return files.count(path) != 0; }
  bool remove(const char* path) override { files.erase(path); return true; }
  bool rename(const char* from, const char* to) override {
    if (failure_ == Failure::Rename) return false;
    auto it = files.find(from);
    if (it == files.end() || files.count(to) != 0) return false;
    files[to] = std::move(it->second);
    files.erase(it);
    return true;
  }
  bool beginWrite(const char* path) override {
    if (failure_ == Failure::BeginWrite && !failureUsed_) {
      failureUsed_ = true;
      return false;
    }
    if (!writePath_.empty()) return false;
    writePath_ = path;
    files[writePath_].clear();
    return true;
  }
  std::size_t write(const uint8_t* data, std::size_t length) override {
    if (writePath_.empty() || (length != 0 && data == nullptr)) return 0;
    std::size_t count = length;
    if (failure_ == Failure::ShortWrite && !failureUsed_) {
      failureUsed_ = true;
      count = length == 0 ? 0 : length - 1;
    }
    files[writePath_].insert(files[writePath_].end(), data, data + count);
    return count;
  }
  bool closeWrite() override {
    if (writePath_.empty()) return false;
    writePath_.clear();
    if (failure_ == Failure::CloseWrite && !failureUsed_) {
      failureUsed_ = true;
      return false;
    }
    return true;
  }
  bool openRead(const char* path) override {
    if (failure_ == Failure::OpenRead && !failureUsed_) {
      failureUsed_ = true;
      return false;
    }
    if (!readPath_.empty() || files.count(path) == 0) return false;
    readPath_ = path;
    return true;
  }
  uint32_t readSize() const override {
    auto it = files.find(readPath_);
    return it == files.end() ? 0 : static_cast<uint32_t>(it->second.size());
  }
  bool readAt(uint32_t offset, uint8_t* data, std::size_t length) override {
    if (readPath_.empty() || (length != 0 && data == nullptr)) return false;
    if ((failure_ == Failure::TrackRead && endsWith(readPath_, "-a.tmp")) ||
        (failure_ == Failure::ValidationRead && endsWith(readPath_, "-f.tmp"))) {
      if (!failureUsed_) {
        failureUsed_ = true;
        return false;
      }
    }
    const auto& bytes = files.at(readPath_);
    if (offset > bytes.size() || length > bytes.size() - offset) return false;
    std::memcpy(data, bytes.data() + offset, length);
    return true;
  }
  bool closeRead() override {
    if (readPath_.empty()) return false;
    readPath_.clear();
    if (failure_ == Failure::CloseRead && !failureUsed_) {
      failureUsed_ = true;
      return false;
    }
    return true;
  }
  uint32_t size(const char* path) const override {
    auto it = files.find(path);
    return it == files.end() ? 0 : static_cast<uint32_t>(it->second.size());
  }

  std::size_t visibleMidiCount() const {
    std::size_t count = 0;
    for (const auto& entry : files) {
      if (entry.first.size() >= 4 &&
          entry.first.compare(entry.first.size() - 4, 4, ".mid") == 0) {
        ++count;
      }
    }
    return count;
  }

  bool hasTemporaryFiles() const {
    for (const auto& entry : files) {
      if (entry.first.find(".gpexp-") != std::string::npos) return true;
    }
    return false;
  }

  std::map<std::string, std::vector<uint8_t>> files;

 private:
  static bool endsWith(const std::string& value, const char* suffix) {
    const std::size_t length = std::strlen(suffix);
    return value.size() >= length &&
           value.compare(value.size() - length, length, suffix) == 0;
  }

  Failure failure_;
  bool failureUsed_{false};
  std::string writePath_;
  std::string readPath_;
};

class FakeMaterials final : public GroovePuterMidi::ISongExportMaterialSource {
 public:
  bool loadPage(uint8_t page, const Scene*& out) override {
    if (!pageAvailable || page != 0) return false;
    out = &scene;
    return true;
  }
  bool loadMelody(uint8_t, uint8_t,
                  PhraseRuntime::RuntimeSynthEventBuffer&) override {
    return melodyAvailable;
  }
  Scene scene{};
  bool pageAvailable{true};
  bool melodyAvailable{false};
};

GroovePuterMidi::MidiSongExportSnapshot makeSnapshot() {
  GroovePuterMidi::MidiSongExportSnapshot snapshot{};
  snapshot.song.length = 1;
  snapshot.song.positions[0].patterns[0] = 0;
  snapshot.patternBars = 2;
  snapshot.tempoMicrosecondsPerQuarter = 500000;
  snapshot.synthSettings[0].synthIndex = 0;
  snapshot.synthSettings[0].gateLengthRatio = 0.5f;
  snapshot.synthSettings[1].synthIndex = 1;
  snapshot.routes.patternSynthA = {false, 4};
  return snapshot;
}

bool exportOnce(Failure failure, const char* fixturePath) {
  FakeStorage storage(failure);
  FakeMaterials materials;
  if (failure == Failure::MaterialPage) materials.pageAvailable = false;
  if (failure == Failure::MissingMelody) {
    materials.scene.materialSlots[0][0].kind =
        GroovePuterMaterial::MaterialKind::Melody;
  }
  materials.scene.synthABanks[0].patterns[0].steps[0].note = 60;
  GroovePuterMidi::MidiSongExportFileResult result{};
  const bool exported = GroovePuterMidi::exportSongAsSmf(
      makeSnapshot(), materials, storage, "ExportTest", result);
  if (failure == Failure::None) {
    assert(exported);
    assert(result.error == GroovePuterMidi::SongExportFileError::None);
    assert(std::strstr(result.path, "/midi/ExportTest-Song1.mid") != nullptr);
    assert(storage.visibleMidiCount() == 1);
    assert(!storage.hasTemporaryFiles());
    const auto bytes = storage.files.at(result.path);
    std::ofstream output(fixturePath, std::ios::binary);
    assert(output.good());
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    output.close();
    assert(output.good());

    GroovePuterMidi::MidiSongExportFileResult repeatResult{};
    assert(GroovePuterMidi::exportSongAsSmf(
        makeSnapshot(), materials, storage, "ExportTest", repeatResult));
    assert(repeatResult.error == GroovePuterMidi::SongExportFileError::None);
    assert(std::strcmp(result.path, repeatResult.path) != 0);
    assert(storage.visibleMidiCount() == 2);
    assert(!storage.hasTemporaryFiles());
    assert(storage.files.at(repeatResult.path) == bytes);
    return true;
  }
  assert(!exported);
  assert(result.error != GroovePuterMidi::SongExportFileError::None);
  assert(storage.visibleMidiCount() == 0);
  assert(!storage.hasTemporaryFiles());
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc == 2);
  {
    FakeStorage storage;
    FakeMaterials materials;
    auto empty = makeSnapshot();
    empty.song.length = 0;
    GroovePuterMidi::MidiSongExportFileResult result{};
    assert(!GroovePuterMidi::exportSongAsSmf(
        empty, materials, storage, "Empty", result));
    assert(result.error == GroovePuterMidi::SongExportFileError::InvalidSnapshot);
    assert(storage.visibleMidiCount() == 0);
    assert(!storage.hasTemporaryFiles());
  }
  assert(!exportOnce(Failure::EnsureDirectory, argv[1]));
  assert(!exportOnce(Failure::BeginWrite, argv[1]));
  assert(exportOnce(Failure::None, argv[1]));
  assert(!exportOnce(Failure::ShortWrite, argv[1]));
  assert(!exportOnce(Failure::CloseWrite, argv[1]));
  assert(!exportOnce(Failure::OpenRead, argv[1]));
  assert(!exportOnce(Failure::CloseRead, argv[1]));
  assert(!exportOnce(Failure::TrackRead, argv[1]));
  assert(!exportOnce(Failure::ValidationRead, argv[1]));
  assert(!exportOnce(Failure::Rename, argv[1]));
  assert(!exportOnce(Failure::MaterialPage, argv[1]));
  assert(!exportOnce(Failure::MissingMelody, argv[1]));
  return 0;
}
