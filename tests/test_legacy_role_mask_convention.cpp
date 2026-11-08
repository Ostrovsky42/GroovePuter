// P05: legacy role-mask convention (bit = step) and bass anchor accents.
#include "src/dsp/miniacid_engine.h"
#include "src/dsp/mode_manager.h"
#include "src/generation/migration/quantized_generation_commit_impl.h"

#include <cassert>
#include <cstdio>

SerialMock Serial;
SDMock SD;

int main() {
  MiniAcid engine{44100.0f, nullptr};
  GrooveboxModeManager mm(engine);

  // (a) Reggae lead: stepAllowed() is bit = step, so the lead mask must be
  // 0x4444 (steps 2/6/10/14), not 0xAAAA (odd 16ths).
  {
    GenreBehavior bass = GenreCatalog::behavior(GenreSettings{});
    GenreBehavior lead = bass;
    GroovePuterRhythm::QuantizedGenerationDetail::applyLegacyRoleSplit(GenerativeMode::Reggae, bass, lead);
    assert(lead.stepMask == 0x4444);
    GenerativeParams params;
    params.minNotes = 8;
    params.maxNotes = 12;
    int total = 0;
    for (int seed = 1; seed <= 40; ++seed) {
      mm.setGenerationSeed((uint32_t)seed);
      SynthPattern p{};
      mm.generatePattern(p, 120.0f, params, lead, 1);
      for (int s = 0; s < 16; ++s) {
        if (p.steps[s].note == -1) continue;
        ++total;
        assert(s == 2 || s == 6 || s == 10 || s == 14);
      }
    }
    assert(total > 0);
    std::printf("reggae lead onsets checked: %d\n", total);
  }

  // (b) Bass anchors on steps 0/8: accent only when accents are allowed.
  {
    GenreBehavior b = {0xFFFF, 4, 0, true, false, false, false};
    GenerativeParams params;
    params.minNotes = 6;
    params.maxNotes = 6;
    mm.setGenerationSeed(7);
    SynthPattern p{};
    params.noAccents = true;
    params.accentProbability = 1.0f;
    mm.generatePattern(p, 120.0f, params, b, 0);
    assert(p.steps[0].note != -1 && p.steps[8].note != -1);
    assert(!p.steps[0].accent && !p.steps[8].accent);

    params.noAccents = false;
    mm.generatePattern(p, 120.0f, params, b, 0);
    assert(p.steps[0].accent && p.steps[8].accent);
  }
  std::puts("test_legacy_role_mask_convention: PASS");
  return 0;
}
