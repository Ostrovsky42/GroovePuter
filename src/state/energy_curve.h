#pragma once
#ifndef GROOVEPUTER_SRC_STATE_ENERGY_CURVE_H
#define GROOVEPUTER_SRC_STATE_ENERGY_CURVE_H

#include <cstdint>

#include "song_edit.h"

// SONG -> FORM: an energy curve builds ordinary Song rows from material that
// already exists (docs/superpowers/plans/2026-10-08-0918-energy-curve.md).
//
// The curve is parameters for building rows, not a second player: once applied
// the sound is whatever the rows say, and later GRID edits are never redone by
// the curve. An empty Song cell silences that track for the bar, so voices
// entering and leaving need no new data model and no new slots.
//
// Source = a block of 1..8 existing rows (a TAKE); each section repeats it
// from its first row, so every section starts on the phrase start.
//
// Energy (first slice, all from the source rows only):
//   0  silence
//   1  drums only
//   2  drums + bass (Synth A)
//   3  drums + bass + lead (Synth B) with space: the lead rests on the third
//      bar of every four (A A' _ B, the call/response shape)
//   4  everything (lead every bar, plus the Voice lane)
//   5  A+B: bass and lead, no drums (H3)
//   6  DR+B: drums and lead, no bass (H3)
// The FORM view orders the seven levels by rank (energyRank): SILENCE, DR, DR+A,
// DR+B, A+B, +B SPACE, FULL. Codes 0..4 keep their meaning.
namespace EnergyCurve {

constexpr uint8_t kMaxSections = 8;
constexpr uint8_t kMaxEnergy = 6;
constexpr uint8_t kEnergyLevels = 7;
constexpr uint8_t kMaxSourceRows = 8;

// Rank of an energy code in the Up/Down order (0 = silence, 6 = full).
inline uint8_t energyRank(uint8_t energy) {
  static constexpr uint8_t kOrder[kEnergyLevels] = {0, 1, 2, 6, 5, 3, 4};
  for (uint8_t i = 0; i < kEnergyLevels; ++i) {
    if (kOrder[i] == energy) return i;
  }
  return 0;
}

inline uint8_t energyAtRank(uint8_t rank) {
  static constexpr uint8_t kOrder[kEnergyLevels] = {0, 1, 2, 6, 5, 3, 4};
  return rank < kEnergyLevels ? kOrder[rank] : 0;
}

struct Section {
  uint8_t bars = 4;    // 4 or 8
  uint8_t energy = 2;  // 0..kMaxEnergy
};

struct Curve {
  Section sections[kMaxSections]{};
  uint8_t count = 0;
};

enum class Preset : uint8_t { Build = 0, Drop, Wave, Count };

inline const char* presetName(Preset preset) {
  switch (preset) {
    case Preset::Build: return "BUILD";
    case Preset::Drop: return "DROP";
    case Preset::Wave: return "WAVE";
    default: return "?";
  }
}

inline Curve presetCurve(Preset preset) {
  static constexpr uint8_t kBuild[] = {1, 2, 3, 4, 4};
  static constexpr uint8_t kDrop[] = {2, 3, 4, 1, 4};
  static constexpr uint8_t kWave[] = {2, 4, 2, 4};
  const uint8_t* levels = kBuild;
  uint8_t count = sizeof(kBuild);
  if (preset == Preset::Drop) {
    levels = kDrop;
    count = sizeof(kDrop);
  } else if (preset == Preset::Wave) {
    levels = kWave;
    count = sizeof(kWave);
  }
  Curve curve{};
  for (uint8_t i = 0; i < count; ++i) curve.sections[i] = Section{4, levels[i]};
  curve.count = count;
  return curve;
}

inline uint16_t totalBars(const Curve& curve) {
  uint16_t bars = 0;
  for (uint8_t i = 0; i < curve.count && i < kMaxSections; ++i) bars += curve.sections[i].bars;
  return bars;
}

struct Voices {
  bool drums = false;
  bool bass = false;
  bool lead = false;
  bool voice = false;
};

inline Voices voicesFor(uint8_t energy, uint16_t barInSection) {
  Voices v{};
  switch (energy) {
    case 1: v.drums = true; break;
    case 2: v.drums = v.bass = true; break;
    case 3: v.drums = v.bass = true; v.lead = (barInSection % 4u) != 2u; break;  // A A' _ B
    case 4: v.drums = v.bass = v.lead = v.voice = true; break;
    case 5: v.bass = v.lead = true; break;  // A+B: no drums
    case 6: v.drums = v.lead = true; break;  // DR+B: no bass
    default: break;
  }
  return v;
}

enum class Status : uint8_t {
  Ok = 0,
  NoSections,
  NoSource,  // the source rows hold no material at all
  NoRoom,    // the Song cannot hold the extra rows
  BadRange,
};

inline const char* statusText(Status status) {
  switch (status) {
    case Status::Ok: return "OK";
    case Status::NoSections: return "ADD A SECTION";
    case Status::NoSource: return "PICK ROWS WITH MATERIAL";
    case Status::NoRoom: return "SONG FULL";
    case Status::BadRange:
    default: return "FORM FAILED";
  }
}

// Inserts totalBars(curve) rows at insertAt. A rise uses the source lead for
// a one-bar pickup when it is available; a drop removes the bass for the last
// bar before the lower-energy section. All checks happen before the first
// change, so a refusal leaves the Song exactly as it was.
inline Status apply(Song& song, const Curve& curve, int sourceFirst, int sourceCount,
                    int insertAt) {
  using GroovePuterUndo::SongEdit::insertRow;
  using GroovePuterUndo::SongEdit::setPattern;
  if (curve.count == 0) return Status::NoSections;
  const int length = song.length < 1 ? 1 : song.length;
  if (sourceCount < 1 || sourceCount > kMaxSourceRows || sourceFirst < 0 ||
      sourceFirst + sourceCount > length || insertAt < 0 || insertAt > length) {
    return Status::BadRange;
  }
  SongPosition source[kMaxSourceRows]{};
  bool any = false;
  for (int i = 0; i < sourceCount; ++i) {
    source[i] = song.positions[sourceFirst + i];
    for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      if (source[i].patterns[t] >= 0) any = true;
    }
  }
  if (!any) return Status::NoSource;
  const int bars = totalBars(curve);
  if (length + bars > Song::kMaxPositions) return Status::NoRoom;

  int row = insertAt;
  for (uint8_t s = 0; s < curve.count; ++s) {
    const Section& section = curve.sections[s];
    for (uint16_t bar = 0; bar < section.bars; ++bar, ++row) {
      insertRow(song, row);
      const SongPosition& from = source[bar % sourceCount];
      Voices v = voicesFor(section.energy, bar);
      const bool lastBar = bar + 1 == section.bars;
      if (lastBar && s + 1 < curve.count) {
        const uint8_t nextEnergy = curve.sections[s + 1].energy;
        const uint8_t rank = energyRank(section.energy);
        const uint8_t nextRank = energyRank(nextEnergy);
        const bool nextHasLead = nextEnergy == 3 || nextEnergy == 4 || nextEnergy == 5 || nextEnergy == 6;
        if (nextRank > rank && nextHasLead && !v.lead &&
            from.patterns[GroovePuterUndo::SongEdit::trackIndex(
                SongTrack::SynthB)] >= 0) {
          v.lead = true;
        }
        // H3: the bass comes off on the last bar before a drop, only in a section that has drums.
        if (nextRank < rank && v.drums && v.bass) {
          v.bass = false;
        }
      }
      const auto put = [&](SongTrack track, bool on) {
        const int ti = GroovePuterUndo::SongEdit::trackIndex(track);
        setPattern(song, row, track, on ? from.patterns[ti] : -1);
      };
      put(SongTrack::Drums, v.drums);
      put(SongTrack::SynthA, v.bass);
      put(SongTrack::SynthB, v.lead);
      put(SongTrack::Voice, v.voice);
    }
  }
  return Status::Ok;
}

}  // namespace EnergyCurve

#endif  // GROOVEPUTER_SRC_STATE_ENERGY_CURVE_H
