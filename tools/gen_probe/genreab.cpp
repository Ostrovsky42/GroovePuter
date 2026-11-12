// Genre A/B renderer (0.9.24 T01). Host only, not part of the firmware.
//
// For every genre and take: a fresh project, the same procedure as
// genrefeatures (k throwaway G + Ctrl+Z, then the measured G on MATERIAL), the
// genre tempo, and the product engine. Each take yields
//   <genre>_t<k>_<tag>.wav   the 4-bar TAKE looped twice (8 bars), 22050 Hz, mix C
//   <genre>_t<k>_<tag>.mid   the same 8 bars as the WAV (the TAKE, then the same TAKE again), format-1 SMF
//   <genre>_t<k>_<tag>.tsv   one row per sounding note of those 8 bars: bar, step, track, channel, pitch, steps, velocity
// plus manifest_<tag>.tsv (without sha256; render_ab.sh adds those).
//
// Usage: genreab --tag <tag> --out <dir> [--takes N] [--genres a,b,c]
//   defaults: takes 4, genres house,uk_garage,techno,acid,dub,trip_hop
//   a genre key is one of the names in kGenreKeys below
//
// MIDI timing: 480 PPQ, one 16th step = 120 ticks, bar = 1920 ticks, tempo from
// the engine. Channels are the engine's routes (Synth A = GroovePuterMidi::kSynthAChannel,
// Synth B = kSynthBChannel, both 0-based in the status byte). Drums go to
// channel 10 with a voice -> GM note table that lives in this tool, not in the
// engine (the engine has no such table on this path); the TSV says so per row.
// Velocity: 100 base, +kPatternMidiAccentVelocityBoost on accent, clamp 1..127.
// The engine's own event.velocity is not read here.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "src/audio/audio_config.h"
#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/idiom/genre_idiom.h"
#include "src/input/performance_keyboard.h"
#include "src/midi/cutoff_sweep.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/state/generation_request_state.h"
#include "src/ui/miniacid_display.h"
#include "src/ui/workflow_mode.h"

SerialMock Serial;
SDMock SD;

class NullGfx : public IGfx {
 public:
  std::vector<std::string> texts;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* s) override { if (s && *s) texts.push_back(s); }
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor) override {}
  void drawCircle(int, int, int, IGfxColor) override {}
  void drawKnobFace(int, int, int, IGfxColor, IGfxColor) override {}
  void fillRect(int, int, int, int, IGfxColor) override {}
  void fillCircle(int, int, int, IGfxColor) override {}
  void drawLine(int32_t, int32_t, int32_t, int32_t, IGfxColor) override {}
  void setRotation(int) override {}
  void setTextColor(IGfxColor) override {}
  void setTextColor(uint16_t) override {}
  void setFont(GfxFont) override {}
  void startWrite() override {}
  void endWrite() override {}
  void flush() override {}
  int textWidth(const char* s) const override { return s ? std::strlen(s) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

struct GenreKey { const char* key; int mode; const char* label; };
static const GenreKey kGenreKeys[] = {
    {"acid", 0, "Acid"},           {"outrun", 1, "Outrun"},       {"darksynth", 2, "Darksynth"},
    {"electro", 3, "Electro"},     {"rave", 4, "Rave"},           {"dub", 5, "Dub-Reggae"},
    {"trip_hop", 6, "TripHop"},    {"breaks", 7, "Breaks"},       {"chip", 8, "Chip"},
    {"house", 9, "House"},         {"techno", 10, "Techno"},      {"hiphop", 11, "HipHop"},
    {"funk_soul", 12, "FunkSoul"}, {"uk_garage", 13, "UKGarage"}, {"dnb", 14, "DnB"},
    {"lofi", 15, "LoFi"},
};
static const char* kDefaultGenres = "house,uk_garage,techno,acid,dub,trip_hop";

static constexpr int kPpq = 480;
static constexpr int kStepTicks = kPpq / 4;       // 16th note
static constexpr int kBarTicks = kStepTicks * 16;  // 4/4 bar of 16 steps
static constexpr int kTakeBars = 4;
static constexpr int kLoops = 2;                   // WAV = take played twice
static constexpr int kBaseVelocity = 100;
static constexpr int kMidiGmDrum[8] = {36, 38, 42, 46, 41, 45, 50, 39};

static const char* kDrumName[8] = {"v0_kick", "v1_snare", "v2_hat_c", "v3_hat_o",
                                   "v4_perc", "v5_perc", "v6_perc", "v7_clap"};

static std::string nm(int n) {
  static const char* k[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if (n < 0) return ".";
  return std::string(k[n % 12]) + std::to_string(n / 12 - 1);
}

static UIEvent key(char c, bool ctrl = false) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = c;
  e.ctrl = ctrl;
  return e;
}

struct NoteEv {
  int bar, step, track;  // track 0 = A, 1 = B, 2 = drums
  int channel;           // 0-based MIDI channel
  int pitch, steps, velocity;
  std::string name;
};

// Engine rule for a pattern note's wire velocity (see the header comment).
static int velocityFor(bool accent) {
  const int v = kBaseVelocity + (accent ? kPatternMidiAccentVelocityBoost : 0);
  return std::clamp(v, 1, 127);
}

// Notes of the TAKE: rows [first, first+kTakeBars) of the song.
static std::vector<NoteEv> collectNotes(Scene& scene, int songSlot, int first) {
  std::vector<NoteEv> out;
  for (int b = 0; b < kTakeBars; ++b) {
    const SongPosition& pos = scene.songs[songSlot].positions[first + b];
    for (int lane = 0; lane < 2; ++lane) {
      const int cell = pos.patterns[lane];
      if (cell < 0) continue;
      const int bank = songPatternBank(cell), idx = songPatternIndexInBank(cell);
      const SynthPattern& p = lane == 0 ? scene.synthABanks[bank].patterns[idx]
                                        : scene.synthBBanks[bank].patterns[idx];
      const int channel = lane == 0 ? GroovePuterMidi::kSynthAChannel
                                    : GroovePuterMidi::kSynthBChannel;
      for (int i = 0; i < 16; ++i) {
        const int note = p.steps[i].note;
        if (note < 0) continue;
        // A slide step with the same pitch as the step before it is a tie, not an attack.
        if (i > 0 && p.steps[i].slide && p.steps[i - 1].note == note) continue;
        int steps = 1;
        for (int j = i + 1; j < 16 && p.steps[j].slide && p.steps[j].note == note; ++j) ++steps;
        out.push_back({b, i, lane, channel, note, steps,
                       velocityFor(p.steps[i].accent), nm(note)});
      }
    }
    const int cell = pos.patterns[static_cast<int>(SongTrack::Drums)];
    if (cell < 0) continue;
    const DrumPatternSet& d =
        scene.drumBanks[songPatternBank(cell)].patterns[songPatternIndexInBank(cell)];
    for (int v = 0; v < 8; ++v)
      for (int i = 0; i < 16; ++i) {
        if (!d.voices[v].steps[i].hit) continue;
        out.push_back({b, i, 2, 9, kMidiGmDrum[v], 1,
                       velocityFor(d.voices[v].steps[i].accent), kDrumName[v]});
      }
  }
  return out;
}

// --- SMF (format 1, 4 tracks: conductor, Synth A, Synth B, drums) ---------

struct MidiEv {
  uint32_t tick;
  int order;  // note-off before note-on at the same tick
  uint8_t status, d1, d2;
};

static void putVarLen(std::vector<uint8_t>& out, uint32_t v) {
  uint8_t buf[4];
  int n = 0;
  do { buf[n++] = static_cast<uint8_t>(v & 0x7F); v >>= 7; } while (v);
  for (int i = n - 1; i >= 0; --i) out.push_back(buf[i] | (i ? 0x80 : 0));
}

static void putBE32(std::vector<uint8_t>& out, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) out.push_back(static_cast<uint8_t>(v >> s));
}

static void putBE16(std::vector<uint8_t>& out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

static void putTrack(std::vector<uint8_t>& out, const std::vector<uint8_t>& body) {
  out.insert(out.end(), {'M', 'T', 'r', 'k'});
  putBE32(out, static_cast<uint32_t>(body.size()));
  out.insert(out.end(), body.begin(), body.end());
}

static void putName(std::vector<uint8_t>& body, const char* name) {
  body.push_back(0);
  body.push_back(0xFF); body.push_back(0x03);
  putVarLen(body, static_cast<uint32_t>(std::strlen(name)));
  body.insert(body.end(), name, name + std::strlen(name));
}

static std::vector<uint8_t> buildSmf(const std::vector<NoteEv>& notes, float bpm, int bars) {
  std::vector<uint8_t> file = {'M', 'T', 'h', 'd'};
  putBE32(file, 6);
  putBE16(file, 1);  // format 1
  putBE16(file, 4);  // tracks
  putBE16(file, kPpq);

  const uint32_t endTick = static_cast<uint32_t>(bars * kBarTicks);
  std::vector<uint8_t> conductor;
  putName(conductor, "GroovePuter take");
  const uint32_t usPerQuarter = static_cast<uint32_t>(std::lround(60000000.0 / bpm));
  putVarLen(conductor, 0);
  conductor.insert(conductor.end(), {0xFF, 0x51, 0x03,
      static_cast<uint8_t>(usPerQuarter >> 16), static_cast<uint8_t>(usPerQuarter >> 8),
      static_cast<uint8_t>(usPerQuarter)});
  putVarLen(conductor, 0);
  conductor.insert(conductor.end(), {0xFF, 0x58, 0x04, 4, 2, 24, 8});
  putVarLen(conductor, endTick);
  conductor.insert(conductor.end(), {0xFF, 0x2F, 0x00});
  putTrack(file, conductor);

  const char* names[3] = {"Synth A", "Synth B", "Drums"};
  for (int t = 0; t < 3; ++t) {
    std::vector<MidiEv> evs;
    for (const NoteEv& n : notes) {
      if (n.track != t) continue;
      const uint32_t on = static_cast<uint32_t>((n.bar * 16 + n.step) * kStepTicks);
      const uint32_t off = on + static_cast<uint32_t>(n.steps * kStepTicks);
      const uint8_t ch = static_cast<uint8_t>(n.channel & 0x0F);
      evs.push_back({on, 1, static_cast<uint8_t>(0x90 | ch), static_cast<uint8_t>(n.pitch),
                     static_cast<uint8_t>(n.velocity)});
      evs.push_back({off, 0, static_cast<uint8_t>(0x80 | ch), static_cast<uint8_t>(n.pitch), 0});
    }
    std::stable_sort(evs.begin(), evs.end(), [](const MidiEv& a, const MidiEv& b) {
      return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });
    std::vector<uint8_t> body;
    putName(body, names[t]);
    uint32_t last = 0;
    for (const MidiEv& e : evs) {
      putVarLen(body, e.tick - last);
      body.insert(body.end(), {e.status, e.d1, e.d2});
      last = e.tick;
    }
    putVarLen(body, endTick > last ? endTick - last : 0);
    body.insert(body.end(), {0xFF, 0x2F, 0x00});
    putTrack(file, body);
  }
  return file;
}

// --- WAV ------------------------------------------------------------------

static void writeWav(const std::string& path, const std::vector<int16_t>& pcm, uint32_t rate) {
  FILE* f = std::fopen(path.c_str(), "wb");
  const uint32_t data = static_cast<uint32_t>(pcm.size() * 2);
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); u32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f);
  u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
  std::fwrite("data", 1, 4, f); u32(data);
  std::fwrite(pcm.data(), 2, pcm.size(), f);
  std::fclose(f);
}

static void writeFile(const std::string& path, const std::vector<uint8_t>& bytes) {
  FILE* f = std::fopen(path.c_str(), "wb");
  std::fwrite(bytes.data(), 1, bytes.size(), f);
  std::fclose(f);
}

// --- one take -------------------------------------------------------------

static bool renderTake(const GenreKey& g, int k, const std::string& tag, const std::string& out,
                       const std::string& workRoot, FILE* manifest) {
  const std::filesystem::path work = std::filesystem::path(workRoot) /
                                     (std::string(g.key) + "_t" + std::to_string(k));
  std::error_code ec;
  std::filesystem::remove_all(work, ec);
  std::filesystem::create_directories(work);
  std::filesystem::current_path(work);
  SD.setRoot(work);
  GroovePuterPlatform::clearMaterialPublication("genreab", 0);
  PatternPagingService::setProjectName("genreab");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(static_cast<float>(kSampleRate), &storage);
  engine.init();
  engine.setSongMode(false);
  engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(g.mode));
  engine.genreManager().setRecipe(0);
  auto& scene = engine.sceneManager().currentScene();
  const auto profile = GroovePuterRhythm::generationProfileFor(scene.genre);
  if (profile.corridor.suggestedBpm > 0) engine.setBpm(profile.corridor.suggestedBpm);

  NullGfx gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx, engine, keyboard);
  display.dismissSplash();
  display.goToPage(WorkflowPages::kPhrase);
  display.update();
  const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);

  for (int skip = 0; skip < k; ++skip) {  // throwaway presses, each taken back
    auto gk = key('g');
    display.handleEvent(gk);
    for (int i = 0; i < 3; ++i) display.update();
    auto z = key('z', true);
    display.handleEvent(z);
    for (int i = 0; i < 2; ++i) display.update();
  }
  auto gk = key('g');
  display.handleEvent(gk);
  for (int i = 0; i < 3; ++i) display.update();
  const int afterG = scene.songs[songSlot].length;
  const int first = afterG - kTakeBars;
  if (first < 0) {
    std::printf("# genreab FAIL %s t%d: G gave %d rows, no TAKE\n", g.label, k, afterG);
    return false;
  }

  // MIDI and TSV carry the same 8 bars as the WAV: the TAKE, then the same TAKE again.
  const std::vector<NoteEv> take = collectNotes(scene, songSlot, first);
  std::vector<NoteEv> notes = take;
  for (NoteEv n : take) { n.bar += kTakeBars; notes.push_back(n); }
  const float bpm = engine.bpm();
  std::string idiom = "none";
  if (const auto* origin = engine.generatedSynthAOrigin()) {
    uint8_t level = 0;
    if (const auto* v = GenreIdiom::idiomTemplateVariant(origin->common.idiomTemplate, level))
      idiom = v->name;
  }

  // WAV: the TAKE looped twice, as takerender does.
  engine.setSongMode(true);
  engine.setLoopMode(true);
  engine.setLoopRange(first, afterG - 1);
  engine.setSongPosition(first);
  engine.start();
  const double seconds = kLoops * kTakeBars * 4 * 60.0 / bpm;
  std::vector<int16_t> pcm(static_cast<size_t>(seconds * kSampleRate));
  std::set<int> seen;
  for (size_t at = 0; at < pcm.size(); at += 512) {
    const size_t n = std::min<size_t>(512, pcm.size() - at);
    engine.generateAudioBuffer(pcm.data() + at, n);
    seen.insert(engine.currentSongPosition());
  }
  engine.stop();
  engine.setSongMode(false);
  engine.setLoopMode(false);
  double sq = 0; int peak = 0;
  for (int16_t s : pcm) { peak = std::max(peak, std::abs(static_cast<int>(s))); sq += double(s) * s; }
  const double rms = pcm.empty() ? 0 : std::sqrt(sq / pcm.size()) / 32768.0;
  const double peakF = peak / 32768.0;

  const std::string stem = out + "/" + g.key + "_t" + std::to_string(k) + "_" + tag;
  writeWav(stem + ".wav", pcm, kSampleRate);
  writeFile(stem + ".mid", buildSmf(notes, bpm, kTakeBars * kLoops));

  // TSV: header comment, column row, one row per note.
  int counts[3] = {0, 0, 0};
  FILE* tsv = std::fopen((stem + ".tsv").c_str(), "w");
  std::fprintf(tsv, "# genre=%s take=%d tag=%s bpm=%.2f idiom=%s bars=%d take_bars=%d loops=%d\n",
               g.label, k, tag.c_str(), bpm, idiom.c_str(), kTakeBars * kLoops, kTakeBars, kLoops);
  std::fprintf(tsv, "bar\tstep\ttrack\tchannel\tpitch\tsteps\tvelocity\tname\n");
  for (const NoteEv& n : notes) {
    ++counts[n.track];
    std::fprintf(tsv, "%d\t%d\t%s\t%d\t%d\t%d\t%d\t%s\n", n.bar, n.step,
                 n.track == 0 ? "A" : n.track == 1 ? "B" : "D", n.channel + 1, n.pitch,
                 n.steps, n.velocity, n.name.c_str());
  }
  std::fclose(tsv);

  std::fprintf(manifest, "%s_t%d_%s.wav\t%s\t%d\t%s\t%.2f\t%s\t%d\t%d\t%d\t%d\t%.4f\t%.1f\t%zu\n",
               g.key, k, tag.c_str(), g.label, k, tag.c_str(), bpm, idiom.c_str(),
               static_cast<int>(notes.size()), counts[0], counts[1], counts[2], peakF,
               rms > 0 ? 20 * std::log10(rms) : -999.0, pcm.size());
  std::printf("# %s t%d bpm=%.2f idiom=%s rows=%d..%d notes A=%d B=%d D=%d peak=%.3f rms=%.1fdBFS played:",
              g.label, k, bpm, idiom.c_str(), first, afterG - 1, counts[0], counts[1], counts[2],
              peakF, rms > 0 ? 20 * std::log10(rms) : -999.0);
  for (int s : seen) std::printf(" %d", s);
  std::printf("\n");
  return true;
}

int main(int argc, char** argv) {
  std::string tag, out, genresArg = kDefaultGenres;
  int takes = 4;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--tag" && i + 1 < argc) tag = argv[++i];
    else if (a == "--out" && i + 1 < argc) out = argv[++i];
    else if (a == "--takes" && i + 1 < argc) takes = std::atoi(argv[++i]);
    else if (a == "--genres" && i + 1 < argc) genresArg = argv[++i];
    else { std::fprintf(stderr, "usage: genreab --tag T --out DIR [--takes N] [--genres a,b]\n"); return 2; }
  }
  if (tag.empty() || out.empty() || takes < 1) {
    std::fprintf(stderr, "genreab: --tag and --out are required, --takes >= 1\n");
    return 2;
  }
  out = std::filesystem::absolute(out).string();
  std::filesystem::create_directories(out);
  const std::string workRoot = out + "/.work";
  std::vector<GenreKey> genres;
  for (size_t p = 0; p <= genresArg.size();) {
    const size_t q = std::min(genresArg.find(',', p), genresArg.size());
    const std::string name = genresArg.substr(p, q - p);
    bool found = false;
    for (const auto& g : kGenreKeys) if (name == g.key) { genres.push_back(g); found = true; }
    if (!found) { std::fprintf(stderr, "genreab: unknown genre '%s'\n", name.c_str()); return 2; }
    p = q + 1;
  }

  FILE* quiet = std::freopen("/dev/null", "w", stderr);
  (void)quiet;
  const std::string manifestPath = out + "/manifest_" + tag + ".tsv";
  FILE* manifest = std::fopen(manifestPath.c_str(), "w");
  std::fprintf(manifest, "file\tgenre\ttake\ttag\tbpm\tidiom\tnotes\tnotes_A\tnotes_B\tnotes_D\tpeak\trms_dbfs\tsamples\n");
  int failures = 0;
  for (const auto& g : genres)
    for (int k = 0; k < takes; ++k)
      if (!renderTake(g, k, tag, out, workRoot, manifest)) ++failures;
  std::fclose(manifest);
  std::error_code ec;
  std::filesystem::remove_all(workRoot, ec);
  return failures ? 1 : 0;
}
