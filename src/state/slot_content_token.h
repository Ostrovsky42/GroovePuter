#ifndef GROOVEPUTER_SRC_STATE_SLOT_CONTENT_TOKEN_H
#define GROOVEPUTER_SRC_STATE_SLOT_CONTENT_TOKEN_H

#include <cstdint>
#include <cstring>

#include "../../scenes.h"

// PML-C: content token of one resident pattern slot across every lane the musician can edit
// (Synth A, Synth B, drum voices, drum automation, pattern groove). Field-wise, so padding and
// service bits (`unused`, including the Song ownership bit) never take part. Any edit changes it;
// an identical rewrite does not; it is reproduced exactly after a save/load (proved in tools/pml).
namespace GroovePuterMaterial {

inline uint64_t slotContentToken(const Scene& scene, int slot) {
  const int bank = slot / Bank<SynthPattern>::kPatterns;
  const int idx = slot % Bank<SynthPattern>::kPatterns;
  uint64_t x = 1469598103934665603ull;
  const auto h = [&x](uint64_t v) {
    x ^= v + 0x9e3779b97f4a7c15ull + (x << 6) + (x >> 2);
    x *= 1099511628211ull;
  };
  const auto hf = [&h](float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    h(bits);
  };
  const DrumPatternSet& d = scene.drumBanks[bank].patterns[idx];
  for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
    for (int s = 0; s < DrumPattern::kSteps; ++s) {
      const DrumStep& t = d.voices[v].steps[s];
      h((uint64_t(t.hit) << 1) | t.accent);
      h(t.velocity); h(uint8_t(t.timing)); h(t.fx); h(t.fxParam); h(t.probability);
    }
  }
  for (int l = 0; l < DrumPatternSet::kMaxLanes; ++l) {
    h(d.lanes[l].targetParam);
    h(d.lanes[l].nodeCount);
    for (int n = 0; n < d.lanes[l].nodeCount && n < AutomationLane::kMaxNodes; ++n) {
      h(d.lanes[l].nodes[n].step);
      hf(d.lanes[l].nodes[n].value);
      h(d.lanes[l].nodes[n].curveType);
    }
  }
  hf(d.groove.swing);
  hf(d.groove.humanize);
  for (const SynthPattern* sp : {&scene.synthABanks[bank].patterns[idx],
                                 &scene.synthBBanks[bank].patterns[idx]}) {
    for (int s = 0; s < SynthPattern::kSteps; ++s) {
      const SynthStep& t = sp->steps[s];
      h(uint8_t(t.note));
      h((uint64_t(t.slide) << 2) | (uint64_t(t.accent) << 1) | t.ghost);
      h(t.velocity); h(uint8_t(t.timing)); h(t.fx); h(t.fxParam); h(t.probability);
    }
  }
  return x;
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_SLOT_CONTENT_TOKEN_H
