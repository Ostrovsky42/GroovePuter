// GroovePuter 0.9.14 M0-A -- MUSICAL PLAY BASELINE (host-side measurement tool).
//
// Deterministic, timbre-free structural corpus + descriptive reports. This is
// NOT a runtime Genre classifier and NOT product code: it only observes what
// production generation already materialized. Nothing here is linked into the
// firmware.
//
// Output: text report on stdout; corpus TSV, SMF listening files, manifest and
// listening cards under $M0_OUT (default build/m0a).

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/dsp/generated_phrase_song.h"
#include "src/generation/rhythm/bar_evolution.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/state/generation_request_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

SerialMock Serial;
SDMock SD;

namespace {

namespace R = GroovePuterRhythm;
constexpr float kSampleRate = 44100.0f;
constexpr uint32_t kOrdinal = 11;  // fixed generation identity for the corpus
int g_failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("SELF-CHECK FAILED: %s\n", what);
    ++g_failures;
  }
}

int pop16(uint32_t v) { return __builtin_popcount(v & 0xFFFFu); }

// ------------------------------------------------------------------ data model
struct LaneRec {
  uint16_t attacks = 0, cont = 0, accent = 0, slide = 0, ghost = 0;
  int nonZeroTiming = 0;
  int regMedian = -1;  // median octave (MIDI note / 12) of attacks
  int noteAt[16];      // MIDI note per step, -1 when none
  LaneRec() { for (int& n : noteAt) n = -1; }
};

struct BarRec {
  uint16_t drum[8]{};
  uint16_t drumAccent[8]{};
  int drumTimingNonZero = 0;
  LaneRec a, b;
  uint16_t harm = 0;
  uint8_t harmEvents = 0;
  R::BarFunction fn = R::BarFunction::Statement;
  R::BassRhythmId bassId = R::BassRhythmId::Auto;
  int automationNodes = 0;
  DrumPatternSet drums;
  SynthPattern synthA, synthB;
};

struct GenreCase {
  const char* name;
  GenerativeMode mode;
  GenreRecipeId recipe;
};

struct Phrase {
  std::string genre;
  uint8_t bars = 0;
  bool p1r = false;
  bool ok = false;
  std::string status;
  std::string archetype;
  R::PhraseEvolutionLawId law = R::PhraseEvolutionLawId::Loop;
  R::TrajectoryId trajectory = R::kNoTrajectoryId;
  uint8_t rootPc = 0;
  float bpm = 120.0f;
  float suggestedBpm = 120.0f;  // midpoint of the archetype's catalogued tempo range
  std::vector<BarRec> bar;
};

const GenreCase kGenres[] = {
    {"Acid", GenerativeMode::Acid, 6},
    {"AcidRolling", GenerativeMode::Acid, 7},
    {"Techno", GenerativeMode::Techno, 0},
    {"Darksynth", GenerativeMode::Darksynth, 0},
    {"House", GenerativeMode::House, 0},
    {"Dub", GenerativeMode::Reggae, 5},
    {"DeepChord", GenerativeMode::Reggae, 10},
    {"Funk", GenerativeMode::FunkSoul, 0},
    {"UKG", GenerativeMode::UkGarage, 0},
    {"DnB", GenerativeMode::DrumAndBass, 0},
    {"BrokenDnB", GenerativeMode::Broken, 2},
    {"Electro", GenerativeMode::Electro, 0},
};

const GenreCase* findGenre(const std::string& name) {
  for (const auto& g : kGenres) if (name == g.name) return &g;
  return nullptr;
}

const char* barFnName(R::BarFunction f) {
  switch (f) {
    case R::BarFunction::Statement: return "Statement";
    case R::BarFunction::Repeat: return "Repeat";
    case R::BarFunction::RepeatWithGhosts: return "RepeatGhosts";
    case R::BarFunction::Response: return "Response";
    case R::BarFunction::Reduction: return "Reduction";
    case R::BarFunction::Build: return "Build";
    case R::BarFunction::Turnaround: return "Turnaround";
    case R::BarFunction::Break: return "Break";
    case R::BarFunction::Return: return "Return";
    default: return "?";
  }
}

const char* lawName(R::PhraseEvolutionLawId l) {
  switch (l) {
    case R::PhraseEvolutionLawId::Loop: return "Loop";
    case R::PhraseEvolutionLawId::RepeatReply: return "RepeatReply";
    case R::PhraseEvolutionLawId::DevelopReturn: return "DevelopReturn";
    case R::PhraseEvolutionLawId::SparseDrift: return "SparseDrift";
    default: return "?";
  }
}

const char* levelName(R::RealizationLevel l) {
  switch (l) {
    case R::RealizationLevel::P1Canonical: return "P1";
    case R::RealizationLevel::P2Variation: return "P2";
    case R::RealizationLevel::P3Transformation: return "P3";
    default: return "?";
  }
}

const char* kDrumLane[8] = {"KICK", "SNARE", "CHAT", "OHAT", "MTOM", "HTOM", "RIM", "CLAP"};

void configure(MiniAcid& engine, const GenreCase& g) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(g.mode);
  scene.genre.recipe = g.recipe;
  scene.genre.morphTarget = 0;
  scene.genre.morphAmount = 0;
  scene.genre.regenerateOnApply = false;
  scene.genre.applyTempoOnApply = false;
  scene.activeSongSlot = 0;
  scene.songs[0] = Song{};
  scene.feel.patternBars = 1;
  engine.genreManager().setGenerativeMode(g.mode);
  engine.genreManager().setRecipe(g.recipe);
}

int medianOf(std::vector<int> v) {
  if (v.empty()) return -1;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

// Neutral (timbre-free) lane record. Attack/continuation for Synth A come from
// the owner's exact BassRhythmPlan; for Synth B they are read from the adapter's
// physical representation (a slid repeat of the previous note).
LaneRec laneFrom(const SynthPattern& p, bool haveOwnerMasks, uint16_t ownerAttacks,
                 uint16_t ownerCont) {
  LaneRec l;
  std::vector<int> attackNotes;
  for (int s = 0; s < 16; ++s) {
    const SynthStep& st = p.steps[s];
    if (st.timing != 0 && st.note >= 0) ++l.nonZeroTiming;
    if (st.note < 0) continue;
    l.noteAt[s] = st.note;
    const uint16_t bit = R::stepBit(static_cast<uint8_t>(s));
    bool isCont;
    if (haveOwnerMasks) {
      isCont = (ownerCont & bit) != 0;
    } else {
      isCont = st.slide && s > 0 && p.steps[s - 1].note == st.note;
    }
    if (isCont) l.cont |= bit; else l.attacks |= bit;
    if (st.accent) l.accent |= bit;
    if (st.slide) l.slide |= bit;
    if (st.ghost) l.ghost |= bit;
    if (!isCont) attackNotes.push_back(st.note / 12);
  }
  (void)ownerAttacks;
  l.regMedian = medianOf(attackNotes);
  return l;
}

// ------------------------------------------------------------ phrase materializer
struct MakeOptions {
  R::RealizationLevel level = R::RealizationLevel::P2Variation;
  int lawOverride = -1;  // -1: natural production selection
  uint16_t manualArchetype = 0;  // 0: natural (Auto); else the user-facing MANUAL rhythm selection
};

// Returns false when the request is not applicable (reason in out.status).
bool makePhrase(const GenreCase& g, uint8_t bars, uint32_t ordinal,
                const MakeOptions& opt, Phrase& out) {
  out = Phrase{};
  out.genre = g.name;
  out.bars = bars;
  MiniAcid engine(kSampleRate, nullptr);
  configure(engine, g);
  if (opt.manualArchetype != 0) {
    Scene& sc = engine.sceneManager().currentScene();
    sc.genre.rhythmSelectionMode = static_cast<uint8_t>(R::RhythmSelectionMode::Manual);
    sc.genre.rhythmArchetypeId = opt.manualArchetype;
  }
  const bool routeP1R = R::selectStrongRhythmRoute(engine.sceneManager().currentScene().genre) !=
                        R::StrongRhythmRoute::Legacy;
  out.p1r = routeP1R;
  GroovePuterState::setGenerationLevel(opt.level);
  GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
  const bool ok = GeneratedPhraseSong::prepareWithGenerationAttempt(
      engine, bars, 0, ordinal, true, prepared);
  GroovePuterState::setGenerationLevel(R::RealizationLevel::P2Variation);
  if (!ok) {
    out.status = routeP1R ? "P1R_LENGTH_POLICY_REJECTION" : "LEGACY_ROUTE";
    return false;
  }
  if (!prepared.useP1RRoute) {
    out.status = "LEGACY_ROUTE";
    return false;
  }
  R::PreparedPhraseExecution exec = prepared.p1rExecution;
  const auto* def = R::ReferenceVocabulary::definitionForId(exec.selection.composition.rhythmArchetypeId);
  out.archetype = def ? def->name : "?";
  out.rootPc = exec.materialization.rootPitchClass;
  out.bpm = engine.bpm();
  if (def) out.suggestedBpm = 0.5f * (def->suggestedBpmMin + def->suggestedBpmMax);

  if (opt.lawOverride >= 0) {
    const auto law = static_cast<R::PhraseEvolutionLawId>(opt.lawOverride);
    R::TrajectoryId requested = R::phraseTrajectoryForLaw(law, opt.level);
    exec.phraseTrajectory = R::kNoTrajectoryId;
    exec.phrasePlan = R::RhythmPhrasePlan{};
    exec.selection.composition.phraseLaw = R::PhraseEvolutionLawId::Loop;
    if (law != R::PhraseEvolutionLawId::Loop) {
      if (requested == R::kNoTrajectoryId || def == nullptr ||
          !R::ReferenceVocabulary::phraseEvolutionEnabled(def->key)) {
        out.status = "NOT_APPLICABLE(archetype not admitted to phrase evolution)";
        return false;
      }
      R::BarEvolutionRequest ev{};
      ev.catalog = &R::ReferenceVocabulary::phraseEvolutionCatalog();
      ev.archetypeId = def->archetypeId;
      ev.phraseBars = exec.length.effectivePhraseBars > R::kMaxPhraseBars
          ? R::kMaxPhraseBars : exec.length.effectivePhraseBars;
      ev.level = opt.level;
      ev.generation = exec.selection.realizationGeneration;
      ev.structuralDensityTarget = exec.selection.structuralDensityTarget;
      ev.requestedTrajectoryId = requested;
      const auto evolved = R::evolveRhythmPhrase(ev);
      if (evolved.status != R::BarEvolutionStatus::Ok || evolved.plan.barCount == 0) {
        out.status = "NOT_APPLICABLE(no eligible trajectory for this bar count/level)";
        return false;
      }
      exec.phraseTrajectory = requested;
      exec.phrasePlan = evolved.plan;
      exec.selection.composition.phraseLaw = law;
    }
  }
  out.law = exec.selection.composition.phraseLaw;
  out.trajectory = exec.phraseTrajectory;

  for (uint8_t b = 0; b < bars; ++b) {
    PhraseGenerator::PhraseBar pb{};
    GeneratedPhraseP1R::MaterializedSynthABarEvidence ev{};
    const int16_t address = static_cast<int16_t>(songPatternFromPageBankIndex(0, 0, b));
    if (!GeneratedPhraseP1R::materializeOneBar(engine, exec, b, address, pb, ev) || !ev.valid) {
      out.status = "MATERIALIZE_FAILED";
      return false;
    }
    BarRec r;
    r.drums = pb.drums;
    r.synthA = pb.synthA;
    r.synthB = pb.synthB;
    for (int v = 0; v < 8; ++v) {
      for (int s = 0; s < 16; ++s) {
        const DrumStep& d = pb.drums.voices[v].steps[s];
        if (!d.hit) continue;
        r.drum[v] |= R::stepBit(static_cast<uint8_t>(s));
        if (d.accent) r.drumAccent[v] |= R::stepBit(static_cast<uint8_t>(s));
        if (d.timing != 0) ++r.drumTimingNonZero;
      }
    }
    r.a = laneFrom(pb.synthA, true, ev.bassRhythm.onsets, ev.bassRhythm.continuations);
    r.b = laneFrom(pb.synthB, false, 0, 0);
    r.bassId = ev.bassRhythm.id;
    const auto& hr = exec.harmonicClock.bars[b].harmonicRhythm;
    r.harm = hr.onsets;
    r.harmEvents = hr.eventCount;
    r.fn = exec.phraseTrajectory != R::kNoTrajectoryId
        ? exec.phrasePlan.bars[b % exec.phrasePlan.barCount].function
        : R::BarFunction::Statement;
    for (int l = 0; l < DrumPatternSet::kMaxLanes; ++l) {
      // automation lanes (reverb/compression/transient/engine switch) = production behavior
      r.automationNodes += pb.drums.lanes[l].nodeCount;
    }
    out.bar.push_back(r);
  }
  out.ok = true;
  out.status = "OK";
  return true;
}

// ------------------------------------------------------------------- metrics
struct Dim {
  double drums = 0, bassRhythm = 0, bassPitch = 0, harmony = 0, articulation = 0, chord = 0;
};

// Root-relative pitch-class histogram of bass attacks over a phrase.
std::vector<double> pcHist(const Phrase& p, uint8_t root) {
  std::vector<double> h(12, 0.0);
  double n = 0;
  for (const auto& b : p.bar) {
    for (int s = 0; s < 16; ++s) {
      if (b.a.attacks & R::stepBit(static_cast<uint8_t>(s)) && b.a.noteAt[s] >= 0) {
        h[(b.a.noteAt[s] % 12 + 12 - root) % 12] += 1.0;
        n += 1.0;
      }
    }
  }
  if (n > 0) for (double& x : h) x /= n;
  return h;
}


double drumHits(const BarRec& b) { double t = 0; for (int v = 0; v < 8; ++v) t += pop16(b.drum[v]); return t; }
double bassAtt(const BarRec& b) { return pop16(b.a.attacks); }
double chordAtt(const BarRec& b) { return pop16(b.b.attacks); }
// slide INTO an attack (real articulation); continuation steps carry slide as representation only
double slideRate(const BarRec& b) { return pop16(b.a.slide & b.a.attacks) + pop16(b.b.slide & b.b.attacks); }
double accentRate(const BarRec& b) {
  double t = pop16(b.a.accent) + pop16(b.b.accent);
  for (int v = 0; v < 8; ++v) t += pop16(b.drumAccent[v]);
  return t;
}
double offbeatRatio(const BarRec& b) {  // share of drum+bass attacks on odd 16ths
  double odd = 0, all = 0;
  for (int s = 0; s < 16; ++s) {
    const uint16_t bit = R::stepBit(static_cast<uint8_t>(s));
    int n = (b.a.attacks & bit) ? 1 : 0;
    for (int v = 0; v < 8; ++v) if (b.drum[v] & bit) ++n;
    all += n;
    if (s & 1) odd += n;
  }
  return all > 0 ? odd / all : 0.0;
}

struct Signature {
  double laneHits[8]{};
  double bassAttacks = 0, chordAttacks = 0, slide = 0, accent = 0, harmEvents = 0, offbeat = 0;
  std::vector<double> pc;
  double bassKickOverlap = 0;  // bass attacks coinciding with a kick, per bar
};

Signature signatureOf(const Phrase& p) {
  Signature s;
  if (p.bar.empty()) return s;
  const double n = static_cast<double>(p.bar.size());
  for (const auto& b : p.bar) {
    for (int v = 0; v < 8; ++v) s.laneHits[v] += pop16(b.drum[v]) / n;
    s.bassAttacks += bassAtt(b) / n;
    s.chordAttacks += chordAtt(b) / n;
    s.slide += slideRate(b) / n;
    s.accent += accentRate(b) / n;
    s.harmEvents += b.harmEvents / n;
    s.offbeat += offbeatRatio(b) / n;
    s.bassKickOverlap += pop16(b.a.attacks & b.drum[0]) / n;
  }
  s.pc = pcHist(p, p.rootPc);
  return s;
}

struct PairVerdict {
  bool drums, bassRhythm, bassPitch, harmony, articulation, chord;
  double dDrums, dBass, dPc, dHarm, dArt, dChord;
  int diffCount;
  const char* label;
};

PairVerdict comparePhrases(const Phrase& x, const Phrase& y, bool stripArticulation) {
  const Signature a = signatureOf(x), b = signatureOf(y);
  PairVerdict v{};
  for (int i = 0; i < 8; ++i) v.dDrums += std::fabs(a.laneHits[i] - b.laneHits[i]);
  v.dBass = std::fabs(a.bassAttacks - b.bassAttacks) + 8.0 * std::fabs(a.offbeat - b.offbeat) * 0.25;
  for (int i = 0; i < 12; ++i) v.dPc += std::fabs(a.pc[i] - b.pc[i]);
  v.dHarm = std::fabs(a.harmEvents - b.harmEvents);
  v.dArt = stripArticulation ? 0.0 : std::fabs(a.slide - b.slide) + std::fabs(a.accent - b.accent);
  v.dChord = std::fabs(a.chordAttacks - b.chordAttacks);
  v.drums = v.dDrums >= 4.0;
  v.bassRhythm = v.dBass >= 2.0;
  v.bassPitch = v.dPc >= 0.5;
  v.harmony = v.dHarm >= 0.5;
  v.articulation = v.dArt >= 2.0;
  v.chord = v.dChord >= 2.0;
  v.diffCount = v.drums + v.bassRhythm + v.bassPitch + v.harmony + v.articulation + v.chord;
  v.label = v.diffCount >= 2 ? "STRUCTURAL DIFFERENCE OBSERVED"
          : v.diffCount == 0 ? "MOSTLY SHARED STRUCTURE" : "INCONCLUSIVE";
  return v;
}

// ------------------------------------------------------------- temporal analysis
struct Feature {
  uint16_t lanes[8 + 2];  // drum lanes + bass attacks + chord attacks
  uint16_t art;
  uint16_t harm;
  int reg;
};

Feature featureOf(const BarRec& b) {
  Feature f{};
  for (int v = 0; v < 8; ++v) f.lanes[v] = b.drum[v];
  f.lanes[8] = b.a.attacks;
  f.lanes[9] = b.b.attacks;
  f.art = static_cast<uint16_t>((b.a.slide & b.a.attacks) ^ b.a.accent ^ (b.b.slide & b.b.attacks) ^ b.b.accent);
  f.harm = b.harm;
  f.reg = b.a.regMedian;
  return f;
}

int featureDistance(const Feature& x, const Feature& y) {
  int d = 0;
  for (int i = 0; i < 10; ++i) d += pop16(x.lanes[i] ^ y.lanes[i]);
  return d;
}

// Mean per-bar distance between two windows of `span` bars starting at s0/s1.
double windowDistance(const std::vector<Feature>& f, size_t s0, size_t s1, size_t span) {
  if (s1 + span > f.size() || s0 + span > f.size()) return -1;
  double t = 0;
  for (size_t i = 0; i < span; ++i) t += featureDistance(f[s0 + i], f[s1 + i]);
  return t / static_cast<double>(span);
}

// Change between consecutive windows of `span` bars, averaged (or -1 if n/a).
double levelChange(const std::vector<Feature>& f, size_t span) {
  double t = 0;
  int n = 0;
  for (size_t s = 0; s + 2 * span <= f.size(); s += span) {
    t += windowDistance(f, s, s + span, span);
    ++n;
  }
  return n ? t / n : -1.0;
}

void printTemporal(const Phrase& p) {
  std::vector<Feature> f;
  for (const auto& b : p.bar) f.push_back(featureOf(b));
  // event/subdivision: hit<->rest transitions across the 16 steps (all lanes)
  double trans = 0;
  for (const auto& b : p.bar) {
    for (int l = 0; l < 10; ++l) {
      const Feature ft = featureOf(b);
      for (int s = 0; s < 15; ++s) {
        const bool x = ft.lanes[l] & R::stepBit(static_cast<uint8_t>(s));
        const bool y = ft.lanes[l] & R::stepBit(static_cast<uint8_t>(s + 1));
        if (x != y) trans += 1;
      }
    }
  }
  trans /= std::max<size_t>(1, p.bar.size());
  // beat: mean distance between successive 4-step beat groups inside a bar
  double beat = 0;
  int bn = 0;
  for (const auto& b : p.bar) {
    const Feature ft = featureOf(b);
    for (int beatIdx = 0; beatIdx < 3; ++beatIdx) {
      int d = 0;
      for (int l = 0; l < 10; ++l) {
        const int m0 = (ft.lanes[l] >> (12 - 4 * beatIdx)) & 0xF;
        const int m1 = (ft.lanes[l] >> (8 - 4 * beatIdx)) & 0xF;
        d += __builtin_popcount(m0 ^ m1);
      }
      beat += d; ++bn;
    }
  }
  beat = bn ? beat / bn : -1;
  // bar-to-bar detail
  std::printf("    per-bar (vs previous bar): ");
  for (size_t i = 1; i < f.size(); ++i) {
    const int drumD = [&] { int d = 0; for (int l = 0; l < 8; ++l) d += pop16(f[i].lanes[l] ^ f[i - 1].lanes[l]); return d; }();
    const int bassD = pop16(f[i].lanes[8] ^ f[i - 1].lanes[8]);
    const int chordD = pop16(f[i].lanes[9] ^ f[i - 1].lanes[9]);
    const int harmD = pop16(f[i].harm ^ f[i - 1].harm);
    const int artD = pop16(f[i].art ^ f[i - 1].art);
    const int regD = (f[i].reg >= 0 && f[i - 1].reg >= 0) ? std::abs(f[i].reg - f[i - 1].reg) : 0;
    int pcD = 0;
    for (int st = 0; st < 16; ++st)
      if (p.bar[i].a.noteAt[st] % 12 != p.bar[i - 1].a.noteAt[st] % 12) ++pcD;
    std::printf("[b%zu drum%d bass%d bassPc%d chord%d harm%d art%d reg%d] ", i, drumD, bassD, pcD, chordD, harmD, artD, regD);
  }
  std::printf("\n");
  std::printf("    levels (mean changed cells): subdivision-transitions/bar=%.1f beat=%.2f bar=%.2f 2bar=%.2f 4bar=%.2f 8bar=%s\n",
              trans, beat, levelChange(f, 1), levelChange(f, 2), levelChange(f, 4),
              f.size() >= 16 ? "n/a(one window)" : "n/a(phrase is one window)");
  // lane entry/exit across the phrase
  for (int l = 0; l < 10; ++l) {
    int active = 0;
    for (const auto& ft : f) if (ft.lanes[l]) ++active;
    if (active != 0 && active != static_cast<int>(f.size())) {
      std::printf("    lane %s enters/exits (active %d/%zu bars)\n",
                  l < 8 ? kDrumLane[l] : (l == 8 ? "BASS" : "CHORD"), active, f.size());
    }
  }
}

// ---------------------------------------------------------------- SMF writer
void vlq(std::vector<uint8_t>& out, uint32_t v) {
  uint8_t buf[5];
  int n = 0;
  buf[n++] = v & 0x7F;
  while ((v >>= 7) != 0) buf[n++] = static_cast<uint8_t>((v & 0x7F) | 0x80);
  while (n--) out.push_back(buf[n]);
}
void be16(std::vector<uint8_t>& o, uint16_t v) { o.push_back(v >> 8); o.push_back(v & 0xFF); }
void be32(std::vector<uint8_t>& o, uint32_t v) { be16(o, v >> 16); be16(o, v & 0xFFFF); }

struct Ev { uint32_t tick; uint8_t a, b, c; };

std::vector<uint8_t> trackBytes(std::vector<Ev> ev, bool tempo, uint32_t usPerQuarter) {
  std::sort(ev.begin(), ev.end(), [](const Ev& x, const Ev& y) {
    if (x.tick != y.tick) return x.tick < y.tick;
    return (x.a & 0xF0) < (y.a & 0xF0);  // note-off (0x80) before note-on (0x90)
  });
  std::vector<uint8_t> t;
  uint32_t last = 0;
  if (tempo) {
    vlq(t, 0); t.push_back(0xFF); t.push_back(0x51); t.push_back(0x03);
    t.push_back((usPerQuarter >> 16) & 0xFF); t.push_back((usPerQuarter >> 8) & 0xFF); t.push_back(usPerQuarter & 0xFF);
  }
  for (const auto& e : ev) {
    vlq(t, e.tick - last); last = e.tick;
    t.push_back(e.a); t.push_back(e.b); t.push_back(e.c);
  }
  vlq(t, 0); t.push_back(0xFF); t.push_back(0x2F); t.push_back(0x00);
  return t;
}

void addSynth(std::vector<Ev>& out, const SynthPattern& p, uint16_t cont, bool haveCont,
              uint32_t barBase, uint8_t ch) {
  for (int s = 0; s < 16; ++s) {
    const SynthStep& st = p.steps[s];
    if (st.note < 0) continue;
    const uint16_t bit = R::stepBit(static_cast<uint8_t>(s));
    const bool isCont = haveCont ? (cont & bit) != 0
                                 : (st.slide && s > 0 && p.steps[s - 1].note == st.note);
    if (isCont) continue;  // merged into the preceding attack
    int steps = 1;
    while (s + steps < 16) {
      const SynthStep& n = p.steps[s + steps];
      const uint16_t nb = R::stepBit(static_cast<uint8_t>(s + steps));
      const bool nCont = n.note >= 0 && (haveCont ? (cont & nb) != 0
                                                  : (n.slide && n.note == st.note));
      if (!nCont) break;
      ++steps;
    }
    const int64_t start = static_cast<int64_t>(barBase) + s * 24 + st.timing;
    const uint32_t on = static_cast<uint32_t>(std::max<int64_t>(0, start));
    const uint32_t len = static_cast<uint32_t>(steps * 24 - (steps > 1 ? 2 : 8));
    out.push_back({on, static_cast<uint8_t>(0x90 | ch), static_cast<uint8_t>(st.note), static_cast<uint8_t>(st.accent ? 120 : std::min<int>(st.velocity, 110))});
    out.push_back({on + len, static_cast<uint8_t>(0x80 | ch), static_cast<uint8_t>(st.note), 0});
  }
}

bool writeSmf(const std::string& path, const std::vector<const BarRec*>& bars, float bpm) {
  const uint8_t gm[8] = {36, 38, 42, 46, 47, 50, 37, 39};
  std::vector<Ev> drums, bass, chord;
  uint32_t base = 0;
  for (const BarRec* b : bars) {
    for (int v = 0; v < 8; ++v) {
      for (int s = 0; s < 16; ++s) {
        const DrumStep& d = b->drums.voices[v].steps[s];
        if (!d.hit) continue;
        const int64_t start = static_cast<int64_t>(base) + s * 24 + d.timing;
        const uint32_t on = static_cast<uint32_t>(std::max<int64_t>(0, start));
        drums.push_back({on, 0x99, gm[v], static_cast<uint8_t>(d.accent ? 120 : std::min<int>(d.velocity, 105))});
        drums.push_back({on + 6, 0x89, gm[v], 0});
      }
    }
    addSynth(bass, b->synthA, b->a.cont, true, base, 0);
    addSynth(chord, b->synthB, 0, false, base, 1);
    base += 384;
  }
  std::vector<uint8_t> out;
  const char head[] = {'M', 'T', 'h', 'd'};
  out.insert(out.end(), head, head + 4);
  be32(out, 6); be16(out, 1); be16(out, 4); be16(out, 96);
  const uint32_t us = static_cast<uint32_t>(60000000.0 / std::max(30.0f, bpm));
  auto emit = [&](const std::vector<uint8_t>& t) {
    const char m[] = {'M', 'T', 'r', 'k'};
    out.insert(out.end(), m, m + 4);
    be32(out, static_cast<uint32_t>(t.size()));
    out.insert(out.end(), t.begin(), t.end());
  };
  emit(trackBytes({}, true, us));
  emit(trackBytes(drums, false, us));
  emit(trackBytes(bass, false, us));
  emit(trackBytes(chord, false, us));
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
  return f.good();
}

// ------------------------------------------------------------------ corpus TSV
uint64_t fnv(uint64_t h, const std::string& s) {
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  return h;
}

std::string corpusRow(const Phrase& p, int b) {
  const BarRec& r = p.bar[b];
  std::ostringstream o;
  o << p.genre << '\t' << int(p.bars) << '\t' << b << '\t' << barFnName(r.fn) << '\t'
    << R::bassRhythmName(r.bassId) << '\t' << p.bpm << '\t' << int(p.rootPc);
  for (int v = 0; v < 8; ++v) o << '\t' << std::hex << r.drum[v] << std::dec;
  o << '\t' << std::hex << r.a.attacks << '\t' << r.a.cont << '\t' << r.a.accent << '\t' << r.a.slide
    << '\t' << r.b.attacks << '\t' << r.b.cont << '\t' << r.harm << std::dec << '\t' << int(r.harmEvents) << '\t';
  for (int s = 0; s < 16; ++s) {
    if (r.a.attacks & R::stepBit(static_cast<uint8_t>(s)))
      o << s << ':' << ((r.a.noteAt[s] % 12 + 12 - p.rootPc) % 12) << '/' << r.a.noteAt[s] / 12 << ' ';
  }
  return o.str();
}

std::string outDir() {
  const char* e = std::getenv("M0_OUT");
  return e ? e : "build/m0a";
}

// First ordinal (0..23) at which the most phrase laws (1..3) are applicable for
// this genre/length/level, so the causal comparison holds identity constant while
// using an archetype that is actually admitted to phrase evolution. -1: none.
int bestOrdinal(const GenreCase& g, uint8_t bars, R::RealizationLevel level, int* appliedOut = nullptr) {
  int best = -1, bestCount = 0;
  for (uint32_t o = 0; o < 24; ++o) {
    int count = 0;
    for (int law = 1; law <= 3; ++law) {
      MakeOptions opt;
      opt.level = level;
      opt.lawOverride = law;
      Phrase p;
      if (makePhrase(g, bars, o, opt, p)) ++count;
    }
    if (count > bestCount) { best = static_cast<int>(o); bestCount = count; }
    if (bestCount == 3) break;
  }
  if (appliedOut) *appliedOut = bestCount;
  return best;
}

}  // namespace

// =================================================================== main
#ifndef M0A_NO_MAIN
int main() {
  const std::string out = outDir();
  std::filesystem::create_directories(out + "/corpus");
  std::filesystem::create_directories(out + "/listening");
  const char* setNames[] = {"Acid", "Techno", "Darksynth", "House", "Dub", "Funk", "UKG", "DnB"};

  // ---- 1. corpus generation (+ determinism) ---------------------------------
  std::printf("== M0A-1 STRUCTURAL CORPUS (fixed identity ordinal=%u, level P2, timbre-free) ==\n", kOrdinal);
  std::map<std::string, Phrase> p4, p8;
  uint64_t hashA = 1469598103934665603ull, hashB = 1469598103934665603ull;
  std::ofstream tsv(out + "/corpus/structural_corpus.tsv");
  tsv << "genre\tbars\tbar\tfunction\tbassRhythm\tbpm\trootPc\tK\tS\tCH\tOH\tMT\tHT\tRIM\tCLAP\t"
         "bassAtt\tbassCont\tbassAcc\tbassSlide\tchordAtt\tchordCont\tharm\tharmEvents\tbassPcRel/oct\n";
  for (int pass = 0; pass < 2; ++pass) {
    for (const GenreCase& g : kGenres) {
      for (uint8_t bars : {uint8_t(4), uint8_t(8)}) {
        Phrase p;
        makePhrase(g, bars, kOrdinal, MakeOptions{}, p);
        if (pass == 0) (bars == 4 ? p4 : p8)[g.name] = p;
        if (!p.ok) continue;
        for (int b = 0; b < bars; ++b) {
          const std::string row = corpusRow(p, b);
          (pass == 0 ? hashA : hashB) = fnv(pass == 0 ? hashA : hashB, row);
          if (pass == 0) tsv << row << '\n';
        }
      }
    }
  }
  tsv.close();
  std::printf("  corpus hash run1=%016llx run2=%016llx\n", (unsigned long long)hashA, (unsigned long long)hashB);
  check(hashA == hashB, "corpus generation is deterministic");
  std::printf("  corpus file: %s/corpus/structural_corpus.tsv\n", out.c_str());
  for (const GenreCase& g : kGenres) {
    for (auto* m : {&p4, &p8}) {
      const Phrase& p = (*m)[g.name];
      std::printf("  %-11s bars=%d %-6s archetype=%-18s law=%-13s traj=%d %s\n", g.name, p.bars,
                  p.ok ? "OK" : "n/a", p.archetype.c_str(), lawName(p.law), int(p.trajectory),
                  p.ok ? "" : p.status.c_str());
    }
  }

  // ---- 2. pair comparison after timbre removal ------------------------------
  std::printf("\n== M0A-2 TIMBRE-REMOVAL PAIRS (4-bar corpus; descriptive, NOT a classifier) ==\n");
  struct Pair { const char* a; const char* b; };
  const Pair pairs[] = {{"Techno", "Darksynth"}, {"House", "Techno"}, {"House", "Dub"},
                        {"Techno", "Funk"}, {"UKG", "DnB"}, {"Acid", "Techno"}};
  for (const Pair& pr : pairs) {
    const Phrase& x = p4[pr.a];
    const Phrase& y = p4[pr.b];
    if (!x.ok || !y.ok) {
      std::printf("  %-9s vs %-9s INCONCLUSIVE (%s: %s | %s: %s)\n", pr.a, pr.b, pr.a,
                  x.ok ? "OK" : x.status.c_str(), pr.b, y.ok ? "OK" : y.status.c_str());
      continue;
    }
    const PairVerdict v = comparePhrases(x, y, false);
    std::printf("  %-9s vs %-9s %-32s drums=%s(%.1f) bassRhythm=%s(%.1f) bassPitch=%s(%.2f) harmony=%s(%.1f) "
                "articulation=%s(%.1f) chord=%s(%.1f)  [%s vs %s]\n",
                pr.a, pr.b, v.label, v.drums ? "DIFF" : "same", v.dDrums, v.bassRhythm ? "DIFF" : "same", v.dBass,
                v.bassPitch ? "DIFF" : "same", v.dPc, v.harmony ? "DIFF" : "same", v.dHarm,
                v.articulation ? "DIFF" : "same", v.dArt, v.chord ? "DIFF" : "same", v.dChord,
                x.archetype.c_str(), y.archetype.c_str());
  }
  {  // Acid articulation falsification
    const Phrase& acid = p4["Acid"];
    const Phrase& techno = p4["Techno"];
    if (acid.ok && techno.ok) {
      const PairVerdict with = comparePhrases(acid, techno, false);
      const PairVerdict without = comparePhrases(acid, techno, true);
      const Signature sa = signatureOf(acid);
      std::printf("  ACID articulation test: with articulation dims=%d, articulation stripped dims=%d; "
                  "acid slide/accent per bar=%.1f/%.1f -> %s\n",
                  with.diffCount, without.diffCount, sa.slide, sa.accent,
                  (with.diffCount > without.diffCount && without.diffCount >= 2)
                      ? "Acid remains structurally distinct WITHOUT articulation (articulation adds identity)"
                  : (with.diffCount > without.diffCount)
                      ? "genre identity includes articulation (structure alone is weaker)"
                      : "articulation not needed for the observed difference");
    }
  }

  {  // Acid articulation across identities and depths (single-identity numbers can mislead)
    for (auto level : {R::RealizationLevel::P2Variation, R::RealizationLevel::P3Transformation}) {
      for (const char* n : {"Acid", "AcidRolling", "Techno"}) {
        const GenreCase* g = findGenre(n);
        double slide = 0, accent = 0, cont = 0, attacks = 0, count = 0;
        MakeOptions opt;
        opt.level = level;
        for (uint32_t o = 0; o < 24; ++o) {
          Phrase p;
          if (!makePhrase(*g, 4, o, opt, p)) continue;
          for (auto& b : p.bar) {
            slide += pop16(b.a.slide & b.a.attacks); accent += pop16(b.a.accent);
            cont += pop16(b.a.cont); attacks += pop16(b.a.attacks); count += 1;
          }
        }
        std::printf("  ARTICULATION %s over 24 identities %-11s per-bar: bass attacks=%.1f slide-into-attack=%.2f accent=%.2f continuations=%.2f\n",
                    levelName(level), n, attacks / count, slide / count, accent / count, cont / count);
      }
    }
  }

  // ---- 3. per-genre realized profile ---------------------------------------
  std::printf("\n== M0A-3 REALIZED PROFILE PER GENRE (4-bar, per-bar means) ==\n");
  for (const GenreCase& g : kGenres) {
    const Phrase& p = p4[g.name];
    if (!p.ok) { std::printf("  %-11s n/a: %s\n", g.name, p.status.c_str()); continue; }
    const Signature s = signatureOf(p);
    std::printf("  %-11s %-16s bass-id=%-14s kick=%.1f snare=%.1f chat=%.1f ohat=%.1f bassAtt=%.1f chordAtt=%.1f "
                "bass&kick=%.1f slide=%.1f accent=%.1f harmEv=%.1f offbeat=%.2f\n",
                g.name, p.archetype.c_str(), R::bassRhythmName(p.bar[0].bassId), s.laneHits[0], s.laneHits[1],
                s.laneHits[2], s.laneHits[3], s.bassAttacks, s.chordAttacks, s.bassKickOverlap, s.slide,
                s.accent, s.harmEvents, s.offbeat);
  }

  // Funk / DnB / UKG / Dub specific realized evidence.
  std::printf("\n== M0A-4 GENRE CLAIM CHECKS ==\n");
  {
    const Phrase& f = p4["Funk"];
    if (f.ok) {
      int k0 = 0, s4 = 0, s12 = 0, ghost = 0, b0 = 0, acc0 = 0;
      for (const auto& b : f.bar) {
        k0 += (b.drum[0] & R::stepBit(0)) != 0;
        s4 += (b.drum[1] & R::stepBit(4)) != 0;
        s12 += (b.drum[1] & R::stepBit(12)) != 0;
        ghost += pop16(b.a.ghost) + pop16(b.b.ghost);
        b0 += (b.a.attacks & R::stepBit(0)) != 0;
        acc0 += (b.a.accent & R::stepBit(0)) != 0;
      }
      std::printf("  FUNK archetype=%s: kick@0 in %d/%zu bars, snare backbeat@4 %d, @12 %d, bass@0 %d, accent on bass@0 %d, "
                  "ghost cells %d. Observed: realized pattern shows kick/backbeat/bass co-occurrence at step 0; "
                  "NO owner establishes a metric hierarchy or 'The One' as a constraint (a tick-0 event is not The One).\n",
                  f.archetype.c_str(), k0, f.bar.size(), s4, s12, b0, acc0, ghost);
    }
    const Phrase& d = p4["DnB"];
    if (d.ok) {
      const Signature s = signatureOf(d);
      const double drumFast = s.laneHits[0] + s.laneHits[1] + s.laneHits[2] + s.laneHits[3];
      std::printf("  DNB archetype=%s bass-id=%s: bass attacks/bar=%.1f vs kick+snare+hats=%.1f; bass-vs-drum hierarchy "
                  "owner: BassRhythmId::HalfTimePocket %s in this realization. Slower-bass-against-faster-drums relation "
                  "is %s.\n",
                  d.archetype.c_str(), R::bassRhythmName(d.bar[0].bassId), s.bassAttacks, drumFast,
                  d.bar[0].bassId == R::BassRhythmId::HalfTimePocket ? "SELECTED" : "not selected",
                  (d.bar[0].bassId == R::BassRhythmId::HalfTimePocket && s.bassAttacks * 2 < drumFast) ? "observed" : "GAP (not established by any realized owner here)");
    }
    const Phrase& u = p4["UKG"];
    if (u.ok) {
      const Signature s = signatureOf(u);
      std::printf("  UKG archetype=%s: kick=%.1f snare=%.1f chat=%.1f offbeat=%.2f timing-shifted drum steps/bar=%.1f "
                  "(two-step/shuffle organisation is visible in drum masks; swing is realized as step timing)\n",
                  u.archetype.c_str(), s.laneHits[0], s.laneHits[1], s.laneHits[2], s.offbeat,
                  [&] { double t = 0; for (auto& b : u.bar) t += b.drumTimingNonZero; return t / u.bar.size(); }());
    }
    const Phrase& du = p4["Dub"];
    if (du.ok) {
      const Signature s = signatureOf(du);
      double total = 0; for (int v = 0; v < 8; ++v) total += s.laneHits[v];
      int automation = 0;
      for (auto& b : du.bar) for (int l = 0; l < DrumPatternSet::kMaxLanes; ++l) automation += b.drums.lanes[l].nodeCount;
      std::printf("  DUB archetype=%s: drum hits/bar=%.1f bass attacks=%.1f chord attacks=%.1f offbeat=%.2f; "
                  "production automation nodes (reverb/compression/transient) in generated patterns=%d -> time-domain "
                  "production behaviour %s in pattern data.\n",
                  du.archetype.c_str(), total, s.bassAttacks, s.chordAttacks, s.offbeat, automation,
                  automation > 0 ? "IS PRESENT" : "is NOT represented");
    }
    const Phrase& t = p4["Techno"];
    if (t.ok) {
      bool sameRoot = true;
      int changedBars = 0;
      for (size_t i = 1; i < t.bar.size(); ++i)
        if (featureDistance(featureOf(t.bar[i]), featureOf(t.bar[0])) != 0) ++changedBars;
      std::printf("  TECHNO archetype=%s: harmonic events/bar=%.1f; bars differing from bar0 in any lane: %d/%zu "
                  "(multi-bar development beyond the same bar with another mask: %s)\n",
                  t.archetype.c_str(), signatureOf(t).harmEvents, changedBars, t.bar.size() - 1,
                  changedBars > 0 ? "some lane change exists" : "NONE observed");
      (void)sameRoot;
    }
    const Phrase& h = p4["House"];
    if (h.ok) {
      const Signature s = signatureOf(h);
      std::printf("  HOUSE archetype=%s: kick=%.1f (quarter pulse=4) chat=%.1f ohat=%.1f offbeat=%.2f harmEv=%.1f\n",
                  h.archetype.c_str(), s.laneHits[0], s.laneHits[2], s.laneHits[3], s.offbeat, s.harmEvents);
    }
  }

  // ---- 5. multi-temporal activity ------------------------------------------
  std::printf("\n== M0A-5 MULTI-TEMPORAL ACTIVITY (natural production law, level P2) ==\n");
  for (const char* n : setNames) {
    for (auto* m : {&p4, &p8}) {
      const Phrase& p = (*m)[n];
      if (!p.ok) continue;
      std::printf("  %s bars=%d law=%s traj=%d fns:", n, p.bars, lawName(p.law), int(p.trajectory));
      for (const auto& b : p.bar) std::printf(" %s", barFnName(b.fn));
      std::printf("\n");
      printTemporal(p);
    }
  }

  // ---- 6. natural reachability of phrase laws --------------------------------
  std::printf("\n== M0A-6 NATURAL PHRASE-LAW REACHABILITY (production selection, ordinals 0..47) ==\n");
  for (const char* n : setNames) {
    const GenreCase* g = findGenre(n);
    for (auto level : {R::RealizationLevel::P2Variation, R::RealizationLevel::P3Transformation}) {
      for (uint8_t bars : {uint8_t(4), uint8_t(8)}) {
        std::map<std::string, int> hist;
        int notOk = 0;
        for (uint32_t o = 0; o < 48; ++o) {
          Phrase p;
          MakeOptions opt;
          opt.level = level;
          if (!makePhrase(*g, bars, o, opt, p)) { ++notOk; continue; }
          char key[64];
          std::snprintf(key, sizeof(key), "%s/t%d", lawName(p.law), int(p.trajectory));
          ++hist[key];
        }
        std::printf("  %-9s %s %d-bar:", n, levelName(level), bars);
        for (auto& kv : hist) std::printf(" %s=%d", kv.first.c_str(), kv.second);
        if (notOk) std::printf(" (unavailable %d/48)", notOk);
        std::printf("\n");
      }
    }
  }

  // ---- 7. phrase-law causal test ----------------------------------------------
  std::printf("\n== M0A-7 PHRASE-LAW CAUSAL TEST (same genre/recipe/identity ordinal/feel/length; ONLY the law varies) ==\n");
  std::printf("  (ordinal = first of 0..23 whose archetype is admitted to phrase evolution; NOT_APPLICABLE otherwise)\n");
  for (const char* n : setNames) {
    const GenreCase* g = findGenre(n);
    for (auto level : {R::RealizationLevel::P2Variation, R::RealizationLevel::P3Transformation}) {
      for (uint8_t bars : {uint8_t(4), uint8_t(8)}) {
        int applied = 0;
        const int ord = bestOrdinal(*g, bars, level, &applied);
        if (ord < 0) {
          MakeOptions probe; probe.level = level; probe.lawOverride = 0;
          Phrase pr;
          makePhrase(*g, bars, kOrdinal, probe, pr);
          std::printf("  %-9s %s %d-bar: NOT_APPLICABLE (no law reachable at ordinals 0..23; %s)\n", n,
                      levelName(level), bars, pr.ok ? "archetypes not admitted / no eligible trajectory" : pr.status.c_str());
          continue;
        }
        MakeOptions base;
        base.level = level;
        base.lawOverride = 0;
        Phrase loop;
        makePhrase(*g, bars, ord, base, loop);
        for (int law = 1; law <= 3; ++law) {
          MakeOptions o = base;
          o.lawOverride = law;
          Phrase v;
          if (!makePhrase(*g, bars, ord, o, v)) {
            std::printf("  %-9s %s %d-bar ord=%d %-13s vs Loop: NOT_APPLICABLE (%s)\n", n, levelName(level), bars, ord,
                        lawName(static_cast<R::PhraseEvolutionLawId>(law)), v.status.c_str());
            continue;
          }
          std::printf("  %-9s %s %d-bar ord=%d arch=%-16s %-13s t%d fns:", n, levelName(level), bars, ord,
                      v.archetype.c_str(), lawName(v.law), int(v.trajectory));
          for (const auto& b : v.bar) std::printf(" %s", barFnName(b.fn));
          int dDr = 0, dBa = 0, dPc = 0, dCh = 0;
          for (size_t i = 0; i < v.bar.size(); ++i) {
            bool drDiff = false;
            for (int l = 0; l < 8; ++l) if (v.bar[i].drum[l] != loop.bar[i].drum[l]) drDiff = true;
            dDr += drDiff;
            dBa += v.bar[i].a.attacks != loop.bar[i].a.attacks;
            bool pcDiff = false;
            for (int st = 0; st < 16; ++st) if (v.bar[i].a.noteAt[st] % 12 != loop.bar[i].a.noteAt[st] % 12) pcDiff = true;
            dPc += pcDiff;
            dCh += v.bar[i].b.attacks != loop.bar[i].b.attacks;
          }
          std::printf(" | bars differing from Loop drums/bassRhythm/bassPitch/chord: %d/%d/%d/%d of %zu\n", dDr, dBa, dPc, dCh, v.bar.size());
        }
      }
    }
  }

  // ---- 8. RETURN: what actually returns ----------------------------------------
  std::printf("\n== M0A-8 WHAT RETURNS (bar with function Return vs bar 0) ==\n");
  struct RetCfg { R::RealizationLevel level; int law; };
  for (const char* n : setNames) {
    const GenreCase* g = findGenre(n);
    for (const RetCfg& rc : {RetCfg{R::RealizationLevel::P2Variation, 1}, RetCfg{R::RealizationLevel::P2Variation, 2},
                             RetCfg{R::RealizationLevel::P3Transformation, 3}}) {
      const int ord = bestOrdinal(*g, 4, rc.level);
      if (ord < 0) { std::printf("  %-9s %s law%d NOT_APPLICABLE (no admitted archetype in ordinals 0..23)\n", n, levelName(rc.level), rc.law); continue; }
      MakeOptions o;
      o.level = rc.level;
      o.lawOverride = rc.law;
      Phrase p;
      if (!makePhrase(*g, 4, ord, o, p)) { std::printf("  %-9s %s law%d NOT_APPLICABLE at ord=%d (%s)\n", n, levelName(rc.level), rc.law, ord, p.status.c_str()); continue; }
      bool sawReturn = false;
      for (size_t i = 1; i < p.bar.size(); ++i) {
        if (p.bar[i].fn != R::BarFunction::Return) continue;
        sawReturn = true;
        const BarRec& a = p.bar[0];
        const BarRec& r = p.bar[i];
        bool drumSame = true;
        for (int l = 0; l < 8; ++l) drumSame = drumSame && a.drum[l] == r.drum[l];
        int pcDiff = 0;
        for (int st = 0; st < 16; ++st) if (a.a.noteAt[st] % 12 != r.a.noteAt[st] % 12) ++pcDiff;
        std::printf("  %-9s %s %-13s ord=%d arch=%-16s bar%zu Return vs bar0: drums=%s bassRhythm=%s bassPitchClasses=%s(%d steps differ) "
                    "chordRhythm=%s harmonicTiming=%s\n",
                    n, levelName(rc.level), lawName(p.law), ord, p.archetype.c_str(), i, drumSame ? "IDENTICAL" : "differs",
                    a.a.attacks == r.a.attacks ? "IDENTICAL" : "differs", pcDiff == 0 ? "IDENTICAL" : "differs", pcDiff,
                    a.b.attacks == r.b.attacks ? "IDENTICAL" : "differs", a.harm == r.harm ? "IDENTICAL" : "differs");
      }
      if (!sawReturn) std::printf("  %-9s %s %-13s ord=%d: trajectory has no Return bar\n", n, levelName(rc.level), lawName(p.law), ord);
    }
  }

  // ---- 9. BREAK: what changes ---------------------------------------------------
  std::printf("\n== M0A-9 BREAK OWNER (bar with function Break vs bar 0; SparseDrift at P3, 4 and 8 bars) ==\n");
  for (const char* n : setNames) {
    const GenreCase* g = findGenre(n);
    for (uint8_t bars : {uint8_t(4), uint8_t(8)}) {
      int chosen = -1;
      for (uint32_t o = 0; o < 24 && chosen < 0; ++o) {
        MakeOptions probe; probe.level = R::RealizationLevel::P3Transformation; probe.lawOverride = 3;
        Phrase pr;
        if (makePhrase(*g, bars, o, probe, pr)) chosen = static_cast<int>(o);
      }
      if (chosen < 0) { std::printf("  %-9s %d-bar NOT_APPLICABLE (SparseDrift/Break not reachable at ordinals 0..23)\n", n, bars); continue; }
      MakeOptions o;
      o.level = R::RealizationLevel::P3Transformation;
      o.lawOverride = 3;
      Phrase p;
      makePhrase(*g, bars, chosen, o, p);
      bool any = false;
      for (size_t i = 1; i < p.bar.size(); ++i) {
        if (p.bar[i].fn != R::BarFunction::Break) continue;
        any = true;
        std::printf("  %-9s %d-bar ord=%d arch=%-16s bar%zu Break vs bar0: drum hits %.0f->%.0f kick %d->%d snare %d->%d bass attacks %d->%d chord attacks %d->%d\n",
                    n, bars, chosen, p.archetype.c_str(), i, drumHits(p.bar[0]), drumHits(p.bar[i]), pop16(p.bar[0].drum[0]),
                    pop16(p.bar[i].drum[0]), pop16(p.bar[0].drum[1]), pop16(p.bar[i].drum[1]), pop16(p.bar[0].a.attacks),
                    pop16(p.bar[i].a.attacks), pop16(p.bar[0].b.attacks), pop16(p.bar[i].b.attacks));
      }
      if (!any) std::printf("  %-9s %d-bar ord=%d trajectory has no Break bar\n", n, bars, chosen);
    }
  }

  // ---- 10. one-minute failure test ------------------------------------------------
  std::printf("\n== M0A-10 ONE-MINUTE TEST (production mechanisms only) ==\n");
  for (const char* n : {"Acid", "Techno", "Dub"}) {
    const GenreCase* g = findGenre(n);
    Phrase base;
    uint8_t unit = 8;
    if (!makePhrase(*g, 8, kOrdinal, MakeOptions{}, base)) {
      unit = 4;
      if (!makePhrase(*g, 4, kOrdinal, MakeOptions{}, base)) { std::printf("  %s n/a (%s)\n", n, base.status.c_str()); continue; }
    }
    const double barSec = 4.0 * 60.0 / std::max(30.0f, base.bpm);
    const int total = static_cast<int>(std::ceil(60.0 / barSec));
    const int loops = (total + unit - 1) / unit;
    // (a) LOOP: leave the 8-bar phrase running.  (b) TAKES: press generate every 8 bars.
    std::vector<Feature> loopF, takeF;
    std::vector<const BarRec*> loopBars, takeBars;
    std::vector<Phrase> takes(loops);
    for (int i = 0; i < loops; ++i) {
      for (auto& b : base.bar) { loopF.push_back(featureOf(b)); loopBars.push_back(&b); }
      makePhrase(*g, unit, kOrdinal + 1 + i, MakeOptions{}, takes[i]);
      if (!takes[i].ok) continue;
    }
    for (auto& t : takes) for (auto& b : t.bar) { takeF.push_back(featureOf(b)); takeBars.push_back(&b); }
    auto predictable = [unit](const std::vector<Feature>& f) {
      int seen = 0, tot = 0;
      for (size_t i = unit; i < f.size(); ++i) {
        ++tot;
        for (size_t j = 0; j < unit; ++j)
          if (featureDistance(f[i], f[j]) == 0) { ++seen; break; }
      }
      return tot ? 100.0 * seen / tot : 0.0;
    };
    std::printf("  %-7s unit=%d-bar phrase, bpm=%.0f => %d bars ~ %.0fs. LOOP: change per level bar=%.2f 2bar=%.2f 4bar=%.2f 8bar=%.2f 16bar=%.2f; "
                "bars after first phrase already heard=%.0f%%\n",
                n, unit, base.bpm, total, total * barSec, levelChange(loopF, 1), levelChange(loopF, 2), levelChange(loopF, 4),
                levelChange(loopF, 8), levelChange(loopF, 16), predictable(loopF));
    std::printf("          TAKES(regenerate every 8 bars): bar=%.2f 2bar=%.2f 4bar=%.2f 8bar=%.2f 16bar=%.2f; bars after first "
                "phrase matching an earlier bar=%.0f%%; laws:",
                levelChange(takeF, 1), levelChange(takeF, 2), levelChange(takeF, 4), levelChange(takeF, 8),
                levelChange(takeF, 16), predictable(takeF));
    for (auto& t : takes) std::printf(" %s", t.ok ? lawName(t.law) : "n/a");
    std::printf("\n          classification: %s\n",
                predictable(loopF) > 90.0 ? "LONG-FORM GAP (loop is fully predictable after the first phrase; takes are unrelated, not developed)"
                                          : "not fully predictable");
    writeSmf(out + "/listening/oneminute_" + n + "_loop.mid", loopBars, base.bpm);
    writeSmf(out + "/listening/oneminute_" + n + "_takes.mid", takeBars, base.bpm);
  }

  // ---- 11. listening corpus -----------------------------------------------------------
  std::printf("\n== M0A-11 LISTENING CORPUS ==\n");
  std::ofstream manifest(out + "/listening/manifest.tsv");
  manifest << "file\tgenre\tarchetype\tbars\tlaw\tlevel\tbarFunctions\tstatus\n";
  std::ofstream cards(out + "/listening/listening_cards.md");
  cards << "# M0-A listening cards\n\nAnswer YES / NO / UNCLEAR (optional short note). No theory vocabulary, no scores.\n\n";
  struct Ex { const char* tag; const char* human; R::RealizationLevel level; int law; uint8_t bars; };
  const Ex exs[] = {
      {"loop", "LOOP (same idea repeating)", R::RealizationLevel::P2Variation, 0, 4},
      {"reply", "DEVELOP/REPLY (idea, answer, repeat, return)", R::RealizationLevel::P2Variation, 1, 4},
      {"develop_return", "DEVELOP then RETURN (idea, repeat, thin out, return)", R::RealizationLevel::P2Variation, 2, 4},
      {"break_return", "BREAK then RETURN (idea, ghosted repeat, break, return) [P3]", R::RealizationLevel::P3Transformation, 3, 4},
  };
  for (const char* n : {"Acid", "Techno", "Dub", "UKG", "DnB"}) {
    const GenreCase* g = findGenre(n);
    for (const Ex& e : exs) {
      MakeOptions o;
      o.level = e.level;
      o.lawOverride = e.law;
      Phrase p;
      const std::string file = std::string(n) + "_" + e.tag + "_" + std::to_string(e.bars) + "bar.mid";
      int ord = -1;
      for (uint32_t cand = 0; cand < 24 && ord < 0; ++cand) {
        Phrase probe;
        if (makePhrase(*g, e.bars, cand, o, probe)) ord = static_cast<int>(cand);
      }
      if (ord < 0 || !makePhrase(*g, e.bars, static_cast<uint32_t>(ord), o, p)) {
        makePhrase(*g, e.bars, kOrdinal, o, p);
        manifest << file << '\t' << n << "\t-\t" << int(e.bars) << '\t' << lawName(static_cast<R::PhraseEvolutionLawId>(e.law))
                 << '\t' << levelName(e.level) << "\t-\tGAP: " << p.status << '\n';
        std::printf("  GAP  %-34s %s\n", file.c_str(), p.status.c_str());
        continue;
      }
      // Two passes so the ear hears the shape twice (idea -> shape -> idea).
      std::vector<const BarRec*> bars;
      for (int pass = 0; pass < 2; ++pass) for (auto& b : p.bar) bars.push_back(&b);
      writeSmf(out + "/listening/" + file, bars, p.bpm);
      std::string fns;
      for (auto& b : p.bar) { fns += barFnName(b.fn); fns += ','; }
      manifest << file << '\t' << n << '\t' << p.archetype << '\t' << int(e.bars) << '\t' << lawName(p.law) << '\t'
               << levelName(e.level) << '\t' << fns << "\tOK ordinal=" << ord << "\n";
      std::printf("  OK   %-34s ord=%d arch=%s law=%s fns=%s\n", file.c_str(), ord, p.archetype.c_str(), lawName(p.law), fns.c_str());
      cards << "## " << file << " -- " << e.human << " (" << n << ")\n\n"
            << "1. Can you hear which section is the original idea? YES / NO / UNCLEAR\n"
            << "2. Does the development feel related but changed? YES / NO / UNCLEAR\n"
            << "3. Is the break an obvious contrast? YES / NO / UNCLEAR\n"
            << "4. When the return happens, is it recognizably a return? YES / NO / UNCLEAR\n"
            << "5. After 30 seconds, did anything meaningful remain unpredictable? YES / NO / UNCLEAR\n"
            << "6. Would you press DEVELOP again? YES / NO / UNCLEAR\n\nNote: ______\n\n";
    }
  }
  std::printf("  listening dir: %s/listening (manifest.tsv, listening_cards.md, *.mid)\n", out.c_str());

  std::printf("\nM0-A tool self-checks: %s\n", g_failures == 0 ? "PASS" : "FAIL");
  return g_failures == 0 ? 0 : 1;
}
#endif  // M0A_NO_MAIN
