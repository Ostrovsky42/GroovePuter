#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/phrase/runtime_pattern_event_bank.h"

namespace {

using namespace PhraseRuntime;

SynthPattern oneNote(uint8_t note) {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
  }
  pattern.steps[0].note = static_cast<int8_t>(note);
  return pattern;
}

PatternProjectionSettings settingsFor(uint8_t synthIndex) {
  PatternProjectionSettings settings{};
  settings.synthIndex = synthIndex;
  settings.gateLengthRatio = 0.5f;
  settings.swingPercent = 50;
  settings.swingEnabled = false;
  return settings;
}

void testUnpublishedBankFailsClosed() {
  RuntimePatternEventBank bank{};
  assert(bank.pageIdentity() == kInvalidPatternRuntimePage);
  assert(bank.refresh(0, 0, 0, oneNote(60), settingsFor(0)) ==
         PatternBankRefreshStatus::Ready);
  assert(bank.select(0, 0, 0).count == 1);
  assert(&bank.selectForPage(0, 0, 0, 0) == &bank.empty());
  std::puts("P2-PAGE A PASS: unpublished prepared bank fails closed");
}

void testPublishedPageSelectsAndWrongPageRejects() {
  RuntimePatternEventBank bank{};
  assert(bank.refresh(0, 0, 0, oneNote(61), settingsFor(0)) ==
         PatternBankRefreshStatus::Ready);
  assert(bank.publishPageIdentity(3));
  assert(bank.pageIdentity() == 3);

  const auto& accepted = bank.selectForPage(3, 0, 0, 0);
  assert(accepted.count == 1);
  assert(accepted.events[0].note == 61);
  assert(&bank.selectForPage(2, 0, 0, 0) == &bank.empty());
  assert(&bank.selectForPage(4, 0, 0, 0) == &bank.empty());
  std::puts("P2-PAGE B PASS: wrong published page is rejected fail-closed");
}

void testInvalidIdentityPublicationIsFailureAtomic() {
  RuntimePatternEventBank bank{};
  assert(bank.refresh(1, 1, 7, oneNote(72), settingsFor(1)) ==
         PatternBankRefreshStatus::Ready);
  assert(bank.publishPageIdentity(5));
  const RuntimePatternEventBank before = bank;

  assert(!bank.publishPageIdentity(-1));
  assert(std::memcmp(&before, &bank, sizeof(bank)) == 0);
  assert(!bank.publishPageIdentity(kMaxPages));
  assert(std::memcmp(&before, &bank, sizeof(bank)) == 0);
  std::puts("P2-PAGE C PASS: invalid page publication is failure-atomic");
}

void testExplicitInvalidationPreventsStaleUseWithoutErasingMaterial() {
  RuntimePatternEventBank bank{};
  assert(bank.refresh(1, 0, 2, oneNote(67), settingsFor(1)) ==
         PatternBankRefreshStatus::Ready);
  assert(bank.publishPageIdentity(1));
  const RuntimePatternEventBuffer before = bank.select(1, 0, 2);

  bank.invalidatePageIdentity();
  assert(bank.pageIdentity() == kInvalidPatternRuntimePage);
  assert(&bank.selectForPage(1, 1, 0, 2) == &bank.empty());
  assert(std::memcmp(&before, &bank.select(1, 0, 2), sizeof(before)) == 0);
  std::puts("P2-PAGE D PASS: invalidation rejects stale runtime use without data loss");
}

}  // namespace

int main() {
  testUnpublishedBankFailsClosed();
  testPublishedPageSelectsAndWrongPageRejects();
  testInvalidIdentityPublicationIsFailureAtomic();
  testExplicitInvalidationPreventsStaleUseWithoutErasingMaterial();
  std::puts("PATTERN/PHRASE P2 page identity contract: PASS");
  return 0;
}
