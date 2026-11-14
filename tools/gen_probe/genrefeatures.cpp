// Genre feature probe (analysis, not a test): what does the product path
// "MATERIAL G -> 4-bar TAKE, then D" produce for every genre and several takes?
//
// Usage: genrefeatures [takes=6] [print]      (engine log goes to stdout: grep -v it or use the T/G/X prefixes)
//   takes  number of TAKEs per genre (each is G on MATERIAL; Ctrl+Z between them)
//   print  also dump bar 1..4 of every lane for takes 0 and 1
//
// Output (TSV on stdout, '#' lines are comments):
//   T rows   one per genre/take: comparable numeric features of the 4-bar TAKE
//            columns after the lanes: A.att412 (bass attacks on steps 4 and 12,
//            mean per bar), adjSame (1 = bar 1 equals the previous take's bar 1,
//            - for take 0), idiom (the idiom template the engine wrote, or none),
//            grow (rows D added, then the D outcome: "DEVELOP + BREAK 8B", "NO ROOM", ...)
//   G rows   one per genre: means over the takes; grow column = takes D grew,
//            distinct takes, DEVELOP + BREAK 8B count, CAN'T GROW (refused by
//            the genre) count, adjacent equal pairs
//   X rows   genre-by-genre distance of bar-1 onset masks (drums / bass / lead);
//            the diagonal is the spread inside a genre
//
// The numbers find mistakes (wrong tempo, no kick on the beat, a bass that does
// not move, takes that are all the same, D refused). They do not prove that a
// genre sounds good: only listening does.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/idiom/genre_idiom.h"
#include "src/input/performance_keyboard.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/state/generation_request_state.h"
#include "src/ui/miniacid_display.h"
#include "src/ui/ui_common.h"
#include "src/ui/workflow_mode.h"

SerialMock Serial;
SDMock SD;

class NullGfx : public IGfx {
 public:
  std::vector<std::string> texts;
  std::vector<std::string> toasts;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  // The global toast is the only text drawn at y = height - 25 (ui_common drawToast).
  // Page status lines such as "NO SLOTS" or "NO ROOM" are drawn elsewhere and are not outcomes.
  void drawText(int, int y, const char* s) override {
    if (!s || !*s) return;
    texts.push_back(s);
    if (y == height() - 25) toasts.push_back(s);
  }
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

static const char* kGenre[] = {"Acid", "Synthwave", "Darksynth", "Electro", "Rave",
                               "Dub/Reggae", "TripHop", "Breaks", "Chip", "House",
                               "Techno", "HipHop", "FunkSoul", "UKGarage", "DnB", "LoFi"};

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

struct Bar {
  uint16_t kick = 0, snare = 0, hat = 0, perc = 0;  // bit = step
  int8_t note[2][16];
  bool slide[2][16];
  bool accent[2][16];
};

struct Take {
  std::vector<Bar> bars;
  float bpm = 0;
  int grow = -1;  // rows added by D, 0 = refused
  std::string growText;  // the D outcome (outcomeOf), "none" when the UI said nothing
  std::string idiom = "none";
};

// The D outcome as the UI words it. Only a text that STARTS with one of these
// phrases counts: "BREAKS 132 BPM" is the genre label and must not match BREAK.
// A bar count right after the phrase is kept: "DEVELOP + BREAK 8B IN SONG"
// gives "DEVELOP + BREAK 8B".
static std::string outcomeOf(const std::string& text) {
  // "HOUSE CAN'T GROW: ALT+V GENRE": the genre does not grow at all (the name
  // before the phrase varies, so this one is found anywhere in the text).
  if (text.find("CAN'T GROW") != std::string::npos) return "CAN'T GROW";
  static const char* kPhrases[] = {"GROW FAILED", "DEVELOP + BREAK", "DEVELOP ONLY",
                                   "BREAK ONLY", "GROW", "NO ROOM", "NO SLOTS", "SET 4B",
                                   "PRESS G", "THEN G", "TAKE: G"};
  for (const char* p : kPhrases) {
    const size_t n = std::strlen(p);
    if (text.compare(0, n, p) != 0) continue;
    std::string out = p;
    if (n < text.size() && text[n] == ' ') {
      size_t d = n + 1, e = d;
      while (e < text.size() && std::isdigit(static_cast<unsigned char>(text[e]))) ++e;
      if (e > d && e < text.size() && text[e] == 'B' &&
          (e + 1 == text.size() || text[e + 1] == ' '))
        out += " " + text.substr(d, e - d + 1);
    }
    return out;
  }
  return "";
}

static int pop(uint16_t v) { return __builtin_popcount(v); }

// An attack starts a note; a step with slide set and the same pitch as the
// step before it holds that note (the engine ties it).
static bool isAttack(const Bar& b, int lane, int i) {
  if (b.note[lane][i] < 0) return false;
  if (i > 0 && b.slide[lane][i] && b.note[lane][i - 1] == b.note[lane][i]) return false;
  return true;
}

static uint16_t attackMask(const Bar& b, int lane) {
  uint16_t m = 0;
  for (int i = 0; i < 16; ++i) if (isAttack(b, lane, i)) m |= static_cast<uint16_t>(1u << i);
  return m;
}

static bool readBar(MiniAcid& engine, Scene& scene, int row, Bar& out) {
  const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
  const SongPosition& pos = scene.songs[songSlot].positions[row];
  for (int lane = 0; lane < 2; ++lane) {
    const int cell = pos.patterns[lane];
    for (int i = 0; i < 16; ++i) {
      out.note[lane][i] = -1; out.slide[lane][i] = false; out.accent[lane][i] = false;
    }
    if (cell < 0) continue;
    const int bank = songPatternBank(cell), idx = songPatternIndexInBank(cell);
    const SynthPattern& p = lane == 0 ? scene.synthABanks[bank].patterns[idx]
                                      : scene.synthBBanks[bank].patterns[idx];
    for (int i = 0; i < 16; ++i) {
      out.note[lane][i] = p.steps[i].note;
      out.slide[lane][i] = p.steps[i].slide;
      out.accent[lane][i] = p.steps[i].accent;
    }
  }
  const int cell = pos.patterns[static_cast<int>(SongTrack::Drums)];
  if (cell >= 0) {
    const DrumPatternSet& d =
        scene.drumBanks[songPatternBank(cell)].patterns[songPatternIndexInBank(cell)];
    for (int i = 0; i < 16; ++i) {
      const uint16_t bit = static_cast<uint16_t>(1u << i);
      if (d.voices[0].steps[i].hit) out.kick |= bit;
      if (d.voices[1].steps[i].hit || d.voices[7].steps[i].hit) out.snare |= bit;
      if (d.voices[2].steps[i].hit || d.voices[3].steps[i].hit) out.hat |= bit;
      if (d.voices[4].steps[i].hit || d.voices[5].steps[i].hit || d.voices[6].steps[i].hit)
        out.perc |= bit;
    }
  }
  (void)engine;
  return true;
}

struct Feat {
  double bpm = 0;
  // drums
  double kickPerBar = 0, kick4 = 0, snarePerBar = 0, backbeat = 0, snareOn3 = 0;
  double hatPerBar = 0, hatOff = 0, drumOdd = 0, drumBarsDiffer = 0;
  // synth lanes
  double notes[2] = {0, 0}, lo[2] = {0, 0}, hi[2] = {0, 0}, distinct[2] = {0, 0};
  double onKick[2] = {0, 0}, off8[2] = {0, 0}, odd16[2] = {0, 0}, len[2] = {0, 0};
  double root[2] = {0, 0}, slides[2] = {0, 0}, barsDiffer[2] = {0, 0};
  double barRoots = 0;  // distinct lowest-bass-note pitch classes over the bars
  double att412 = 0;    // lane A (bass) attacks on steps 4 and 12, mean per bar (0..2)
};

static Feat measure(const Take& t, int rootPc) {
  Feat f;
  f.bpm = t.bpm;
  const double n = t.bars.empty() ? 1 : t.bars.size();
  int kickHits = 0, snareHits = 0, hatHits = 0, hatOffHits = 0, odd = 0;
  std::set<int> barRoot;
  for (size_t b = 0; b < t.bars.size(); ++b) {
    const Bar& bar = t.bars[b];
    kickHits += pop(bar.kick); snareHits += pop(bar.snare); hatHits += pop(bar.hat);
    hatOffHits += pop(bar.hat & 0x4444);
    odd += pop((bar.kick | bar.snare) & 0xAAAA);
    if ((bar.kick & 0x1111) == 0x1111) f.kick4 += 1;
    if ((bar.snare & 0x1010) == 0x1010) f.backbeat += 1;
    if ((bar.snare & 0x0100) && !(bar.snare & 0x1010)) f.snareOn3 += 1;
    if (b > 0 && (bar.kick != t.bars[0].kick || bar.snare != t.bars[0].snare ||
                  bar.hat != t.bars[0].hat || bar.perc != t.bars[0].perc))
      f.drumBarsDiffer += 1;
    int lowest = 999;
    for (int i = 0; i < 16; ++i)
      if (bar.note[0][i] >= 0) lowest = std::min<int>(lowest, bar.note[0][i]);
    if (lowest < 999) barRoot.insert(lowest % 12);
  }
  f.barRoots = static_cast<double>(barRoot.size());
  f.kickPerBar = kickHits / n; f.snarePerBar = snareHits / n; f.hatPerBar = hatHits / n;
  f.kick4 /= n; f.backbeat /= n; f.snareOn3 /= n;
  f.hatOff = hatHits ? double(hatOffHits) / hatHits : 0;
  f.drumOdd = (kickHits + snareHits) ? double(odd) / (kickHits + snareHits) : 0;

  for (int lane = 0; lane < 2; ++lane) {
    int attacks = 0, held = 0, lo = 999, hi = -1, onKick = 0, off8 = 0, odd16 = 0, root = 0,
        slides = 0;
    std::set<int> pitches;
    for (size_t b = 0; b < t.bars.size(); ++b) {
      const Bar& bar = t.bars[b];
      bool differs = false;
      for (int i = 0; i < 16; ++i) {
        if (b > 0 && (bar.note[lane][i] != t.bars[0].note[lane][i] ||
                      bar.slide[lane][i] != t.bars[0].slide[lane][i])) differs = true;
        const int note = bar.note[lane][i];
        if (note < 0) continue;
        if (!isAttack(bar, lane, i)) { ++held; continue; }
        ++attacks;
        if (bar.slide[lane][i]) ++slides;
        pitches.insert(note);
        lo = std::min(lo, note); hi = std::max(hi, note);
        if (bar.kick & (1u << i)) ++onKick;
        if ((i & 3) == 2) ++off8;
        if (i & 1) ++odd16;
        if (note % 12 == rootPc) ++root;
      }
      if (differs) f.barsDiffer[lane] += 1;
    }
    const double a = attacks ? attacks : 1;
    f.notes[lane] = attacks / n;
    f.lo[lane] = attacks ? lo : 0; f.hi[lane] = attacks ? hi : 0;
    f.distinct[lane] = static_cast<double>(pitches.size());
    f.onKick[lane] = onKick / a; f.off8[lane] = off8 / a; f.odd16[lane] = odd16 / a;
    f.len[lane] = (attacks + held) / a;
    f.root[lane] = root / a; f.slides[lane] = slides / n;
  }
  for (const Bar& bar : t.bars)
    for (int i : {4, 12})
      if (isAttack(bar, 0, i)) f.att412 += 1;
  f.att412 /= n;
  return f;
}

static void printHeader(const char* tag) {
  std::printf("%s\tgenre\ttake\tbpm\tkick/bar\tkick4fl\tsnr/bar\tbackbeat\tsnrOn3\that/bar\thatOff8\tdrOdd16\tdrBarsDiff"
              "\tA.n/bar\tA.lo\tA.hi\tA.dist\tA.onKick\tA.off8\tA.odd16\tA.len\tA.root\tA.slide/bar\tA.barsDiff\tA.barRoots"
              "\tB.n/bar\tB.lo\tB.hi\tB.dist\tB.onKick\tB.off8\tB.odd16\tB.len\tB.root\tB.barsDiff\tA.att412\tadjSame\tidiom\tgrow\n", tag);
}

static void printFeat(const char* tag, const char* genre, const char* take, const Feat& f,
                      const char* adj, const char* idiom, const char* grow) {
  std::printf("%s\t%s\t%s\t%.0f\t%.1f\t%.2f\t%.1f\t%.2f\t%.2f\t%.1f\t%.2f\t%.2f\t%.1f"
              "\t%.1f\t%s\t%s\t%.1f\t%.2f\t%.2f\t%.2f\t%.1f\t%.2f\t%.1f\t%.1f\t%.1f"
              "\t%.1f\t%s\t%s\t%.1f\t%.2f\t%.2f\t%.2f\t%.1f\t%.2f\t%.1f\t%.2f\t%s\t%s\t%s\n",
              tag, genre, take, f.bpm, f.kickPerBar, f.kick4, f.snarePerBar, f.backbeat,
              f.snareOn3, f.hatPerBar, f.hatOff, f.drumOdd, f.drumBarsDiffer,
              f.notes[0], nm(static_cast<int>(std::lround(f.lo[0]))).c_str(),
              nm(static_cast<int>(std::lround(f.hi[0]))).c_str(), f.distinct[0], f.onKick[0],
              f.off8[0], f.odd16[0], f.len[0], f.root[0], f.slides[0], f.barsDiffer[0], f.barRoots,
              f.notes[1], nm(static_cast<int>(std::lround(f.lo[1]))).c_str(),
              nm(static_cast<int>(std::lround(f.hi[1]))).c_str(), f.distinct[1], f.onKick[1],
              f.off8[1], f.odd16[1], f.len[1], f.root[1], f.barsDiffer[1], f.att412, adj, idiom,
              grow);
}

int main(int argc, char** argv) {
  const int takes = argc > 1 ? std::max(1, std::atoi(argv[1])) : 6;
  const bool print = argc > 2;
  const bool applyTempo = std::getenv("GP_PROBE_NO_TEMPO") == nullptr;
  // GP_PROBE_RECIPE=<id>: measure that recipe where the genre has it (BASE elsewhere).
  const int recipe = std::getenv("GP_PROBE_RECIPE") ? std::atoi(std::getenv("GP_PROBE_RECIPE")) : 0;
  FILE* quiet = std::freopen("/dev/null", "w", stderr);
  (void)quiet;
  std::vector<std::vector<Take>> all(kGenerativeModeCount);

  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    // One fresh project per TAKE: k throwaway presses of G (each taken back
    // with Ctrl+Z, as a musician who does not like a TAKE would) and then the
    // TAKE that is measured, followed by D. A project that already holds a
    // grown cycle would answer the next D with NO ROOM, which says nothing
    // about the genre.
    for (int k = 0; k < takes; ++k) {
      const auto root = std::filesystem::temp_directory_path() /
                        ("gp_genrefeat_" + std::to_string(mode) + "_" + std::to_string(k));
      std::error_code ec;
      std::filesystem::remove_all(root, ec);
      std::filesystem::create_directories(root);
      std::filesystem::current_path(root);
      SD.setRoot(root);
      GroovePuterPlatform::clearMaterialPublication("genrefeat", 0);
      PatternPagingService::setProjectName("genrefeat");
      SceneStorageSdl storage;
      storage.setCurrentSceneName("default");
      MiniAcid engine(44100, &storage);
      engine.init();
      engine.setSongMode(false);
      engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
      engine.genreManager().setRecipe(recipe);
      auto& scene = engine.sceneManager().currentScene();
      if (!GroovePuterRhythm::isRecipeAvailable(static_cast<GenerativeMode>(mode),
                                                 static_cast<GenreRecipeId>(recipe))) {
        engine.genreManager().setRecipe(0);
      }
      if (applyTempo) {
        const auto profile = GroovePuterRhythm::generationProfileFor(scene.genre);
        if (profile.corridor.suggestedBpm > 0) engine.setBpm(profile.corridor.suggestedBpm);
      }
      NullGfx gfx;
      MusicalEventRouter router;
      PerformanceKeyboard keyboard(router);
      MiniAcidDisplay display(gfx, engine, keyboard);
      display.dismissSplash();
      display.goToPage(WorkflowPages::kPhrase);
      display.update();
      const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);

      for (int skip = 0; skip < k; ++skip) {
        auto g = key('g');
        display.handleEvent(g);
        for (int i = 0; i < 3; ++i) display.update();
        auto z = key('z', true);
        display.handleEvent(z);
        for (int i = 0; i < 2; ++i) display.update();
      }
      auto g = key('g');
      display.handleEvent(g);
      for (int i = 0; i < 3; ++i) display.update();
      const int afterG = scene.songs[songSlot].length;
      if (std::getenv("GP_PROBE_SLOTS") != nullptr) {
        // Which of the page's 16 slots hold material right after the TAKE.
        std::printf("#S %s take %d songLength=%d slots:", kGenre[mode], k, afterG);
        for (int slot = 0; slot < 16; ++slot) {
          const bool used = !scene.synthABanks[slot / 8].patterns[slot % 8].isEmpty() ||
                            !scene.synthBBanks[slot / 8].patterns[slot % 8].isEmpty() ||
                            !scene.drumBanks[slot / 8].patterns[slot % 8].isEmpty();
          std::printf("%c", used ? 'x' : '.');
        }
        std::printf("\n");
      }
      Take take;
      take.bpm = engine.bpm();
      // The idiom template the engine wrote for this TAKE, if it kept one.
      if (const auto* origin = engine.generatedSynthAOrigin()) {
        uint8_t level = 0;
        if (const auto* v = GenreIdiom::idiomTemplateVariant(origin->common.idiomTemplate, level))
          take.idiom = v->name;
      }
      for (int row = std::max(0, afterG - 4); row < afterG; ++row) {
        Bar bar{};
        readBar(engine, scene, row, bar);
        take.bars.push_back(bar);
      }
      // Clear the toast left by G (an empty toast with 0 ms expires at once), so the
      // toast read after D is the one D showed.
      // D is growKeptPhrase (phrase_page.cpp): the same keys as the owner's path in
      // src/ui/docs/keys.md (G on the defaults, LENGTH 4B and STYLE P3, then D on the
      // unedited TAKE). Its outcome is the toast it shows: BREAK ONLY 4B means the
      // DEVELOP section was skipped (developSkipped), DEVELOP + BREAK 8B means both.
      // NO ROOM is refused here without the R step that keys.md asks for.
      UI::showToast("", 0);
      display.update();
      gfx.texts.clear();
      gfx.toasts.clear();
      auto d = key('d');
      display.handleEvent(d);
      for (int i = 0; i < 3; ++i) display.update();
      const int afterD = scene.songs[songSlot].length;
      take.grow = afterD - afterG;
      take.growText = "none";
      // GP_PROBE_TEXTS=1: every text the UI drew during D, before the outcome is picked.
      if (std::getenv("GP_PROBE_TEXTS") != nullptr)
        for (const auto& t : gfx.toasts) std::printf("#D %s t%d toast: %s\n", kGenre[mode], k, t.c_str());
      for (const auto& t : gfx.toasts) {
        const std::string outcome = outcomeOf(t);
        if (!outcome.empty()) {
          take.growText = outcome;
          break;
        }
      }
      all[mode].push_back(take);
    }
  }

  const int rootPc = 0;  // default project key is C
  printHeader("T");
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    for (size_t k = 0; k < all[mode].size(); ++k) {
      const Feat f = measure(all[mode][k], rootPc);
      char grow[96];
      std::snprintf(grow, sizeof(grow), "%d:%s", all[mode][k].grow,
                    all[mode][k].growText.c_str());
      // Bar 1 of this take equal to bar 1 of the previous take (all lanes, drums too).
      const char* adj = "-";
      if (k > 0 && !all[mode][k].bars.empty() && !all[mode][k - 1].bars.empty())
        adj = std::memcmp(&all[mode][k].bars[0], &all[mode][k - 1].bars[0], sizeof(Bar)) == 0
                  ? "1" : "0";
      printFeat("T", kGenre[mode], std::to_string(k).c_str(), f, adj,
                all[mode][k].idiom.c_str(), grow);
      if (print && k < 2) {
        for (size_t b = 0; b < all[mode][k].bars.size(); ++b) {
          const Bar& bar = all[mode][k].bars[b];
          std::printf("#  %s t%zu bar%zu K:", kGenre[mode], k, b + 1);
          for (int i = 0; i < 16; ++i) std::printf("%c", bar.kick & (1u << i) ? 'x' : '.');
          std::printf(" S:");
          for (int i = 0; i < 16; ++i) std::printf("%c", bar.snare & (1u << i) ? 'x' : '.');
          std::printf(" H:");
          for (int i = 0; i < 16; ++i) std::printf("%c", bar.hat & (1u << i) ? 'x' : '.');
          std::printf("\n#     A:");
          for (int i = 0; i < 16; ++i)
            std::printf(" %s%s", nm(bar.note[0][i]).c_str(), bar.slide[0][i] ? "~" : "");
          std::printf("\n#     B:");
          for (int i = 0; i < 16; ++i)
            std::printf(" %s%s", nm(bar.note[1][i]).c_str(), bar.slide[1][i] ? "~" : "");
          std::printf("\n");
        }
      }
    }
  }

  // Per-genre means, unique takes, and how often D could grow the TAKE.
  std::printf("#\n# G rows: means over the takes; uniq = distinct takes (all lanes, all bars) of N;\n"
              "# grown = takes D extended (rows added > 0) of N.\n");
  printHeader("G");
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    Feat sum;
    const auto& v = all[mode];
    const double c = v.empty() ? 1 : v.size();
    int grown = 0, unique = 0, developBreak8 = 0, adjSame = 0, refused = 0;
    for (size_t k = 0; k < v.size(); ++k) {
      const Feat f = measure(v[k], rootPc);
      const double* src = reinterpret_cast<const double*>(&f);
      double* dst = reinterpret_cast<double*>(&sum);
      for (size_t i = 0; i < sizeof(Feat) / sizeof(double); ++i) dst[i] += src[i] / c;
      grown += v[k].grow > 0;
      developBreak8 += v[k].growText == "DEVELOP + BREAK 8B";
      refused += v[k].growText == "CAN'T GROW";
      if (k > 0 && !v[k].bars.empty() && !v[k - 1].bars.empty())
        adjSame += std::memcmp(&v[k].bars[0], &v[k - 1].bars[0], sizeof(Bar)) == 0;
      bool same = false;
      for (size_t j = 0; j < k && !same; ++j) {
        same = v[j].bars.size() == v[k].bars.size();
        for (size_t b = 0; same && b < v[k].bars.size(); ++b)
          same = std::memcmp(&v[j].bars[b], &v[k].bars[b], sizeof(Bar)) == 0;
      }
      unique += !same;
    }
    char grow[200];
    std::snprintf(grow, sizeof(grow),
                  "grown %d/%zu uniq %d/%zu DEVELOP+BREAK8B %d/%zu CANTGROW %d/%zu adjSame %d/%zu",
                  grown, v.size(), unique, v.size(), developBreak8, v.size(), refused, v.size(),
                  adjSame, v.size() > 1 ? v.size() - 1 : 0);
    printFeat("G", kGenre[mode], "mean", sum, "-", "-", grow);
  }

  // Genre-by-genre distance: mean Hamming distance (steps of 16) between the
  // bar-1 onset masks of every take pair. The diagonal is the spread inside a
  // genre. A pair whose distance is no larger than both diagonals is not told
  // apart by its onsets (pitch and sound are not in this number).
  static const char* kLane[] = {"drums(K+S)", "bass", "lead"};
  for (int lane = 0; lane < 3; ++lane) {
    std::printf("#\n# X rows, %s: mean onset distance, steps of %d\nX.%s\t", kLane[lane],
                lane == 0 ? 32 : 16, kLane[lane]);
    for (int b = 0; b < kGenerativeModeCount; ++b) std::printf("%.4s\t", kGenre[b]);
    std::printf("\n");
    for (int a = 0; a < kGenerativeModeCount; ++a) {
      std::printf("X.%s\t%s", kLane[lane], kGenre[a]);
      for (int b = 0; b < kGenerativeModeCount; ++b) {
        double total = 0; int pairs = 0;
        for (size_t i = 0; i < all[a].size(); ++i) {
          for (size_t j = 0; j < all[b].size(); ++j) {
            if (a == b && i >= j) continue;
            if (all[a][i].bars.empty() || all[b][j].bars.empty()) continue;
            const Bar& x = all[a][i].bars[0];
            const Bar& y = all[b][j].bars[0];
            if (lane == 0) total += pop(x.kick ^ y.kick) + pop(x.snare ^ y.snare);
            else total += pop(attackMask(x, lane - 1) ^ attackMask(y, lane - 1));
            ++pairs;
          }
        }
        std::printf("\t%.1f", pairs ? total / pairs : 0.0);
      }
      std::printf("\n");
    }
  }
  return 0;
}
