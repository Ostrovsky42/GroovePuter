// GroovePuter 0.9.14 M0-A -- audio renders of the listening scenarios.
//
// Host-only. Uses the REAL GroovePuter engine (MiniAcid::generateAudioBuffer, Song
// playback, the genre's groovebox mode) to render mono 16-bit / 44.1 kHz WAV files
// of TAKE -> KEEP -> DEVELOP -> BREAK -> RETURN style sequences built from
// production phrase owners. NOT product code, NOT linked into firmware.
//
// HONEST FRAMING (also written into the manifest): KEEP / DEVELOP / RETURN are not
// product actions today. The sequences are stitched from separately generated
// 4-bar phrases that share genre + recipe + generation identity, so bar 0 of every
// section is the SAME idea. What each section is:
//   TAKE     phrase law Loop (the idea)                           <- production
//   KEEP     the same idea again (KEEP has no sound of its own)   <- nothing exists
//   DEVELOP  law DevelopReturn at depth P3 (Statement Build RepeatGhosts Turnaround)
//   BREAK    law SparseDrift at depth P3 (Statement RepeatGhosts Break Return)
//   RETURN   the kept idea back, exactly                          <- IDEAL CEILING;
//            production only returns rhythm (see the Return bar at the end of BREAK)

#define M0A_NO_MAIN
#include "m0a_corpus.cpp"

namespace {

// The firmware/SDL production sample rate (audio_config.h), NOT the corpus tool's constant.
constexpr uint32_t kRenderRate = ::kSampleRate;

// Engine track-fader levels applied to every render (defaults = engine defaults).
struct MixSettings { float synthA = 1.0f, synthB = 1.0f, drums = 1.0f; };
MixSettings g_mix;

struct Section {
  std::string label;
  std::string what;
  const Phrase* phrase;  // section is all bars of this phrase
};

struct RenderJob {
  std::string file;      // base name (no extension)
  std::string genre;
  std::string title;
  std::vector<Section> sections;
};

void writeWav(const std::string& path, const std::vector<int16_t>& pcm) {
  std::ofstream f(path, std::ios::binary);
  auto w32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
  auto w16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
  const uint32_t bytes = static_cast<uint32_t>(pcm.size() * 2);
  f.write("RIFF", 4); w32(36 + bytes); f.write("WAVE", 4);
  f.write("fmt ", 4); w32(16); w16(1); w16(1); w32(kRenderRate); w32(kRenderRate * 2); w16(2); w16(16);
  f.write("data", 4); w32(bytes);
  f.write(reinterpret_cast<const char*>(pcm.data()), bytes);
}

double rmsOf(const std::vector<int16_t>& pcm, size_t a, size_t b) {
  if (b > pcm.size()) b = pcm.size();
  if (a >= b) return 0;
  double t = 0;
  for (size_t i = a; i < b; ++i) t += static_cast<double>(pcm[i]) * pcm[i];
  return std::sqrt(t / static_cast<double>(b - a)) / 32768.0;
}

// Renders `bars` (concatenated) through the real engine in Song mode.
bool render(const GenreCase& g, float bpm, const std::vector<const BarRec*>& bars,
            std::vector<int16_t>& pcm, std::string& note) {
  MiniAcid engine(static_cast<float>(kRenderRate), nullptr);
  engine.init();
  configure(engine, g);
  engine.setGrooveboxMode(GenreManager::grooveboxModeForRecipe(g.recipe, g.mode));
  Scene& scene = engine.sceneManager().currentScene();
  Song song{};
  const int perBank = Bank<SynthPattern>::kPatterns;
  // Identical bars (same BarRec object, e.g. TAKE / KEEP / RETURN) share one pattern
  // slot, so the Song revisits the very same material -- exactly what "keep" means.
  std::vector<const BarRec*> slots;
  for (size_t i = 0; i < bars.size(); ++i) {
    size_t slot = 0;
    for (; slot < slots.size(); ++slot) if (slots[slot] == bars[i]) break;
    if (slot == slots.size()) slots.push_back(bars[i]);
    const int bank = static_cast<int>(slot) / perBank;
    const int index = static_cast<int>(slot) % perBank;
    if (bank >= kBankCount) { note = "more distinct bars than one page holds"; return false; }
    scene.synthABanks[bank].patterns[index] = bars[i]->synthA;
    scene.synthBBanks[bank].patterns[index] = bars[i]->synthB;
    scene.drumBanks[bank].patterns[index] = bars[i]->drums;
    const int16_t global = static_cast<int16_t>(songPatternFromPageBankIndex(0, bank, index));
    SongPosition& pos = song.positions[i];
    pos.patterns[static_cast<int>(SongTrack::SynthA)] = global;
    pos.patterns[static_cast<int>(SongTrack::SynthB)] = global;
    pos.patterns[static_cast<int>(SongTrack::Drums)] = global;
  }
  song.length = static_cast<int>(bars.size());
  scene.songs[0] = song;
  scene.activeSongSlot = 0;
  scene.feel.patternBars = 1;
  // Production rebuilds the pattern runtime event bank whenever pattern material changes;
  // without this the synth lanes keep playing stale (empty) events.
  const bool bankOk = engine.rebuildPatternRuntimeEventBank();
  if (!bankOk) { note = "event bank rebuild failed"; return false; }
  if (const char* trim = std::getenv("M0_DRUM_TRIM")) g_mix.drums = static_cast<float>(std::atof(trim));
  engine.setTrackVolume(VoiceId::SynthA, g_mix.synthA);
  engine.setTrackVolume(VoiceId::SynthB, g_mix.synthB);
  for (int id = static_cast<int>(VoiceId::DrumKick); id < static_cast<int>(VoiceId::Count); ++id)
    engine.setTrackVolume(static_cast<VoiceId>(id), g_mix.drums);
  engine.setBpm(bpm);
  engine.setSongMode(true);
  engine.setSongPlaybackSlot(0);
  engine.setSongPosition(0);
  engine.start();

  const size_t samplesPerBar = static_cast<size_t>(std::llround(4.0 * 60.0 / bpm * kRenderRate));
  const size_t total = samplesPerBar * bars.size() + static_cast<size_t>(kRenderRate * 1.5f);  // decay tail
  pcm.assign(total, 0);
  constexpr size_t kBlock = 128;
  int lastRow = -1, maxRow = 0;
  for (size_t at = 0; at < total; at += kBlock) {
    const size_t n = std::min(kBlock, total - at);
    engine.generateAudioBuffer(pcm.data() + at, n);
    const int row = engine.currentSongPosition();
    if (row != lastRow) { lastRow = row; maxRow = std::max(maxRow, row); }
  }
  engine.stop();
  note = "song rows reached: " + std::to_string(maxRow + 1) + "/" + std::to_string(bars.size());
  return maxRow + 1 >= static_cast<int>(bars.size()) - 1;
}

// Front-panel `D` ("DEVELOP") = Revoice with the UI's default request: every note +12
// semitones, folded (>84 => -24, <24 => +24), exactly as transformRevoice does.
BarRec revoiced(const BarRec& in) {
  BarRec out = in;
  for (SynthStep& st : out.synthA.steps) {
    if (st.note < 0) continue;
    int n = st.note + 12;
    if (n > 84) n -= 24;
    if (n < 24) n += 24;
    st.note = static_cast<int8_t>(n);
  }
  return out;
}

std::string fmt(double v) {
  char b[32];
  std::snprintf(b, sizeof(b), "%.2f", v);
  return b;
}

}  // namespace

#ifndef M0A_RENDER_NO_MAIN
int main() {
  const char* e = std::getenv("M0_AUDIO_OUT");
  const std::string out = e ? e : "build/m0a/audio";
  std::filesystem::create_directories(out);
  if (std::getenv("M0_DIAG")) {
    // Diagnostic: does each lane actually reach the audio? Renders one Techno idea with lanes removed.
    const GenreCase* g = findGenre("Techno");
    Phrase idea;
    MakeOptions o; o.level = R::RealizationLevel::P3Transformation; o.lawOverride = 0;
    makePhrase(*g, 4, static_cast<uint32_t>(bestOrdinal(*g, 4, R::RealizationLevel::P3Transformation)), o, idea);
    {
      MiniAcid probe(static_cast<float>(kRenderRate), nullptr);
      probe.init();
      std::printf("DIAG default track volumes: SynthA=%.2f SynthB=%.2f Kick=%.2f Snare=%.2f HatC=%.2f HatO=%.2f Clap=%.2f main=%.2f\n",
                  probe.getTrackVolume(VoiceId::SynthA), probe.getTrackVolume(VoiceId::SynthB),
                  probe.getTrackVolume(VoiceId::DrumKick), probe.getTrackVolume(VoiceId::DrumSnare),
                  probe.getTrackVolume(VoiceId::DrumHatC), probe.getTrackVolume(VoiceId::DrumHatO),
                  probe.getTrackVolume(VoiceId::DrumClap), probe.mainVolume());
    }
    struct V { const char* name; bool a, b, d; };
    for (V v : {V{"full", true, true, true}, V{"noSynthA", false, true, true}, V{"noSynthB", true, false, true},
                V{"noDrums", true, true, false}, V{"drumsOnly", false, false, true}, V{"synthAOnly", true, false, false}}) {
      Phrase c = idea;
      for (BarRec& b : c.bar) {
        if (!v.a) b.synthA = SynthPattern{};
        if (!v.b) b.synthB = SynthPattern{};
        if (!v.d) b.drums = DrumPatternSet{};
      }
      std::vector<const BarRec*> bars;
      for (int rep = 0; rep < 2; ++rep) for (BarRec& b : c.bar) bars.push_back(&b);
      std::vector<int16_t> pcm; std::string note;
      render(*g, idea.suggestedBpm, bars, pcm, note);
      writeWav(out + "/diag_" + v.name + ".wav", pcm);
      std::printf("DIAG %-10s rms=%.4f  %s\n", v.name, rmsOf(pcm, 0, pcm.size()), note.c_str());
    }
    // Octave test: Synth A alone, before vs after D (Revoice). A real +1 octave moves energy up a band.
    for (int variant = 0; variant < 2; ++variant) {
      Phrase c = idea;
      for (BarRec& b : c.bar) {
        b.synthB = SynthPattern{}; b.drums = DrumPatternSet{};
        if (variant == 1) b = [&] { BarRec r = revoiced(b); return r; }();
      }
      std::vector<const BarRec*> bars;
      for (int rep = 0; rep < 2; ++rep) for (BarRec& b : c.bar) bars.push_back(&b);
      std::vector<int16_t> pcm; std::string note;
      render(*g, idea.suggestedBpm, bars, pcm, note);
      writeWav(out + (variant ? "/diag_synthA_afterD.wav" : "/diag_synthA_beforeD.wav"), pcm);
      int lowNote = 127, highNote = 0;
      for (const BarRec& b : c.bar) for (const SynthStep& st : b.synthA.steps) if (st.note >= 0) { lowNote = std::min<int>(lowNote, st.note); highNote = std::max<int>(highNote, st.note); }
      std::printf("DIAG synthA %s: notes %d..%d rms=%.4f\n", variant ? "afterD " : "beforeD", lowNote, highNote, rmsOf(pcm, 0, pcm.size()));
    }
    return 0;
  }
  std::ofstream manifest(out + "/manifest.tsv");
  manifest << "file\tgenre\tarchetype\tbpm\tordinal\ttitle\tcue_table(section|start_s|end_s|law|bar_functions)\tsound_check\n";

  const R::RealizationLevel P3 = R::RealizationLevel::P3Transformation;
  std::vector<Phrase> keep;  // stable storage
  keep.reserve(64);
  auto mk = [&](const GenreCase& g, uint8_t bars, uint32_t ord, R::RealizationLevel lvl, int law) -> const Phrase* {
    MakeOptions o;
    o.level = lvl;
    o.lawOverride = law;
    Phrase p;
    if (!makePhrase(g, bars, ord, o, p)) return nullptr;
    keep.push_back(p);
    return &keep.back();
  };

  std::vector<RenderJob> jobs;
  std::vector<std::string> gaps;
  for (const char* name : {"Acid", "Techno", "Dub", "UKG", "DnB"}) {
    const GenreCase* g = findGenre(name);
    const int ord = bestOrdinal(*g, 4, P3);
    const uint32_t ordinal = ord >= 0 ? static_cast<uint32_t>(ord) : 0u;
    const Phrase* idea = mk(*g, 4, ordinal, P3, 0);
    if (!idea) { gaps.push_back(std::string(name) + ": Loop idea unavailable"); continue; }
    RenderJob seq;
    seq.file = std::string(name) + "_1_sequence";
    seq.genre = name;
    seq.title = "TAKE -> KEEP -> DEVELOP -> BREAK -> RETURN";
    seq.sections.push_back({"TAKE", "the idea (phrase law Loop)", idea});
    seq.sections.push_back({"KEEP", "the same idea again - KEEP has no sound of its own", idea});
    const Phrase* dev = ord >= 0 ? mk(*g, 4, ordinal, P3, 2) : nullptr;
    const Phrase* brk = ord >= 0 ? mk(*g, 4, ordinal, P3, 3) : nullptr;
    if (dev && brk) {
      seq.sections.push_back({"DEVELOP", "law DevelopReturn @P3: Statement, Build, RepeatGhosts, Turnaround", dev});
      seq.sections.push_back({"BREAK", "law SparseDrift @P3: Statement, RepeatGhosts, Break, Return (last bar = production RETURN)", brk});
      seq.sections.push_back({"RETURN", "the kept idea back, exactly (IDEAL; production does not do this)", idea});
    } else {
      gaps.push_back(std::string(name) + ": DEVELOP/BREAK/RETURN not producible (archetype not admitted to phrase evolution)");
    }
    jobs.push_back(seq);

    // What the real 'D' key does: same idea, then the octave-shifted version.
    RenderJob dk;
    dk.file = std::string(name) + "_2_Dkey";
    dk.genre = name;
    dk.title = "what the DEVELOP key (D = Revoice) actually does: idea, then the same bars +1 octave";
    keep.push_back(*idea);
    Phrase& shifted = keep.back();
    for (BarRec& b : shifted.bar) b = revoiced(b);
    dk.sections.push_back({"BEFORE", "the idea", idea});
    dk.sections.push_back({"AFTER D", "after pressing D (Revoice, +1 octave, folded)", &shifted});
    jobs.push_back(dk);
  }

  // One-minute demonstration for Techno: leave it looping vs press TAKE every phrase.
  {
    const GenreCase* g = findGenre("Techno");
    const Phrase* first = mk(*g, 4, kOrdinal, R::RealizationLevel::P2Variation, -1);
    if (first) {
      const double barSec = 4.0 * 60.0 / first->suggestedBpm;
      const int units = std::max(1, static_cast<int>(std::floor(60.0 / (barSec * 4.0) + 0.5)));  // ~60 s
      RenderJob loop, takes;
      loop.file = "Techno_3_oneminute_loop"; loop.genre = "Techno";
      loop.title = "~1 minute: one 4-bar phrase left looping (natural production law)";
      takes.file = "Techno_4_oneminute_takes"; takes.genre = "Techno";
      takes.title = "~1 minute: press TAKE at every phrase boundary (each 4-bar phrase is a fresh take)";
      for (int i = 0; i < units; ++i) {
        loop.sections.push_back({"LOOP " + std::to_string(i + 1), "same phrase", first});
        const Phrase* t = mk(*g, 4, kOrdinal + 1 + i, R::RealizationLevel::P2Variation, -1);
        if (t) takes.sections.push_back({"TAKE " + std::to_string(i + 1), std::string("law ") + lawName(t->law), t});
      }
      jobs.push_back(loop);
      jobs.push_back(takes);
    }
  }

  int failures = 0;
  for (const RenderJob& job : jobs) {
    const GenreCase* g = findGenre(job.genre);
    if (job.sections.empty()) continue;
    const Phrase* firstPhrase = job.sections[0].phrase;
    const float bpm = firstPhrase->suggestedBpm;
    std::vector<const BarRec*> bars;
    std::vector<size_t> sectionStart;
    for (const Section& s : job.sections) {
      sectionStart.push_back(bars.size());
      for (const BarRec& b : s.phrase->bar) bars.push_back(&b);
    }
    sectionStart.push_back(bars.size());
    const double barSec = 4.0 * 60.0 / bpm;
    const size_t spb = static_cast<size_t>(std::llround(barSec * kRenderRate));
    // One page holds 16 distinct bars. Longer takes-style jobs are rendered in groups of
    // whole bars (fresh engine per group) and concatenated at exact bar boundaries.
    std::vector<int16_t> pcm;
    std::string note;
    bool ok = true;
    {
      size_t begin = 0;
      const size_t kMaxDistinct = 16;
      while (begin < bars.size()) {
        std::vector<const BarRec*> distinct;
        size_t end = begin;
        while (end < bars.size()) {
          if (std::find(distinct.begin(), distinct.end(), bars[end]) == distinct.end()) {
            if (distinct.size() == kMaxDistinct) break;
            distinct.push_back(bars[end]);
          }
          ++end;
        }
        std::vector<const BarRec*> group(bars.begin() + begin, bars.begin() + end);
        std::vector<int16_t> part;
        std::string partNote;
        ok = render(*g, bpm, group, part, partNote) && ok;
        note += (note.empty() ? "" : " + ") + partNote;
        if (end < bars.size()) part.resize(group.size() * spb);  // cut at the bar line between groups
        pcm.insert(pcm.end(), part.begin(), part.end());
        begin = end;
      }
    }

    // Sound check: RMS per section proves the render is not silent / misaligned.
    std::string check = note;
    bool silent = false;
    for (size_t i = 0; i < job.sections.size(); ++i) {
      const double r = rmsOf(pcm, sectionStart[i] * spb, sectionStart[i + 1] * spb);
      check += " | " + job.sections[i].label + " rms=" + fmt(r);
      if (r < 0.005) silent = true;
    }
    if (!ok || silent) { ++failures; check += "  ** SOUND CHECK FAILED **"; }

    writeWav(out + "/" + job.file + ".wav", pcm);
    std::string cues;
    for (size_t i = 0; i < job.sections.size(); ++i) {
      const Phrase& p = *job.sections[i].phrase;
      std::string fns;
      for (const BarRec& b : p.bar) { fns += barFnName(b.fn); fns += '/'; }
      cues += job.sections[i].label + "|" + fmt(sectionStart[i] * barSec) + "|" + fmt(sectionStart[i + 1] * barSec) +
              "|" + lawName(p.law) + "|" + fns + "|" + job.sections[i].what + " ;; ";
    }
    manifest << job.file << '\t' << job.genre << '\t' << firstPhrase->archetype << '\t' << fmt(bpm) << '\t'
             << "-" << '\t' << job.title << '\t' << cues << '\t' << check << '\n';
    std::printf("%-28s %-6s %-16s bpm=%.0f %.1fs  %s\n", job.file.c_str(), job.genre.c_str(),
                firstPhrase->archetype.c_str(), bpm, static_cast<double>(pcm.size()) / kRenderRate, check.c_str());
  }
  for (const std::string& gap : gaps) {
    manifest << "GAP\t-\t-\t-\t-\t" << gap << "\t-\t-\n";
    std::printf("GAP: %s\n", gap.c_str());
  }
  std::printf("render failures: %d\n", failures);
  return failures == 0 ? 0 : 1;
}
#endif  // M0A_RENDER_NO_MAIN
