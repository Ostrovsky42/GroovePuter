// SONG -> FORM: an energy curve inserts ordinary Song rows from existing material.
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/state/energy_curve.h"

using namespace EnergyCurve;
using GroovePuterUndo::SongEdit::patternAt;

namespace {
// A 4-row TAKE: A slots 0..3, B slots 4..7, drums 8..11, Voice 12..15.
Song takeSong() {
  Song song{};
  song.length = 4;
  for (int r = 0; r < 4; ++r) {
    song.positions[r].patterns[0] = static_cast<int16_t>(r);
    song.positions[r].patterns[1] = static_cast<int16_t>(4 + r);
    song.positions[r].patterns[2] = static_cast<int16_t>(8 + r);
    song.positions[r].patterns[3] = static_cast<int16_t>(12 + r);
  }
  return song;
}

bool sameSong(const Song& lhs, const Song& rhs) {
  if (lhs.length != rhs.length || lhs.reverse != rhs.reverse) return false;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      if (lhs.positions[row].patterns[track] !=
          rhs.positions[row].patterns[track]) return false;
    }
    for (int voice = 0; voice < 2; ++voice) {
      if (lhs.stepsOverrideAt(voice, row) != rhs.stepsOverrideAt(voice, row))
        return false;
    }
  }
  return true;
}
}  // namespace

int main() {
  // Levels are five different things; 3 and 4 differ (lead space vs full lead).
  for (uint16_t bar = 0; bar < 8; ++bar) {
    const Voices v0 = voicesFor(0, bar), v1 = voicesFor(1, bar), v2 = voicesFor(2, bar);
    assert(!v0.drums && !v0.bass && !v0.lead && !v0.voice);
    assert(v1.drums && !v1.bass && !v1.lead);
    assert(v2.drums && v2.bass && !v2.lead);
    const Voices v3 = voicesFor(3, bar), v4 = voicesFor(4, bar);
    assert(v3.drums && v3.bass && !v3.voice);
    assert(v3.lead == ((bar % 4) != 2));          // A A' _ B
    assert(v4.lead && v4.voice);
  }

  // Presets.
  assert(presetCurve(Preset::Build).count == 5 && totalBars(presetCurve(Preset::Build)) == 20);
  assert(presetCurve(Preset::Drop).sections[3].energy == 1);
  assert(totalBars(presetCurve(Preset::Wave)) == 16);

  // Named arrangement roles are transient starting curves. Their musical
  // contours stay editable and materialize through the same Song row path.
  {
    const Curve intro = presetCurve(Preset::Intro);
    const Curve main = presetCurve(Preset::Main);
    const Curve development = presetCurve(Preset::Development);
    const Curve breakRole = presetCurve(Preset::Break);
    const Curve build = presetCurve(Preset::Build);
    const Curve drop = presetCurve(Preset::Drop);
    const Curve outro = presetCurve(Preset::Outro);
    assert(std::strcmp(presetName(Preset::Intro), "INTRO") == 0);
    assert(std::strcmp(presetName(Preset::Main), "MAIN") == 0);
    assert(std::strcmp(presetName(Preset::Development), "DEVELOPMENT") == 0);
    assert(std::strcmp(presetName(Preset::Break), "BREAK") == 0);
    assert(std::strcmp(presetName(Preset::Outro), "OUTRO") == 0);
    for (const Curve* role : {&intro, &main, &development, &breakRole,
                              &build, &drop, &outro}) {
      assert(validCurve(*role));
      assert(role->count > 0 && role->count <= kMaxSections);
      assert(totalBars(*role) > 0);
    }
    assert(intro.sections[0].energy == 1 &&
           energyRank(intro.sections[intro.count - 1].energy) >
               energyRank(intro.sections[0].energy));
    for (uint8_t i = 0; i < main.count; ++i)
      assert(main.sections[i].energy == 4);
    assert(development.count >= 3);
    assert(breakRole.sections[0].energy == 4);
    bool hasPause = false;
    for (uint8_t i = 0; i < breakRole.count; ++i)
      hasPause = hasPause || breakRole.sections[i].energy == 0;
    assert(hasPause && breakRole.sections[breakRole.count - 1].energy > 0);
    for (uint8_t i = 1; i < build.count; ++i)
      assert(energyRank(build.sections[i - 1].energy) <=
             energyRank(build.sections[i].energy));
    assert(drop.sections[0].energy < 4 && drop.sections[1].energy == 3 &&
           drop.sections[2].energy == 4 && drop.sections[3].energy == 1 &&
           drop.sections[4].energy == 4);
    for (uint8_t i = 1; i < outro.count; ++i)
      assert(energyRank(outro.sections[i - 1].energy) >=
             energyRank(outro.sections[i].energy));
  }

  // BUILD after the TAKE: 20 rows inserted, existing rows kept, sections restart the TAKE.
  {
    Song song = takeSong();
    song.setStepsOverride(0, 0, true);
    song.setStepsOverride(1, 1, true);
    assert(apply(song, presetCurve(Preset::Build), 0, 4, 4) == Status::Ok);
    assert(song.length == 24);
    // The copied source cells keep their explicit STEPS choice even when the
    // referenced slot is a Melody. Check an energy-4 section that retains both
    // synth tracks; the lower-energy section intentionally removes them.
    assert(song.stepsOverrideAt(0, 16));
    assert(!song.stepsOverrideAt(1, 16));
    assert(song.stepsOverrideAt(1, 17));
    assert(!song.stepsOverrideAt(0, 17));
    for (int r = 0; r < 4; ++r) assert(patternAt(song, r, SongTrack::SynthA) == r);  // untouched
    // Section 1 (energy 1, rows 4..7): drums only, in TAKE order.
    for (int b = 0; b < 4; ++b) {
      assert(patternAt(song, 4 + b, SongTrack::Drums) == 8 + b);
      assert(patternAt(song, 4 + b, SongTrack::SynthA) < 0);
      assert(patternAt(song, 4 + b, SongTrack::SynthB) < 0);
    }
    // Section 3 (energy 3, rows 12..15): lead rests on its third bar only.
    assert(patternAt(song, 12, SongTrack::SynthB) == 4);
    assert(patternAt(song, 13, SongTrack::SynthB) == 5);
    assert(patternAt(song, 14, SongTrack::SynthB) < 0);
    assert(patternAt(song, 15, SongTrack::SynthB) == 7);
    assert(patternAt(song, 14, SongTrack::SynthA) == 2);
    // Section 4 (energy 4, rows 16..19): everything, Voice lane included.
    assert(patternAt(song, 18, SongTrack::SynthB) == 6 && patternAt(song, 18, SongTrack::Voice) == 14);
  }

  // Inserted in the middle: rows after the insert point move down intact.
  {
    Song song = takeSong();
    Curve one{};
    one.sections[0] = Section{4, 2};
    one.count = 1;
    assert(apply(song, one, 0, 2, 2) == Status::Ok);  // a 2-row source cycles
    assert(song.length == 8);
    assert(patternAt(song, 2, SongTrack::SynthA) == 0 && patternAt(song, 3, SongTrack::SynthA) == 1);
    assert(patternAt(song, 4, SongTrack::SynthA) == 0 && patternAt(song, 5, SongTrack::SynthA) == 1);
    assert(patternAt(song, 6, SongTrack::SynthA) == 2 && patternAt(song, 7, SongTrack::SynthA) == 3);
  }

  // Energy 0 rows are real (silent) bars.
  {
    Song song = takeSong();
    Curve silent{};
    silent.sections[0] = Section{4, 0};
    silent.count = 1;
    assert(apply(song, silent, 0, 4, 4) == Status::Ok);
    assert(song.length == 8);
    for (int t = 0; t < SongPosition::kTrackCount; ++t) assert(song.positions[5].patterns[t] < 0);
  }

  // Refusals change nothing.
  {
    const Song before = takeSong();
    Song song = before;
    Curve none{};
    assert(apply(song, none, 0, 4, 4) == Status::NoSections);
    Song empty{};
    empty.length = 2;
    assert(apply(empty, presetCurve(Preset::Wave), 0, 2, 2) == Status::NoSource);
    Song full = before;
    full.length = Song::kMaxPositions - 10;
    assert(apply(full, presetCurve(Preset::Build), 0, 4, 4) == Status::NoRoom);
    assert(full.length == Song::kMaxPositions - 10);
    assert(apply(song, presetCurve(Preset::Wave), 3, 4, 4) == Status::BadRange);
    for (int r = 0; r < 4; ++r)
      for (int t = 0; t < SongPosition::kTrackCount; ++t)
        assert(song.positions[r].patterns[t] == before.positions[r].patterns[t]);
    assert(song.length == before.length);
  }

  // Malformed curves are refused before reading beyond the fixed section
  // array or inserting any rows.
  {
    Curve tooMany{};
    tooMany.count = kMaxSections + 1;
    for (uint8_t i = 0; i < kMaxSections; ++i)
      tooMany.sections[i] = Section{4, 2};
    Song song = takeSong();
    const Song before = song;
    assert(apply(song, tooMany, 0, 4, 4) == Status::BadRange);
    assert(sameSong(song, before));

    Curve badEnergy{};
    badEnergy.count = 1;
    badEnergy.sections[0] = Section{4, kMaxEnergy + 1};
    song = takeSong();
    const Song beforeBadEnergy = song;
    assert(apply(song, badEnergy, 0, 4, 4) == Status::BadRange);
    assert(sameSong(song, beforeBadEnergy));

    Curve badLength{};
    badLength.count = 1;
    badLength.sections[0] = Section{3, 2};
    song = takeSong();
    const Song beforeBadLength = song;
    assert(apply(song, badLength, 0, 4, 4) == Status::BadRange);
    assert(sameSong(song, beforeBadLength));
  }

  // H3: seven levels in the Up/Down order, codes 5 (A+B) and 6 (DR+B).
  {
    const uint8_t order[] = {0, 1, 2, 6, 5, 3, 4};  // SILENCE DR DR+A DR+B A+B +B SPACE FULL
    for (uint8_t r = 0; r < EnergyCurve::kEnergyLevels; ++r) {
      assert(EnergyCurve::energyRank(order[r]) == r && EnergyCurve::energyAtRank(r) == order[r]);
    }
    assert(EnergyCurve::kEnergyLevels == 7 && EnergyCurve::kMaxEnergy == 6);
    const Voices a = voicesFor(5, 0), dB = voicesFor(6, 0);
    assert(!a.drums && a.bass && a.lead && !a.voice);  // A+B: no drums
    assert(dB.drums && !dB.bass && dB.lead && !dB.voice);  // DR+B: no bass
    // Codes 0..4 keep their voices (the bar-3 lead rest of code 3 included).
    assert(voicesFor(4, 0).voice && voicesFor(4, 0).lead && voicesFor(4, 0).bass && voicesFor(4, 0).drums);
    assert(voicesFor(3, 2).lead == false && voicesFor(3, 3).lead == true);
  }

  // H5: the genre forms: 32 bars, at most 8 sections, and they apply to a TAKE.
  {
    const uint8_t modes[] = {9, 13, 10, 0, 5, 6};  // House, UKG, Techno, Acid, Dub, Trip-Hop
    for (uint8_t mode : modes) {
      const Curve curve = genreCurve(mode);
      assert(totalBars(curve) == 32 && curve.count <= kMaxSections);
      Song song = takeSong();
      assert(apply(song, curve, 0, 4, 4) == Status::Ok);
      assert(song.length == 4 + 32);
    }
    // WAVE for the other modes (the preset, 16 bars).
    assert(totalBars(genreCurve(3)) == totalBars(presetCurve(Preset::Wave)));
    // A full song refuses with NoRoom and stays unchanged.
    Song full{};
    full.length = Song::kMaxPositions - 10;
    for (int r = 0; r < full.length; ++r) full.positions[r].patterns[0] = static_cast<int16_t>(r % 8);
    const Song before = full;
    assert(apply(full, genreCurve(9), 0, 4, 4) == Status::NoRoom);
    assert(full.length == before.length);
    for (int r = 0; r < full.length; ++r)
      for (int t = 0; t < SongPosition::kTrackCount; ++t)
        assert(full.positions[r].patterns[t] == before.positions[r].patterns[t]);
  }

  // H4: an 8-row source over two 4-bar sections: the second section reads rows 4..7.
  {
    Song song{};
    song.length = 8;
    for (int r = 0; r < 8; ++r) song.positions[r].patterns[0] = static_cast<int16_t>(r);
    EnergyCurve::Curve two{};
    two.sections[0] = EnergyCurve::Section{4, 2};
    two.sections[1] = EnergyCurve::Section{4, 2};
    two.count = 2;
    assert(apply(song, two, 0, 8, 8) == Status::Ok);
    for (int b = 0; b < 8; ++b) assert(patternAt(song, 8 + b, SongTrack::SynthA) == b);
    // A 4-row source keeps the old repeat: the second section starts again on row 0.
    Song four = takeSong();
    assert(apply(four, two, 0, 4, 4) == Status::Ok);
    for (int b = 0; b < 4; ++b) assert(patternAt(four, 4 + b, SongTrack::SynthA) == b);
    for (int b = 0; b < 4; ++b) assert(patternAt(four, 8 + b, SongTrack::SynthA) == b);
  }

  // H3: the bass comes off before a drop only in a section that has drums.
  {
    // A+B (5) -> DR (1): no drums in the section, so the bass stays on its last bar.
    EnergyCurve::Curve curve{};
    curve.sections[0] = EnergyCurve::Section{4, 5};
    curve.sections[1] = EnergyCurve::Section{4, 1};
    curve.count = 2;
    Song song = takeSong();
    assert(apply(song, curve, 0, 4, 4) == Status::Ok);
    assert(patternAt(song, 7, SongTrack::SynthA) == 3);  // last bar of A+B keeps the bass
    // FULL (4) -> DR (1): drums in the section, so the bass goes on its last bar.
    EnergyCurve::Curve drop{};
    drop.sections[0] = EnergyCurve::Section{4, 4};
    drop.sections[1] = EnergyCurve::Section{4, 1};
    drop.count = 2;
    Song song2 = takeSong();
    assert(apply(song2, drop, 0, 4, 4) == Status::Ok);
    assert(patternAt(song2, 7, SongTrack::SynthA) < 0);
    assert(patternAt(song2, 6, SongTrack::SynthA) == 2);
  }

  std::puts("energy curve (SONG FORM): PASS");
  return 0;
}
