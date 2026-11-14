#include "midi_song_export.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "src/phrase/runtime_phrase_edit.h"
#include "src/state/material_slot_access.h"

namespace GroovePuterMidi {
namespace {

constexpr uint8_t kMidiAccentVelocityBoost = 30;
constexpr uint32_t kDrumGateMilliseconds = kDefaultDrumGateMs;
constexpr uint32_t kRuntimeTicksPerBar = PhraseRuntime::kTicksPerBar;

void setError(SongExportError* out, SongExportError error) {
  if (out != nullptr) *out = error;
}

uint8_t channelForLane(const MidiSongExportSnapshot& snapshot,
                       SongExportLane lane) {
  if (lane == SongExportLane::SynthA) {
    return snapshot.routes.patternSynthA.channel;
  }
  if (lane == SongExportLane::SynthB) {
    return snapshot.routes.patternSynthB.channel;
  }
  return 0;
}

const char* trackName(SongExportLane lane) {
  switch (lane) {
    case SongExportLane::SynthA: return "Synth A";
    case SongExportLane::SynthB: return "Synth B";
    case SongExportLane::Drums: return "Drums";
  }
  return "Unknown";
}

struct PendingNoteOff {
  uint32_t tick{0};
  uint8_t channel{0};
  uint8_t note{0};
};

bool emitNoteOffsThrough(uint32_t limit,
                         PendingNoteOff (&pending)[PhraseRuntime::kMaxSynthEvents],
                         uint16_t& count,
                         SmfExportTrackWriter& writer,
                         SongExportLane lane,
                         SongExportSummary& summary,
                         SongExportError* error) {
  for (;;) {
    int selected = -1;
    for (uint16_t i = 0; i < count; ++i) {
      if (pending[i].tick > limit) continue;
      if (selected < 0 || pending[i].tick < pending[selected].tick) {
        selected = i;
      }
    }
    if (selected < 0) return true;
    const PendingNoteOff off = pending[selected];
    pending[selected] = pending[count - 1];
    --count;
    if (!writer.writeChannelEvent(
            off.tick, static_cast<uint8_t>(0x80u | off.channel), off.note, 0)) {
      setError(error, SongExportError::WriteFailed);
      return false;
    }
    summary.record(lane, off.tick,
                   static_cast<uint8_t>(0x80u | off.channel), off.note, 0);
  }
}

bool addPendingOff(PendingNoteOff (&pending)[PhraseRuntime::kMaxSynthEvents],
                   uint16_t& count, uint32_t tick,
                   uint8_t channel, uint8_t note,
                   SongExportError* error) {
  if (count >= PhraseRuntime::kMaxSynthEvents) {
    setError(error, SongExportError::EventCapacityExceeded);
    return false;
  }
  pending[count++] = PendingNoteOff{tick, channel, note};
  return true;
}

void sortEventIndices(const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                      uint8_t (&indices)[PhraseRuntime::kMaxSynthEvents]) {
  for (uint16_t i = 0; i < phrase.count; ++i) {
    indices[i] = static_cast<uint8_t>(i);
    uint16_t at = i;
    while (at > 0) {
      const uint8_t previous = indices[at - 1];
      const uint8_t current = indices[at];
      const auto& a = phrase.events[previous];
      const auto& b = phrase.events[current];
      if (a.startTick < b.startTick ||
          (a.startTick == b.startTick && previous < current)) {
        break;
      }
      indices[at - 1] = current;
      indices[at] = previous;
      --at;
    }
  }
}

bool validEvent(const PhraseRuntime::RuntimeSynthEvent& event,
                uint16_t phraseLength) {
  return event.note <= 127 && event.velocity <= 127 &&
         event.probability <= 100 && event.startTick < phraseLength &&
         event.durationSubticks > 0;
}

bool emitPhraseInRow(const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                     uint16_t rowLengthTicks,
                     uint32_t rowStartTick,
                     uint32_t rowEndTick,
                     uint8_t channel,
                     SongExportLane lane,
                     SmfExportTrackWriter& writer,
                     SongExportSummary& summary,
                     SongExportError* error) {
  if (phrase.count > PhraseRuntime::kMaxSynthEvents ||
      phrase.lengthTicks == 0) {
    setError(error, SongExportError::InvalidPattern);
    return false;
  }
  uint8_t indices[PhraseRuntime::kMaxSynthEvents]{};
  sortEventIndices(phrase, indices);
  PendingNoteOff pending[PhraseRuntime::kMaxSynthEvents]{};
  uint16_t pendingCount = 0;

  for (uint32_t repeatOffset = 0; repeatOffset < rowLengthTicks;
       repeatOffset += phrase.lengthTicks) {
    for (uint16_t index = 0; index < phrase.count; ++index) {
      const auto& event = phrase.events[indices[index]];
      if (!validEvent(event, phrase.lengthTicks)) {
        setError(error, SongExportError::InvalidPattern);
        return false;
      }
      if (event.probability < 100 ||
          (event.flags & PhraseRuntime::kEventGhost) != 0) {
        summary.add(SongExportWarning::ConditionalFlattened);
      }
      const uint32_t onset = rowStartTick +
          (repeatOffset + event.startTick) * kSongExportTicksPerRuntimeTick;
      if (onset >= rowEndTick) continue;
      if (!emitNoteOffsThrough(onset, pending, pendingCount, writer,
                               lane, summary, error)) {
        return false;
      }
      const int velocityValue = static_cast<int>(event.velocity) +
          ((event.flags & PhraseRuntime::kEventAccent) != 0
               ? kMidiAccentVelocityBoost
               : 0);
      const uint8_t velocity = static_cast<uint8_t>(
          std::max(1, std::min(127, velocityValue)));
      if (!writer.writeChannelEvent(
              onset, static_cast<uint8_t>(0x90u | channel), event.note,
              velocity)) {
        setError(error, SongExportError::WriteFailed);
        return false;
      }
      summary.record(lane, onset, static_cast<uint8_t>(0x90u | channel),
                     event.note, velocity);
      const uint32_t rawOff = onset + event.durationSubticks;
      const uint32_t noteOff = std::min(rawOff, rowEndTick);
      if (noteOff <= onset ||
          !addPendingOff(pending, pendingCount, noteOff, channel, event.note,
                         error)) {
        if (noteOff <= onset) setError(error, SongExportError::InvalidPattern);
        return false;
      }
    }
  }
  return emitNoteOffsThrough(rowEndTick, pending, pendingCount, writer,
                             lane, summary, error);
}

bool loadPageForGlobal(ISongExportMaterialSource& materials,
                       int globalSlot,
                       const Scene*& page,
                       int& bank,
                       int& slot,
                       SongExportError* error) {
  if (globalSlot < 0 || globalSlot >= kMaxGlobalPatterns) {
    setError(error, SongExportError::InvalidReference);
    return false;
  }
  const int pageIndex = songPatternPage(globalSlot);
  bank = songPatternBank(globalSlot);
  slot = songPatternIndexInBank(globalSlot);
  if (pageIndex < 0 || pageIndex >= kMaxPages || bank < 0 ||
      bank >= kBankCount || slot < 0 ||
      slot >= Bank<SynthPattern>::kPatterns ||
      !materials.loadPage(static_cast<uint8_t>(pageIndex), page) ||
      page == nullptr) {
    setError(error, SongExportError::PageUnavailable);
    return false;
  }
  return true;
}

bool loadSynthPhrase(const MidiSongExportSnapshot& snapshot,
                     ISongExportMaterialSource& materials,
                     int row,
                     int voice,
                     PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                     SongExportError* error) {
  const int track = voice;
  const int globalSlot = snapshot.song.positions[row].patterns[track];
  if (globalSlot < 0 || globalSlot >= kMaxGlobalPatterns) {
    setError(error, SongExportError::InvalidReference);
    return false;
  }
  const Scene* page = nullptr;
  int bank = 0;
  int slot = 0;
  if (!loadPageForGlobal(materials, globalSlot, page, bank, slot, error)) {
    return false;
  }
  const int residentSlot = bank * Bank<SynthPattern>::kPatterns + slot;
  const GroovePuterMaterial::MaterialKind kind =
      page->materialSlots[voice][residentSlot].kind;
  if (!GroovePuterMaterial::validKindValue(static_cast<int>(kind))) {
    setError(error, SongExportError::InvalidPattern);
    return false;
  }
  const bool stepsOverride = snapshot.song.stepsOverrideAt(voice, row);
  if (kind == GroovePuterMaterial::MaterialKind::Melody && !stepsOverride) {
    if (!materials.loadMelody(static_cast<uint8_t>(voice),
                              static_cast<uint8_t>(globalSlot), phrase)) {
      setError(error, SongExportError::MelodyUnavailable);
      return false;
    }
    if (!RuntimePhraseEdit::validate(phrase)) {
      setError(error, SongExportError::InvalidMelody);
      return false;
    }
    return true;
  }

  const SynthPattern& pattern = voice == 0
      ? page->synthABanks[bank].patterns[slot]
      : page->synthBBanks[bank].patterns[slot];
  if (PhraseRuntime::projectPatternToRuntimeEvents(
          pattern, snapshot.synthSettings[voice], phrase) !=
      PhraseRuntime::PatternProjectionStatus::Ready) {
    setError(error, SongExportError::InvalidPattern);
    return false;
  }
  return true;
}

uint32_t drumStepTick(const MidiSongExportSnapshot& snapshot,
                      const DrumStep& step,
                      uint8_t voice,
                      uint8_t sourceStep) {
  const int swingPercent = std::max(50, std::min(75,
      static_cast<int>(snapshot.swingPercent)));
  const int swingDelay = static_cast<int>(std::lround(
      (static_cast<double>(swingPercent) - 50.0) * 24.0 / 50.0));
  const uint16_t swingBit = static_cast<uint16_t>(
      1u << (static_cast<uint8_t>(VoiceId::DrumKick) + voice));
  const int swing = ((sourceStep & 1u) != 0u &&
                     (snapshot.swingMask & swingBit) != 0)
      ? swingDelay
      : 0;
  int tick = static_cast<int>(sourceStep) * 24 + swing + step.timing;
  tick %= static_cast<int>(kRuntimeTicksPerBar);
  if (tick < 0) tick += kRuntimeTicksPerBar;
  return static_cast<uint32_t>(tick);
}

struct DrumHitEvent {
  uint32_t tick{0};
  uint8_t voice{0};
  uint8_t channel{0};
  uint8_t note{0};
  uint8_t velocity{1};
};

bool emitDrumRow(const MidiSongExportSnapshot& snapshot,
                 ISongExportMaterialSource& materials,
                 int row,
                 uint32_t rowStartTick,
                 uint32_t rowEndTick,
                 SmfExportTrackWriter& writer,
                 SongExportSummary& summary,
                 SongExportError* error) {
  const int globalSlot = snapshot.song.positions[row].patterns[2];
  if (globalSlot < 0 || globalSlot >= kMaxGlobalPatterns) {
    setError(error, SongExportError::InvalidReference);
    return false;
  }
  const Scene* page = nullptr;
  int bank = 0;
  int slot = 0;
  if (!loadPageForGlobal(materials, globalSlot, page, bank, slot, error)) {
    return false;
  }
  const DrumPatternSet& drums = page->drumBanks[bank].patterns[slot];
  for (const AutomationLane& automation : drums.lanes) {
    if (automation.nodeCount > 0) {
      summary.add(SongExportWarning::DrumAutomationOmitted);
      break;
    }
  }

  PendingNoteOff pending[PhraseRuntime::kMaxSynthEvents]{};
  uint16_t pendingCount = 0;
  const uint32_t rowLengthTicks = static_cast<uint32_t>(
      snapshot.patternBars) * kRuntimeTicksPerBar;
  const uint32_t gateTicks = static_cast<uint32_t>(
      (static_cast<uint64_t>(kDrumGateMilliseconds) * 1536000u +
       snapshot.tempoMicrosecondsPerQuarter / 2u) /
      snapshot.tempoMicrosecondsPerQuarter);

  for (uint32_t barOffset = 0; barOffset < rowLengthTicks;
       barOffset += kRuntimeTicksPerBar) {
    DrumHitEvent hits[DrumPatternSet::kVoices * DrumPattern::kSteps]{};
    uint16_t hitCount = 0;
    for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
      const DrumMidiRoute& route = snapshot.routes.patternDrums[voice];
      const DrumPattern& pattern = drums.voices[voice];
      for (uint8_t sourceStep = 0; sourceStep < DrumPattern::kSteps;
           ++sourceStep) {
        const DrumStep& step = pattern.steps[sourceStep];
        if (!step.hit) continue;
        if (step.probability < 100) {
          summary.add(SongExportWarning::ConditionalFlattened);
        }
        if (step.fx != DRUM_FX_NONE) {
          summary.add(SongExportWarning::DrumFxOmitted);
        }
        const uint32_t localTick = drumStepTick(
            snapshot, step, voice, sourceStep);
        const uint32_t onset = rowStartTick +
            (barOffset + localTick) * kSongExportTicksPerRuntimeTick;
        if (onset >= rowEndTick) continue;
        if (hitCount >= DrumPatternSet::kVoices * DrumPattern::kSteps) {
          setError(error, SongExportError::EventCapacityExceeded);
          return false;
        }
        hits[hitCount++] = DrumHitEvent{
            onset, voice, route.channel, route.note,
            static_cast<uint8_t>(std::max(1, std::min(127,
                static_cast<int>(step.velocity))))};
      }
    }
    std::sort(hits, hits + hitCount,
              [](const DrumHitEvent& lhs, const DrumHitEvent& rhs) {
      if (lhs.tick != rhs.tick) return lhs.tick < rhs.tick;
      if (lhs.voice != rhs.voice) return lhs.voice < rhs.voice;
      if (lhs.channel != rhs.channel) return lhs.channel < rhs.channel;
      return lhs.note < rhs.note;
    });
    for (uint16_t i = 0; i < hitCount; ++i) {
      const DrumHitEvent& hit = hits[i];
      if (!emitNoteOffsThrough(hit.tick, pending, pendingCount, writer,
                               SongExportLane::Drums, summary, error)) {
        return false;
      }
      if (!writer.writeChannelEvent(
              hit.tick, static_cast<uint8_t>(0x90u | hit.channel),
              hit.note, hit.velocity)) {
        setError(error, SongExportError::WriteFailed);
        return false;
      }
      summary.record(SongExportLane::Drums, hit.tick,
                     static_cast<uint8_t>(0x90u | hit.channel),
                     hit.note, hit.velocity);
      const uint32_t noteOff = std::min(hit.tick + gateTicks, rowEndTick);
      if (noteOff <= hit.tick ||
          !addPendingOff(pending, pendingCount, noteOff,
                         hit.channel, hit.note, error)) {
        if (noteOff <= hit.tick) setError(error, SongExportError::InvalidSnapshot);
        return false;
      }
    }
  }
  return emitNoteOffsThrough(rowEndTick, pending, pendingCount, writer,
                             SongExportLane::Drums, summary, error);
}

bool songHasVoiceContent(const Song& song) {
  const int length = std::max(0, std::min(song.length, Song::kMaxPositions));
  for (int row = 0; row < length; ++row) {
    if (song.positions[row].patterns[3] >= 0) return true;
  }
  return false;
}

}  // namespace

bool validSongExportSnapshot(const MidiSongExportSnapshot& snapshot) {
  if (snapshot.song.length < 1 ||
      snapshot.song.length > Song::kMaxPositions ||
      (snapshot.patternBars != 1 && snapshot.patternBars != 2 &&
       snapshot.patternBars != 4 && snapshot.patternBars != 8) ||
      snapshot.tempoMicrosecondsPerQuarter == 0 ||
      snapshot.tempoMicrosecondsPerQuarter > 0xFFFFFFu ||
      snapshot.songSlot > 1 ||
      snapshot.synthSettings[0].synthIndex != 0 ||
      snapshot.synthSettings[1].synthIndex != 1 ||
      snapshot.routes.patternSynthA.channel > 15 ||
      snapshot.routes.patternSynthB.channel > 15) {
    return false;
  }
  for (const DrumMidiRoute& route : snapshot.routes.patternDrums) {
    if (route.channel > 15 || route.note > 127) return false;
  }
  for (int row = 0; row < snapshot.song.length; ++row) {
    for (int lane = 0; lane < SongPosition::kTrackCount; ++lane) {
      const int reference = snapshot.song.positions[row].patterns[lane];
      if (reference < -1 || reference >= kMaxGlobalPatterns) return false;
    }
  }
  return true;
}

uint32_t songExportEndTick(const MidiSongExportSnapshot& snapshot) {
  if (!validSongExportSnapshot(snapshot)) return 0;
  return static_cast<uint32_t>(snapshot.song.length) * snapshot.patternBars *
      kRuntimeTicksPerBar * kSongExportTicksPerRuntimeTick;
}

bool writeSongExportTrack(const MidiSongExportSnapshot& snapshot,
                          ISongExportMaterialSource& materials,
                          SongExportLane lane,
                          ISmfExportByteSink& sink,
                          SmfExportTrackWriter& writer,
                          SongExportSummary& summary,
                          SongExportError* error) {
  setError(error, SongExportError::None);
  if (!validSongExportSnapshot(snapshot) ||
      static_cast<uint8_t>(lane) > static_cast<uint8_t>(SongExportLane::Drums)) {
    setError(error, SongExportError::InvalidSnapshot);
    return false;
  }
  if (!writer.begin(sink, trackName(lane))) {
    setError(error, SongExportError::WriteFailed);
    return false;
  }

  const uint32_t rowLengthTicks = static_cast<uint32_t>(
      snapshot.patternBars) * kRuntimeTicksPerBar;
  const uint32_t rowLengthSmf = rowLengthTicks *
      kSongExportTicksPerRuntimeTick;
  for (int outputRow = 0; outputRow < snapshot.song.length; ++outputRow) {
    const int sourceRow = snapshot.song.reverse
        ? snapshot.song.length - outputRow - 1
        : outputRow;
    const uint32_t rowStartTick =
        static_cast<uint32_t>(outputRow) * rowLengthSmf;
    const uint32_t rowEndTick = rowStartTick + rowLengthSmf;
    const int trackIndex = static_cast<int>(lane);
    const int globalSlot = snapshot.song.positions[sourceRow]
        .patterns[trackIndex];
    if (globalSlot < 0) continue;

    if (lane == SongExportLane::Drums) {
      if (!emitDrumRow(snapshot, materials, sourceRow, rowStartTick,
                       rowEndTick, writer, summary, error)) {
        return false;
      }
      continue;
    }

    const int voice = lane == SongExportLane::SynthA ? 0 : 1;
    PhraseRuntime::RuntimeSynthEventBuffer phrase{};
    if (!loadSynthPhrase(snapshot, materials, sourceRow, voice, phrase, error)) {
      return false;
    }
    const uint16_t rowLength = static_cast<uint16_t>(rowLengthTicks);
    if (!emitPhraseInRow(phrase, rowLength, rowStartTick, rowEndTick,
                         channelForLane(snapshot, lane), lane, writer, summary,
                         error)) {
      return false;
    }
  }
  if (!writer.finish(songExportEndTick(snapshot))) {
    setError(error, SongExportError::WriteFailed);
    return false;
  }
  return true;
}

bool writeSongExportConductor(const MidiSongExportSnapshot& snapshot,
                              SongExportSummary& summary,
                              ISmfExportByteSink& sink,
                              SmfExportTrackWriter& writer,
                              SongExportError* error) {
  setError(error, SongExportError::None);
  if (!validSongExportSnapshot(snapshot)) {
    setError(error, SongExportError::InvalidSnapshot);
    return false;
  }
  if (!writer.begin(sink, "Conductor") ||
      !writer.writeTempo(0, snapshot.tempoMicrosecondsPerQuarter) ||
      !writer.writeTimeSignature(0, 4, 2, 24, 8)) {
    setError(error, SongExportError::WriteFailed);
    return false;
  }

  if (songHasVoiceContent(snapshot.song)) {
    summary.add(SongExportWarning::VoiceOmitted);
  }
  struct WarningText {
    SongExportWarning warning;
    const char* text;
  };
  static constexpr WarningText warnings[] = {
      {SongExportWarning::ConditionalFlattened,
       "Conditional/probability steps are exported as always-on notes."},
      {SongExportWarning::VoiceOmitted,
       "Voice audio/sample content is not rendered to MIDI."},
      {SongExportWarning::DrumFxOmitted,
       "Drum step FX are omitted; only base hits are exported."},
      {SongExportWarning::DrumAutomationOmitted,
       "Internal drum automation is omitted from MIDI."},
  };
  for (const WarningText& warning : warnings) {
    if (!summary.has(warning.warning)) continue;
    if (!writer.writeText(0, 0x06, warning.text,
                          std::strlen(warning.text))) {
      setError(error, SongExportError::WriteFailed);
      return false;
    }
  }
  if (!writer.finish(songExportEndTick(snapshot))) {
    setError(error, SongExportError::WriteFailed);
    return false;
  }
  return true;
}

}  // namespace GroovePuterMidi
