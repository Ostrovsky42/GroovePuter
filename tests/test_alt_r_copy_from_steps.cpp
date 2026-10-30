// Alt+R on STEPS with no Melody copies the steps into a new Melody and the
// synth/params pages toast "MELODY <- STEPS".
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/ui/phrase_source_toggle.h"

SerialMock Serial;
SDMock SD;

namespace {
int g_failures = 0;
void expect(bool ok, const char* msg) {
  if (ok) return;
  std::fprintf(stderr, "ALT-R-COPY FAIL: %s\n", msg);
  ++g_failures;
}
constexpr int kA = 0;
}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};
  expect(engine.rebuildPatternRuntimeEventBank(), "runtime Pattern bank must build");
  engine.editSynthPattern(kA).steps[0].note = 36;
  engine.editSynthPattern(kA).steps[4].note = 43;
  expect(engine.retainedWorkingMelody(kA) == nullptr, "precondition: no Melody");

  expect(PhraseSourceToggle::switchSource(engine, nullptr, kA) ==
             PhraseSourceToggle::Result::MadePhrase,
         "switch with no Melody did not copy");
  expect(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Phrase,
         "source is not MELODY");
  expect(engine.retainedWorkingMelody(kA) != nullptr, "no Melody created");
  const auto& m = engine.currentPhraseBuffer(kA);
  expect(m.count == 2, "Melody does not hold the two step notes");
  if (m.count == 2) {
    expect(m.events[0].note == 36 && m.events[1].note == 43, "Melody notes differ from STEPS");
  }
  expect(engine.editSynthPattern(kA).steps[0].note == 36 &&
             engine.editSynthPattern(kA).steps[4].note == 43,
         "copy changed the steps");

  const char* const files[] = {"../src/ui/pages/synth_sequencer_page.cpp",
                               "../src/ui/pages/tb303_params_page.cpp"};
  for (const char* f : files) {
    std::ifstream in(f);
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string src = buf.str();
    expect(!src.empty(), "page source unreadable");
    expect(src.find("\"MELODY <- STEPS\"") != std::string::npos, "copy toast missing");
    expect(src.find("MELODY EMPTY") == std::string::npos, "old refusal toast remains");
  }

  if (g_failures == 0) {
    std::printf("alt+r copy from steps: PASS\n");
    return 0;
  }
  return 1;
}
