#define M0A_NO_MAIN
#include "../m0/m0a_corpus.cpp"
#define M0A_RENDER_EXTERNAL_CORPUS
#define M0A_RENDER_NO_MAIN
#include "../m0/m0a_render.cpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>

namespace {

const char* policyName(R::PhraseHarmonicPolicyId policy) {
  switch (policy) {
    case R::PhraseHarmonicPolicyId::HalfBar: return "HALF-BAR";
    case R::PhraseHarmonicPolicyId::Static: return "STATIC";
    case R::PhraseHarmonicPolicyId::Slow: return "SLOW";
    case R::PhraseHarmonicPolicyId::Prolong: return "PROLONG";
    case R::PhraseHarmonicPolicyId::Syncopated: return "SYNCOPATED";
    default: return "?";
  }
}

const char* roleName(R::CompositionSecondaryRole role) {
  switch (role) {
    case R::CompositionSecondaryRole::Chord: return "Chord";
    case R::CompositionSecondaryRole::Melodic: return "Melodic";
    case R::CompositionSecondaryRole::ChordWithMelodicFill:
      return "ChordWithMelodicFill";
    default: return "?";
  }
}

bool sameProgressionSource(const R::ChordProgressionSource& a,
                           const R::ChordProgressionSource& b) {
  if (a.id != b.id || a.period != b.period) return false;
  for (uint8_t i = 0; i < a.period; ++i) {
    if (a.events[i].degree != b.events[i].degree ||
        a.events[i].quality != b.events[i].quality ||
        a.events[i].rootOffsetSemitones != b.events[i].rootOffsetSemitones)
      return false;
  }
  return true;
}

bool sameCompositionExceptPolicy(const R::GenerationCompositionResult& a,
                                 const R::GenerationCompositionResult& b) {
  return a.status == b.status &&
         a.rhythmSelectionMode == b.rhythmSelectionMode &&
         a.rhythmArchetypeId == b.rhythmArchetypeId &&
         a.normalizedRhythmToAuto == b.normalizedRhythmToAuto &&
         a.suggestedFeel == b.suggestedFeel && a.bassRhythm == b.bassRhythm &&
         a.chordRhythm == b.chordRhythm && a.progression == b.progression &&
         a.melodicRhythm == b.melodicRhythm && a.motifShape == b.motifShape &&
         a.phraseLaw == b.phraseLaw && a.phraseBars == b.phraseBars &&
         a.corridor.bpmMin == b.corridor.bpmMin &&
         a.corridor.bpmMax == b.corridor.bpmMax &&
         a.corridor.suggestedBpm == b.corridor.suggestedBpm &&
         a.corridor.gridSteps == b.corridor.gridSteps &&
         a.corridor.densityMin == b.corridor.densityMin &&
         a.corridor.densityMax == b.corridor.densityMax &&
         a.secondaryRole == b.secondaryRole;
}

std::string pitchClasses(const LaneRec& lane, uint8_t firstStep,
                         uint8_t stepCount) {
  std::set<uint8_t> values;
  for (uint8_t step = firstStep; step < firstStep + stepCount; ++step) {
    if ((lane.attacks & R::stepBit(step)) == 0 || lane.noteAt[step] < 0)
      continue;
    values.insert(static_cast<uint8_t>(lane.noteAt[step] % 12));
  }
  if (values.empty()) return "-";
  std::string result;
  for (uint8_t value : values) {
    if (!result.empty()) result += ',';
    result += std::to_string(value);
  }
  return result;
}

uint8_t harmonicRootPc(const Phrase& phrase, uint8_t sourceOrdinal) {
  const auto sourceEvent =
      R::chordProgressionEventAt(phrase.progressionSource, sourceOrdinal);
  const int semitone = R::scaleDegreeToSemitone(
      phrase.scaleType, sourceEvent.event.degree);
  const int pc = static_cast<int>(phrase.rootPc) + semitone +
                 sourceEvent.event.rootOffsetSemitones;
  return static_cast<uint8_t>((pc % 12 + 12) % 12);
}

void appendMaterializedRows(std::ofstream& report, const char* variant,
                            const Phrase& phrase) {
  const uint16_t phraseSteps =
      static_cast<uint16_t>(phrase.harmonicTimeline.phraseBars) * 16u;
  for (uint8_t index = 0;
       index < phrase.harmonicTimeline.totalEventPositions; ++index) {
    const R::PhraseHarmonicEvent& event = phrase.harmonicTimeline.events[index];
    uint16_t position = event.onsetStep;
    uint16_t remaining = event.durationSteps;
    while (remaining > 0 && position < phraseSteps) {
      const uint8_t bar = static_cast<uint8_t>(position / 16u);
      const uint8_t localStep = static_cast<uint8_t>(position % 16u);
      const uint8_t count = static_cast<uint8_t>(
          std::min<uint16_t>(remaining, 16u - localStep));
      report << variant << "\tHouse\t" << static_cast<unsigned>(phrase.recipe)
             << '\t' << policyName(phrase.harmonicPolicy) << '\t'
             << static_cast<unsigned>(bar) << '\t'
             << static_cast<unsigned>(event.sourceOrdinal) << '\t'
             << static_cast<unsigned>(harmonicRootPc(phrase, event.sourceOrdinal))
             << '\t' << pitchClasses(phrase.bar[bar].a, localStep, count)
             << '\t' << pitchClasses(phrase.bar[bar].b, localStep, count)
             << '\n';
      position = static_cast<uint16_t>(position + count);
      remaining = static_cast<uint16_t>(remaining - count);
    }
  }
}

bool renderTwice(const GenreCase& genre, const Phrase& phrase,
                 const std::string& path) {
  std::vector<const BarRec*> bars;
  bars.reserve(static_cast<size_t>(phrase.bars) * 2u);
  for (uint8_t repeat = 0; repeat < 2; ++repeat)
    for (const BarRec& bar : phrase.bar) bars.push_back(&bar);

  std::vector<int16_t> pcm;
  std::string note;
  if (!render(genre, phrase.suggestedBpm, bars, pcm, note)) {
    std::fprintf(stderr, "H0-R1 render failed: %s\n", note.c_str());
    return false;
  }
  const size_t samplesPerBar = static_cast<size_t>(
      std::llround(4.0 * 60.0 / phrase.suggestedBpm * kRenderRate));
  const double firstLoopRms = rmsOf(pcm, 0, samplesPerBar * phrase.bars);
  const double secondLoopRms = rmsOf(
      pcm, samplesPerBar * phrase.bars, samplesPerBar * phrase.bars * 2u);
  if (firstLoopRms < 0.005 || secondLoopRms < 0.005) {
    std::fprintf(stderr, "H0-R1 silent phrase render: %s\n", path.c_str());
    return false;
  }
  writeWav(path, pcm);
  std::printf("Rendered %s at %.1f BPM; %s; loop RMS %.4f / %.4f\n",
              path.c_str(), phrase.suggestedBpm, note.c_str(), firstLoopRms,
              secondLoopRms);
  return true;
}

}  // namespace

int main() {
  // Mix C (owner-accepted 2026-09-30): SynthA/B 1.5, drums 0.45.
  g_mix = MixSettings{1.5f, 1.5f, 0.45f};
  const char* outEnv = std::getenv("H0_R1_OUT");
  const std::filesystem::path out = outEnv ? outEnv : "build/h0-r1";
  std::filesystem::create_directories(out);
  std::ofstream report(out / "report.tsv");
  std::ofstream summary(out / "summary.txt");
  if (!report || !summary) {
    std::fprintf(stderr, "Could not create H0-R1 listening outputs\n");
    return 2;
  }
  report << "variant\tgenre\trecipe\tpolicy\tbar\tsource_ordinal\t"
            "harmonic_root_pc\tsynth_a_pitch_classes\tsynth_b_pitch_classes\n";

  const GenreCase* house = findGenre("House");
  if (house == nullptr) return 3;
  constexpr uint32_t kAuditionIdentity = 11;
  MakeOptions halfBarOptions{};
  halfBarOptions.level = R::RealizationLevel::P2Variation;
  MakeOptions slowOptions = halfBarOptions;
  slowOptions.harmonicPolicyOverride =
      static_cast<int>(R::PhraseHarmonicPolicyId::Slow);

  Phrase halfBar{};
  Phrase slow{};
  if (!makePhrase(*house, 4, kAuditionIdentity, halfBarOptions, halfBar) ||
      !makePhrase(*house, 4, kAuditionIdentity, slowOptions, slow)) {
    std::fprintf(stderr, "House phrase generation failed: half=%s slow=%s\n",
                 halfBar.status.c_str(), slow.status.c_str());
    return 4;
  }
  if (halfBar.harmonicPolicy != R::PhraseHarmonicPolicyId::HalfBar ||
      slow.harmonicPolicy != R::PhraseHarmonicPolicyId::Slow ||
      halfBar.phraseIdentity != slow.phraseIdentity ||
      halfBar.projectSeed != slow.projectSeed ||
      halfBar.progression != slow.progression ||
      !sameCompositionExceptPolicy(halfBar.composition, slow.composition) ||
      halfBar.chordRhythm != slow.chordRhythm ||
      halfBar.synthBRole != slow.synthBRole ||
      halfBar.resolvedFeel != slow.resolvedFeel ||
      halfBar.bpm != slow.bpm ||
      !sameProgressionSource(halfBar.progressionSource,
                             slow.progressionSource)) {
    std::fprintf(stderr, "Paired render controls differ beyond harmonic policy\n");
    return 5;
  }

  appendMaterializedRows(report, "house_halfbar", halfBar);
  appendMaterializedRows(report, "house_slow", slow);
  if (!renderTwice(*house, halfBar, (out / "house_halfbar.wav").string()) ||
      !renderTwice(*house, slow, (out / "house_slow.wav").string()))
    return 6;

  MakeOptions fixtureOptions = slowOptions;
  fixtureOptions.controlledCMajorPopCycle = true;
  Phrase fixture{};
  if (!makePhrase(*house, 4, kAuditionIdentity, fixtureOptions, fixture)) {
    std::fprintf(stderr, "Controlled SLOW fixture failed: %s\n",
                 fixture.status.c_str());
    return 7;
  }
  constexpr uint8_t expectedRoots[] = {0, 7, 9, 5};
  for (uint8_t bar = 0; bar < 4; ++bar) {
    const uint8_t actualRoot = harmonicRootPc(fixture, bar);
    const int8_t actualNote = fixture.bar[bar].synthB.steps[0].note;
    if (actualRoot != expectedRoots[bar] || actualNote < 0 ||
        static_cast<uint8_t>(actualNote % 12) != expectedRoots[bar]) {
      std::fprintf(stderr,
                   "Controlled fixture mismatch at bar %u: root=%u note=%d\n",
                   bar, actualRoot, actualNote);
      return 8;
    }
  }
  appendMaterializedRows(report, "slow_c_major_popcycle_fixture", fixture);

  summary << "genre=House\nrecipe=" << static_cast<unsigned>(halfBar.recipe)
          << "\nphrase_generation_identity=" << halfBar.phraseIdentity
          << "\nrealization_project_seed=0x" << std::hex << std::setw(8)
          << std::setfill('0') << halfBar.projectSeed << std::dec
          << "\nrealization_level=P2Variation\nfeel="
          << R::feelProfileName(halfBar.resolvedFeel)
          << "\nprogression=" << R::chordProgressionName(halfBar.progression)
          << "\nbpm=" << std::fixed << std::setprecision(1)
          << halfBar.suggestedBpm
          << "\nchord_rhythm=" << R::chordRhythmName(halfBar.chordRhythm)
          << "\nsynth_b_role=" << roleName(halfBar.synthBRole)
          << "\nhalfbar_policy=" << policyName(halfBar.harmonicPolicy)
          << "\nslow_policy=" << policyName(slow.harmonicPolicy)
          << "\ncontrolled_fixture=House C-major PopCycle, SLOW, roots C/G/A/F verified against Synth B step-0 notes\n";
  std::printf("H0-R1 House paired audit: progression=%s, identity=%u, "
              "project-seed=0x%08x, BPM=%.1f, chord-rhythm=%s, Synth B=%s\n",
              R::chordProgressionName(halfBar.progression),
              halfBar.phraseIdentity, halfBar.projectSeed,
              halfBar.suggestedBpm,
              R::chordRhythmName(halfBar.chordRhythm),
              roleName(halfBar.synthBRole));
  return 0;
}
