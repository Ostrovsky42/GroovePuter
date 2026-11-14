#pragma once

#include <cstddef>
#include <cstdint>

#include "midi_song_export.h"

namespace GroovePuterMidi {

class ISongExportStorage {
 public:
  virtual ~ISongExportStorage() = default;
  virtual bool ensureDirectory(const char* path) = 0;
  virtual bool exists(const char* path) const = 0;
  virtual bool remove(const char* path) = 0;
  virtual bool rename(const char* from, const char* to) = 0;

  // One sequential writer and one independent reader may be open at once.
  virtual bool beginWrite(const char* path) = 0;
  virtual std::size_t write(const uint8_t* data, std::size_t length) = 0;
  virtual bool closeWrite() = 0;
  virtual bool openRead(const char* path) = 0;
  virtual uint32_t readSize() const = 0;
  virtual bool readAt(uint32_t offset, uint8_t* data,
                      std::size_t length) = 0;
  virtual bool closeRead() = 0;
  virtual uint32_t size(const char* path) const = 0;
};

enum class SongExportFileError : uint8_t {
  None = 0,
  InvalidSnapshot,
  NameUnavailable,
  StorageFailure,
  MaterialFailure,
  EncodeFailure,
  ValidationFailure,
  PublishFailure,
  PlaybackActive,
  SourceChanged,
};

struct MidiSongExportFileResult {
  SongExportFileError error{SongExportFileError::None};
  uint8_t warnings{0};
  char path[128]{};
};

bool exportSongAsSmf(const MidiSongExportSnapshot& snapshot,
                     ISongExportMaterialSource& materials,
                     ISongExportStorage& storage,
                     const char* projectName,
                     MidiSongExportFileResult& result);

}  // namespace GroovePuterMidi
