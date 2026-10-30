// A user-facing source switch (Alt+R, SONG Opt) only chooses which existing
// material plays. It never creates, copies or converts a Melody.
#include <cstdio>
#include <cstring>

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
  std::fprintf(stderr, "NO-COPY FAIL: %s\n", msg);
  ++g_failures;
}
constexpr int kA = 0;
}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};
  expect(engine.rebuildPatternRuntimeEventBank(), "runtime Pattern bank must build");
  engine.editSynthPattern(kA).steps[0].note = 36;
  engine.editSynthPattern(kA).steps[4].note = 43;

  // 1. STEPS only, no Melody: the switch is refused and nothing is created.
  expect(PhraseSourceToggle::switchSource(engine, nullptr, kA) ==
             PhraseSourceToggle::Result::NoMelody,
         "switch with no Melody was not refused");
  expect(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Pattern,
         "refused switch moved the source");
  expect(engine.retainedWorkingMelody(kA) == nullptr,
         "refused switch created a Melody");
  expect(!engine.hasUnsavedWorkingMelody(kA), "refused switch marked a Melody modified");
  expect(engine.editSynthPattern(kA).steps[0].note == 36 &&
             engine.editSynthPattern(kA).steps[4].note == 43,
         "refused switch changed the steps");

  // 2. With a Melody (made explicitly), switching changes only the source.
  expect(PhraseSourceToggle::makePhrase(engine, nullptr, kA), "explicit make failed");
  expect(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Phrase,
         "explicit make did not land on MELODY");
  const auto before = engine.currentPhraseBuffer(kA);

  expect(PhraseSourceToggle::switchSource(engine, nullptr, kA) ==
             PhraseSourceToggle::Result::SwitchedToPattern,
         "switch to STEPS failed");
  expect(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Pattern,
         "source not STEPS");
  expect(engine.editSynthPattern(kA).steps[0].note == 36, "switch to STEPS changed steps");
  expect(PhraseSourceToggle::switchSource(engine, nullptr, kA) ==
             PhraseSourceToggle::Result::SwitchedToPhrase,
         "switch to MELODY failed");
  expect(engine.currentSequencedSource(kA) == MiniAcid::SequencedSource::Phrase,
         "source not MELODY");
  const auto& after = engine.currentPhraseBuffer(kA);
  expect(after.count == before.count &&
             std::memcmp(after.events, before.events,
                         sizeof(after.events[0]) * before.count) == 0,
         "switching altered the Melody material");

  if (g_failures == 0) {
    std::printf("source toggle no-copy: PASS\n");
    return 0;
  }
  return 1;
}
