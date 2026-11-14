#pragma once

#include <cstdint>

#include "midi_output_route_projection.h"
#include "smf_export_writer.h"
#include "src/phrase/runtime_synth_events.h"
#include "src/state/material_slot.h"

namespace GroovePuterMidi {

constexpr uint16_t kSongExportPpqn = 1536;
constexpr uint8_t kSongExportTicksPerRuntimeTick = 16;

enum class SongExportLane : uint8_t { SynthA = 0, SynthB, Drums };

enum class SongExportWarning : uint8_t {
  ConditionalFlattened = 1u << 0,
  VoiceOmitted = 1u << 1,
  DrumFxOmitted = 1u << 2,
  DrumAutomationOmitted = 1u << 3,
};

struct SongExportSummary {
  struct LaneStats {
    uint32_t eventCount{0};
    uint64_t eventDigest{14695981039346656037ull};
  };

  uint8_t warnings{0};
  LaneStats lanes[3]{};

  void add(SongExportWarning warning) {
    warnings |= static_cast<uint8_t>(warning);
  }
  bool has(SongExportWarning warning) const {
    return (warnings & static_cast<uint8_t>(warning)) != 0;
  }
  void record(SongExportLane lane, uint32_t tick,
              uint8_t status, uint8_t data1, uint8_t data2) {
    LaneStats& stats = lanes[static_cast<uint8_t>(lane)];
    uint8_t bytes[7] = {
        static_cast<uint8_t>(tick),
        static_cast<uint8_t>(tick >> 8),
        static_cast<uint8_t>(tick >> 16),
        static_cast<uint8_t>(tick >> 24),
        status, data1, data2,
    };
    for (uint8_t byte : bytes) {
      stats.eventDigest ^= byte;
      stats.eventDigest *= 1099511628211ull;
    }
    ++stats.eventCount;
  }
};

enum class SongExportError : uint8_t {
  None = 0,
  InvalidSnapshot,
  InvalidReference,
  PageUnavailable,
  MelodyUnavailable,
  InvalidMelody,
  InvalidPattern,
  EventCapacityExceeded,
  WriteFailed,
};

struct MidiSongExportSnapshot {
  Song song{};
  uint8_t patternBars{1};
  uint8_t swingPercent{50};
  uint16_t swingMask{0xFFFF};
  uint32_t tempoMicrosecondsPerQuarter{500000};
  PhraseRuntime::PatternProjectionSettings synthSettings[2]{};
  MidiOutputRouteProjection routes{};
  uint32_t sceneRevision{0};
  uint8_t songSlot{0};
};

class ISongExportMaterialSource {
 public:
  virtual ~ISongExportMaterialSource() = default;
  // Returned Scene remains valid until the next loadPage() call.
  virtual bool loadPage(uint8_t page, const Scene*& out) = 0;
  virtual bool loadMelody(uint8_t voice, uint8_t globalSlot,
                          PhraseRuntime::RuntimeSynthEventBuffer& out) = 0;
  virtual bool snapshotStillCurrent(uint32_t) const { return true; }
};

bool validSongExportSnapshot(const MidiSongExportSnapshot& snapshot);
uint32_t songExportEndTick(const MidiSongExportSnapshot& snapshot);

bool writeSongExportTrack(const MidiSongExportSnapshot& snapshot,
                          ISongExportMaterialSource& materials,
                          SongExportLane lane,
                          ISmfExportByteSink& sink,
                          SmfExportTrackWriter& writer,
                          SongExportSummary& summary,
                          SongExportError* error = nullptr);

bool writeSongExportConductor(const MidiSongExportSnapshot& snapshot,
                              SongExportSummary& summary,
                              ISmfExportByteSink& sink,
                              SmfExportTrackWriter& writer,
                              SongExportError* error = nullptr);

}  // namespace GroovePuterMidi
