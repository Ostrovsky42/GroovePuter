#include "midi_song_export_file.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "smf_stream.h"
#include "src/ui/midi_file_name_policy.h"

namespace GroovePuterMidi {
namespace {

constexpr uint8_t kTrackFileOrder[] = {3, 0, 1, 2};  // conductor, A, B, drums

class StorageSink final : public ISmfExportByteSink {
 public:
  explicit StorageSink(ISongExportStorage& storage) : storage_(storage) {}
  bool write(const uint8_t* data, std::size_t length) override {
    return storage_.write(data, length) == length;
  }

 private:
  ISongExportStorage& storage_;
};

class StorageSource final : public ISmfByteSource {
 public:
  explicit StorageSource(ISongExportStorage& storage) : storage_(storage) {}
  uint32_t size() const override { return storage_.readSize(); }
  bool readAt(uint32_t offset, uint8_t* dst, std::size_t length) override {
    return storage_.readAt(offset, dst, length);
  }

 private:
  ISongExportStorage& storage_;
};

void setFileError(MidiSongExportFileResult& result,
                  SongExportFileError error) {
  result.error = error;
}

bool buildFinalPath(ISongExportStorage& storage,
                    const char* projectName,
                    uint8_t songSlot,
                    char* path,
                    std::size_t pathSize) {
  char stem[64]{};
  const char* base = projectName != nullptr && projectName[0] != '\0'
      ? projectName
      : "GroovePuter";
  std::snprintf(stem, sizeof(stem), "%s-Song%u", base,
                static_cast<unsigned>(songSlot + 1));
  char baseFilename[64]{};
  if (!GroovePuterUi::buildMidiFilenameFromStem(
          stem, baseFilename, sizeof(baseFilename))) {
    std::snprintf(stem, sizeof(stem), "GroovePuter-Song%u",
                  static_cast<unsigned>(songSlot + 1));
    if (!GroovePuterUi::buildMidiFilenameFromStem(
            stem, baseFilename, sizeof(baseFilename))) return false;
  }
  const std::size_t stemLength = std::strlen(baseFilename) - 4;
  for (uint16_t suffix = 1; suffix <= 9999; ++suffix) {
    char filename[64]{};
    if (suffix == 1) {
      std::memcpy(filename, baseFilename, std::strlen(baseFilename) + 1);
    } else {
      char candidateStem[64]{};
      const int written = std::snprintf(candidateStem, sizeof(candidateStem),
          "%.*s-%u", static_cast<int>(stemLength), baseFilename,
          static_cast<unsigned>(suffix));
      if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(candidateStem) ||
          !GroovePuterUi::buildMidiFilenameFromStem(
              candidateStem, filename, sizeof(filename))) {
        continue;
      }
    }
    const int written = std::snprintf(path, pathSize, "/midi/%s", filename);
    if (written <= 0 || static_cast<std::size_t>(written) >= pathSize) return false;
    if (!storage.exists(path)) return true;
  }
  return false;
}

bool buildTemporaryPaths(ISongExportStorage& storage,
                         char (&paths)[5][64]) {
  static constexpr char suffixes[5] = {'a', 'b', 'd', 'c', 'f'};
  for (uint16_t id = 0; id < 10000; ++id) {
    bool available = true;
    for (uint8_t i = 0; i < 5; ++i) {
      const int written = std::snprintf(
          paths[i], sizeof(paths[i]), "/midi/.gpexp-%04u-%c.tmp",
          static_cast<unsigned>(id), suffixes[i]);
      if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(paths[i])) {
        return false;
      }
      if (storage.exists(paths[i])) available = false;
    }
    if (available) return true;
  }
  return false;
}

bool cleanupTemporaryFiles(ISongExportStorage& storage,
                           const char (&paths)[5][64]) {
  bool ok = true;
  for (const auto& path : paths) {
    if (!storage.remove(path)) ok = false;
  }
  return ok;
}

bool writeTrackFile(ISongExportStorage& storage,
                    const char* path,
                    const MidiSongExportSnapshot& snapshot,
                    ISongExportMaterialSource* materials,
                    SongExportLane lane,
                    SongExportSummary& summary,
                    SongExportError& domainError) {
  if (!storage.beginWrite(path)) return false;
  StorageSink sink(storage);
  SmfExportTrackWriter writer;
  bool generated = false;
  if (materials != nullptr) {
    generated = writeSongExportTrack(snapshot, *materials, lane, sink,
                                     writer, summary, &domainError);
  } else {
    generated = writeSongExportConductor(snapshot, summary, sink, writer,
                                         &domainError);
  }
  const bool closed = storage.closeWrite();
  return generated && closed && storage.size(path) == writer.bytesWritten();
}

bool appendTrackFile(ISongExportStorage& storage,
                     StorageSink& sink,
                     const char* path,
                     uint64_t& expectedFileSize) {
  if (!storage.openRead(path)) return false;
  const uint32_t trackLength = storage.readSize();
  bool ok = trackLength > 0 &&
      expectedFileSize + 8u + trackLength <= 0xFFFFFFFFu &&
      writeSmfTrackHeader(sink, trackLength);
  expectedFileSize += 8u + trackLength;
  uint8_t buffer[256]{};
  uint32_t offset = 0;
  while (ok && offset < trackLength) {
    const std::size_t chunk = std::min<std::size_t>(
        sizeof(buffer), trackLength - offset);
    ok = storage.readAt(offset, buffer, chunk) && sink.write(buffer, chunk);
    offset += static_cast<uint32_t>(chunk);
  }
  const bool closed = storage.closeRead();
  return ok && closed;
}

bool assembleFile(ISongExportStorage& storage,
                  const MidiSongExportSnapshot& snapshot,
                  const char (&paths)[5][64]) {
  if (!storage.beginWrite(paths[4])) return false;
  StorageSink sink(storage);
  bool ok = writeSmfHeader(sink, 4, kSongExportPpqn);
  uint64_t expectedSize = 14;
  for (uint8_t orderedIndex : kTrackFileOrder) {
    if (!ok) break;
    ok = appendTrackFile(storage, sink, paths[orderedIndex], expectedSize);
  }
  const bool closed = storage.closeWrite();
  return ok && closed && expectedSize <= 0xFFFFFFFFu &&
         storage.size(paths[4]) == expectedSize &&
         songExportEndTick(snapshot) > 0;
}

bool validateTrack(ISmfByteSource& source,
                   const SmfTrackSpan& span,
                   uint16_t trackIndex,
                   const MidiSongExportSnapshot& snapshot,
                   const SongExportSummary& summary,
                   uint8_t* cache) {
  if (span.length < 4) return false;
  uint8_t endMarker[3]{};
  if (!source.readAt(span.offset + span.length - sizeof(endMarker),
                     endMarker, sizeof(endMarker)) ||
      endMarker[0] != 0xFF || endMarker[1] != 0x2F || endMarker[2] != 0) {
    return false;
  }

  SmfTrackStream stream;
  stream.setCache(cache, 512);
  if (!stream.open(source, span, trackIndex)) return false;
  uint8_t active[16][128]{};
  SongExportSummary observed{};
  uint8_t tempoCount = 0;
  uint8_t signatureCount = 0;
  SmfStreamEvent streamed{};
  while (stream.next(streamed)) {
    const SmfEvent& event = streamed.event;
    if (event.kind == SmfEventKind::Tempo) {
      if (trackIndex != 0 || event.tick != 0 ||
          event.value != snapshot.tempoMicrosecondsPerQuarter ||
          ++tempoCount != 1) return false;
      continue;
    }
    if (event.kind == SmfEventKind::TimeSignature) {
      if (trackIndex != 0 || event.tick != 0 || event.data1 != 4 ||
          event.data2 != 2 || ++signatureCount != 1) return false;
      continue;
    }
    if (event.kind == SmfEventKind::ProgramChange) return false;
    if (event.kind != SmfEventKind::NoteOn &&
        event.kind != SmfEventKind::NoteOff) continue;
    if (trackIndex == 0 || event.channel > 15 || event.data1 > 127 ||
        event.data2 > 127) return false;

    const SongExportLane lane = static_cast<SongExportLane>(trackIndex - 1);
    const uint8_t status = static_cast<uint8_t>(
        (event.kind == SmfEventKind::NoteOn ? 0x90u : 0x80u) |
        event.channel);
    if (event.kind == SmfEventKind::NoteOn) {
      uint8_t& count = active[event.channel][event.data1];
      if (event.data2 == 0 || count == 0xFFu) return false;
      ++count;
    } else {
      uint8_t& count = active[event.channel][event.data1];
      if (count == 0) return false;
      --count;
    }
    observed.record(lane, event.tick, status, event.data1, event.data2);
  }
  if (!stream.ended()) return false;
  if (trackIndex == 0) {
    return tempoCount == 1 && signatureCount == 1;
  }
  for (const auto& channel : active) {
    for (uint8_t count : channel) {
      if (count != 0) return false;
    }
  }
  const auto& actual = observed.lanes[trackIndex - 1];
  const auto& expected = summary.lanes[trackIndex - 1];
  return actual.eventCount == expected.eventCount &&
         actual.eventDigest == expected.eventDigest;
}

bool validateWholeFile(ISongExportStorage& storage,
                       const char* path,
                       const MidiSongExportSnapshot& snapshot,
                       const SongExportSummary& summary) {
  if (!storage.openRead(path)) return false;
  StorageSource source(storage);
  const SmfIndexResult indexed = SmfFileIndexer::build(source);
  bool valid = indexed.ok() && indexed.index.format == 1 &&
      indexed.index.division == kSongExportPpqn &&
      indexed.index.trackCount == 4 &&
      indexed.index.declaredTrackCount == 4 &&
      !indexed.index.tracksTruncated();
  uint8_t cache[512]{};
  for (uint16_t track = 0; valid && track < 4; ++track) {
    valid = validateTrack(source, indexed.index.tracks[track], track,
                          snapshot, summary, cache);
  }
  const bool closed = storage.closeRead();
  return valid && closed;
}

}  // namespace

bool exportSongAsSmf(const MidiSongExportSnapshot& snapshot,
                     ISongExportMaterialSource& materials,
                     ISongExportStorage& storage,
                     const char* projectName,
                     MidiSongExportFileResult& result) {
  result = MidiSongExportFileResult{};
  if (!validSongExportSnapshot(snapshot) || snapshot.songSlot > 1) {
    setFileError(result, SongExportFileError::InvalidSnapshot);
    return false;
  }
  if (!storage.ensureDirectory("/midi")) {
    setFileError(result, SongExportFileError::StorageFailure);
    return false;
  }
  char finalPath[128]{};
  char temporaryPaths[5][64]{};
  if (!buildFinalPath(storage, projectName, snapshot.songSlot,
                      finalPath, sizeof(finalPath)) ||
      !buildTemporaryPaths(storage, temporaryPaths)) {
    setFileError(result, SongExportFileError::NameUnavailable);
    return false;
  }

  SongExportSummary summary{};
  SongExportError domainError = SongExportError::None;
  bool ok = writeTrackFile(storage, temporaryPaths[0], snapshot, &materials,
                           SongExportLane::SynthA, summary, domainError) &&
      writeTrackFile(storage, temporaryPaths[1], snapshot, &materials,
                     SongExportLane::SynthB, summary, domainError) &&
      writeTrackFile(storage, temporaryPaths[2], snapshot, &materials,
                     SongExportLane::Drums, summary, domainError) &&
      writeTrackFile(storage, temporaryPaths[3], snapshot, nullptr,
                     SongExportLane::SynthA, summary, domainError) &&
      assembleFile(storage, snapshot, temporaryPaths);

  if (!ok) {
    const SongExportFileError error =
        domainError == SongExportError::PageUnavailable ||
        domainError == SongExportError::MelodyUnavailable ||
        domainError == SongExportError::InvalidMelody ||
        domainError == SongExportError::InvalidPattern ||
        domainError == SongExportError::InvalidReference
            ? SongExportFileError::MaterialFailure
            : SongExportFileError::EncodeFailure;
    setFileError(result, error);
    cleanupTemporaryFiles(storage, temporaryPaths);
    return false;
  }

  const bool valid = validateWholeFile(
      storage, temporaryPaths[4], snapshot, summary);
  if (!valid) {
    setFileError(result, SongExportFileError::ValidationFailure);
    cleanupTemporaryFiles(storage, temporaryPaths);
    return false;
  }
  if (!materials.snapshotStillCurrent(snapshot.sceneRevision)) {
    setFileError(result, SongExportFileError::SourceChanged);
    cleanupTemporaryFiles(storage, temporaryPaths);
    return false;
  }
  for (uint8_t i = 0; i < 4; ++i) {
    if (!storage.remove(temporaryPaths[i])) {
      setFileError(result, SongExportFileError::StorageFailure);
      cleanupTemporaryFiles(storage, temporaryPaths);
      return false;
    }
  }
  if (!storage.rename(temporaryPaths[4], finalPath)) {
    setFileError(result, SongExportFileError::PublishFailure);
    cleanupTemporaryFiles(storage, temporaryPaths);
    return false;
  }
  std::snprintf(result.path, sizeof(result.path), "%s", finalPath);
  result.warnings = summary.warnings;
  result.error = SongExportFileError::None;
  return true;
}

}  // namespace GroovePuterMidi
