// GroovePuter 0.9.14 P0 preparation corpus (host-only measurement; no product code).
//
// Answers, before any product path exists:
//   S1  Structural table: genre x ordinals 0..7 at P3, laws DevelopReturn (DEVELOP) and
//       SparseDrift (BREAK) vs the Loop phrase A: which lanes change in which section.
//   S2  Does the Loop phrase differ between depth P2 and P3 for the same identity?
//   S3  Mix table: synth-only vs drums-only RMS, peak and time at the ceiling for candidate
//       fader sets (diagnostic only; RMS is not a target).
//
// Output: report on stdout; build/m0a/p0/{p0_structural.tsv,p0_mix.tsv}.

#define M0A_RENDER_NO_MAIN
#include "m0a_render.cpp"

namespace {

const R::RealizationLevel kP2 = R::RealizationLevel::P2Variation;
const R::RealizationLevel kP3 = R::RealizationLevel::P3Transformation;

bool sameDrums(const BarRec& a, const BarRec& b) {
  for (int l = 0; l < 8; ++l) if (a.drum[l] != b.drum[l]) return false;
  return true;
}
bool sameLane(const LaneRec& a, const LaneRec& b) {
  if (a.attacks != b.attacks || a.cont != b.cont) return false;
  for (int s = 0; s < 16; ++s) if (a.noteAt[s] != b.noteAt[s]) return false;  // exact note incl. register
  return true;
}
bool samePc(const LaneRec& a, const LaneRec& b) {
  for (int s = 0; s < 16; ++s) if (a.noteAt[s] % 12 != b.noteAt[s] % 12) return false;
  return true;
}

struct SectionDiff {
  int drums = 0, bassRhythm = 0, bassPitch = 0, chord = 0;
  bool bassOrChord() const { return bassRhythm + bassPitch + chord > 0; }
};

// Bars of `v` that differ from the same bar of Loop phrase `a`.
SectionDiff diffAgainst(const Phrase& a, const Phrase& v) {
  SectionDiff d;
  for (size_t i = 0; i < v.bar.size() && i < a.bar.size(); ++i) {
    d.drums += !sameDrums(v.bar[i], a.bar[i]);
    d.bassRhythm += v.bar[i].a.attacks != a.bar[i].a.attacks;
    d.bassPitch += !samePc(v.bar[i].a, a.bar[i].a);
    d.chord += (v.bar[i].b.attacks != a.bar[i].b.attacks) || !samePc(v.bar[i].b, a.bar[i].b);
  }
  return d;
}

double db(double x) { return 20.0 * std::log10(std::max(x, 1e-9)); }

struct Levels { double rms = 0, peak = 0, ceilFrac = 0; };
Levels measure(const std::vector<int16_t>& pcm) {
  Levels l;
  double sum = 0;
  size_t ceil = 0;
  for (int16_t v : pcm) {
    const double x = std::fabs(static_cast<double>(v)) / 32768.0;
    sum += x * x;
    l.peak = std::max(l.peak, x);
    if (x >= 0.98) ++ceil;
  }
  l.rms = pcm.empty() ? 0 : std::sqrt(sum / static_cast<double>(pcm.size()));
  l.ceilFrac = pcm.empty() ? 0 : static_cast<double>(ceil) / static_cast<double>(pcm.size());
  return l;
}

}  // namespace

int main() {
  const char* e = std::getenv("M0_P0_OUT");
  const std::string out = e ? e : "build/m0a/p0";
  std::filesystem::create_directories(out);

  if (std::getenv("M0_P0_AUDITION")) {
    // Mix audition: A (Loop) + DEVELOP + BREAK, 12 bars, P3, at three fader sets, for identities that pass the structural condition.
    struct Item { const char* genre; uint32_t ordinal; };
    struct Mix { const char* tag; MixSettings m; };
    const Mix mixes[] = {{"mixA_default", {1.0f, 1.0f, 1.0f}}, {"mixB_synth150_drums80", {1.5f, 1.5f, 0.8f}},
                         {"mixC_synth150_drums45", {1.5f, 1.5f, 0.45f}}};
    std::filesystem::create_directories(out + "/audition");
    for (Item it : {Item{"Dub", 0}, Item{"Funk", 0}, Item{"DnB", 6}}) {
      const GenreCase* g = findGenre(it.genre);
      MakeOptions oa; oa.level = kP3; oa.lawOverride = 0;
      MakeOptions od = oa; od.lawOverride = 2;
      MakeOptions ob = oa; ob.lawOverride = 3;
      Phrase a, d, b;
      if (!makePhrase(*g, 4, it.ordinal, oa, a) || !makePhrase(*g, 4, it.ordinal, od, d) || !makePhrase(*g, 4, it.ordinal, ob, b)) continue;
      std::vector<const BarRec*> bars;
      for (const Phrase* ph : {&a, &d, &b}) for (const BarRec& br : ph->bar) bars.push_back(&br);
      for (const Mix& mx : mixes) {
        g_mix = mx.m;
        std::vector<int16_t> pcm; std::string note;
        const bool ok = render(*g, a.suggestedBpm, bars, pcm, note);
        const std::string name = std::string(it.genre) + "_ord" + std::to_string(it.ordinal) + "_" + mx.tag;
        writeWav(out + "/audition/" + name + ".wav", pcm);
        std::printf("AUDITION %-44s %s %s bpm=%.0f %.1fs\n", name.c_str(), a.archetype.c_str(), ok ? "ok" : "FAILED", a.suggestedBpm,
                    static_cast<double>(pcm.size()) / kRenderRate);
      }
      g_mix = MixSettings{};
    }
    return 0;
  }

  // ------------------------------------------------------------------ S1
  std::printf("== P0-S1 STRUCTURAL CORPUS (all sections P3; ordinals 0..7; changed bars out of 4 vs Loop phrase A) ==\n");
  std::ofstream tsv(out + "/p0_structural.tsv");
  tsv << "genre\tordinal\tarchetype\tapplicable\tdev_drums\tdev_bassRhythm\tdev_bassPitch\tdev_chord\t"
         "brk_drums\tbrk_bassRhythm\tbrk_bassPitch\tbrk_chord\tcycle_bass_or_chord\treturn_bar_rhythm_equals_A\n";
  struct Agg { int n = 0, pass = 0, na = 0; std::vector<std::string> failing; };
  std::map<std::string, Agg> byArch;  // key genre/archetype
  std::vector<Phrase> store;
  store.reserve(512);
  for (const char* name : {"Acid", "House", "Techno", "Darksynth", "Dub", "Funk", "UKG", "DnB"}) {
    const GenreCase* g = findGenre(name);
    for (uint32_t o = 0; o < 8; ++o) {
      MakeOptions oa; oa.level = kP3; oa.lawOverride = 0;
      Phrase a;
      if (!makePhrase(*g, 4, o, oa, a)) {
        std::printf("  %-9s ord=%u  Loop phrase unavailable (%s)\n", name, o, a.status.c_str());
        continue;
      }
      MakeOptions od = oa; od.lawOverride = 2;
      MakeOptions ob = oa; ob.lawOverride = 3;
      Phrase d, b;
      const bool okD = makePhrase(*g, 4, o, od, d);
      const bool okB = makePhrase(*g, 4, o, ob, b);
      const std::string key = std::string(name) + "/" + a.archetype;
      Agg& agg = byArch[key];
      if (!okD || !okB) {
        ++agg.na;
        std::printf("  %-9s ord=%u %-18s NOT_APPLICABLE (%s)\n", name, o, a.archetype.c_str(),
                    (okD ? b.status : d.status).c_str());
        tsv << name << '\t' << o << '\t' << a.archetype << "\tNO\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\n";
        continue;
      }
      const SectionDiff sd = diffAgainst(a, d), sb = diffAgainst(a, b);
      const bool cycle = sd.bassOrChord() || sb.bassOrChord();
      // Return bar (4th bar of the BREAK section) vs A's first bar: rhythm restored?
      const BarRec& ret = b.bar[3];
      const bool retRhythm = sameDrums(ret, a.bar[0]) && ret.a.attacks == a.bar[0].a.attacks && ret.b.attacks == a.bar[0].b.attacks;
      ++agg.n;
      if (cycle) ++agg.pass; else agg.failing.push_back("ord" + std::to_string(o));
      std::printf("  %-9s ord=%u %-18s DEVELOP d/b/p/c=%d/%d/%d/%d  BREAK d/b/p/c=%d/%d/%d/%d  cycle bass|chord=%s  returnRhythm=%s\n",
                  name, o, a.archetype.c_str(), sd.drums, sd.bassRhythm, sd.bassPitch, sd.chord, sb.drums, sb.bassRhythm,
                  sb.bassPitch, sb.chord, cycle ? "YES" : "no", retRhythm ? "restored" : "differs");
      tsv << name << '\t' << o << '\t' << a.archetype << "\tYES\t" << sd.drums << '\t' << sd.bassRhythm << '\t' << sd.bassPitch
          << '\t' << sd.chord << '\t' << sb.drums << '\t' << sb.bassRhythm << '\t' << sb.bassPitch << '\t' << sb.chord << '\t'
          << (cycle ? "YES" : "NO") << '\t' << (retRhythm ? "YES" : "NO") << '\n';
    }
  }
  std::printf("\n  Per archetype (identities that reach it among ordinals 0..7):\n");
  for (auto& kv : byArch) {
    const Agg& a = kv.second;
    std::string fails;
    for (auto& f : a.failing) fails += " " + f;
    std::printf("    %-28s identities=%d applicable=%d structural(bass|chord)=%d/%d %s%s\n", kv.first.c_str(), a.n + a.na, a.n,
                a.pass, a.n, (a.n > 0 && a.pass == a.n) ? "PASSES 8/8 rule" : (a.n == 0 ? "NOT ADMITTED" : "FAILS"),
                fails.empty() ? "" : (" (drum-only:" + fails + ")").c_str());
  }

  // ------------------------------------------------------------------ S2
  std::printf("\n== P0-S2 DEPTH: Loop phrase at P2 vs P3, same identity (ordinals 0..7) ==\n");
  for (const char* name : {"Techno", "Dub", "Funk", "UKG", "DnB"}) {
    const GenreCase* g = findGenre(name);
    int same = 0, total = 0, drumDiff = 0, bassDiff = 0, chordDiff = 0;
    for (uint32_t o = 0; o < 8; ++o) {
      MakeOptions o2; o2.level = kP2; o2.lawOverride = 0;
      MakeOptions o3; o3.level = kP3; o3.lawOverride = 0;
      Phrase p2, p3;
      if (!makePhrase(*g, 4, o, o2, p2) || !makePhrase(*g, 4, o, o3, p3)) continue;
      ++total;
      bool all = true;
      for (size_t i = 0; i < 4; ++i) {
        const bool dd = !sameDrums(p2.bar[i], p3.bar[i]);
        const bool bb = !sameLane(p2.bar[i].a, p3.bar[i].a);
        const bool cc = !sameLane(p2.bar[i].b, p3.bar[i].b);
        drumDiff += dd; bassDiff += bb; chordDiff += cc;
        all = all && !dd && !bb && !cc;
      }
      same += all;
    }
    std::printf("  %-7s identical phrases %d/%d; differing bars over all identities: drums=%d bass=%d chord=%d\n", name, same, total,
                drumDiff, bassDiff, chordDiff);
  }

  // ------------------------------------------------------------------ S3
  std::printf("\n== P0-S3 MIX VARIANTS (Loop phrase A, 8 bars, engine track faders; diagnostic, not a target) ==\n");
  std::ofstream mtsv(out + "/p0_mix.tsv");
  mtsv << "genre\tsynthA\tsynthB\tdrums\tsynth_rms_db\tdrums_rms_db\tsynth_minus_drums_db\tfull_peak\tfull_crest_db\tfull_time_at_ceiling_pct\n";
  struct Variant { float sa, sb, dr; };
  const Variant variants[] = {{1.0f, 1.0f, 1.0f}, {1.5f, 1.5f, 1.0f}, {1.5f, 1.5f, 0.8f}, {1.5f, 1.5f, 0.6f},
                              {1.5f, 1.5f, 0.45f}, {1.25f, 1.25f, 0.6f}};
  for (const char* name : {"Techno", "Dub", "Funk"}) {
    const GenreCase* g = findGenre(name);
    int ord = bestOrdinal(*g, 4, kP3);
    if (ord < 0) ord = 0;
    MakeOptions oa; oa.level = kP3; oa.lawOverride = 0;
    Phrase a;
    if (!makePhrase(*g, 4, static_cast<uint32_t>(ord), oa, a)) continue;
    for (const Variant& v : variants) {
      g_mix = MixSettings{v.sa, v.sb, v.dr};
      auto run = [&](bool synth, bool drums) {
        Phrase c = a;
        for (BarRec& b : c.bar) {
          if (!synth) { b.synthA = SynthPattern{}; b.synthB = SynthPattern{}; }
          if (!drums) b.drums = DrumPatternSet{};
        }
        std::vector<const BarRec*> bars;
        for (int rep = 0; rep < 2; ++rep) for (BarRec& b : c.bar) bars.push_back(&b);
        std::vector<int16_t> pcm; std::string note;
        render(*g, a.suggestedBpm, bars, pcm, note);
        return measure(pcm);
      };
      const Levels full = run(true, true), synth = run(true, false), drums = run(false, true);
      const double diff = db(synth.rms) - db(drums.rms);
      std::printf("  %-6s ord=%d synth %.2f/%.2f drums %.2f : synth %.1f dB, drums %.1f dB, synth-drums %+.1f dB | full peak %.2f crest %.1f dB at-ceiling %.3f%%\n",
                  name, ord, v.sa, v.sb, v.dr, db(synth.rms), db(drums.rms), diff, full.peak, db(full.peak) - db(full.rms),
                  full.ceilFrac * 100.0);
      mtsv << name << '\t' << v.sa << '\t' << v.sb << '\t' << v.dr << '\t' << db(synth.rms) << '\t' << db(drums.rms) << '\t' << diff
           << '\t' << full.peak << '\t' << (db(full.peak) - db(full.rms)) << '\t' << full.ceilFrac * 100.0 << '\n';
    }
    g_mix = MixSettings{};
  }
  std::printf("\nP0 preparation corpus: done (%s)\n", out.c_str());
  return 0;
}
