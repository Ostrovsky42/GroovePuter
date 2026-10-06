#include <cassert>
#include <cstdio>
#include <cstring>
#include "scenes.h"
#include "src/generation/migration/phrase_execution.h"

using namespace GroovePuterRhythm;

// Stable field serialization excludes C++ padding and covers all physical
// pattern fields. Compare this stream against the unmodified base executable.
void byte(uint8_t value) { std::putchar(value); }
void floatBytes(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  for (unsigned i = 0; i < 4; ++i) byte(static_cast<uint8_t>(bits >> (8 * i)));
}
void emit(const SynthPattern& pattern) {
  for (const auto& s : pattern.steps) {
    byte(s.note); byte(s.slide); byte(s.accent); byte(s.ghost); byte(s.unused);
    byte(s.velocity); byte(s.timing); byte(s.fx); byte(s.fxParam); byte(s.probability);
  }
}
void emit(const DrumPatternSet& pattern) {
  for (const auto& voice : pattern.voices) for (const auto& s : voice.steps) {
    byte(s.hit); byte(s.accent); byte(s.unused); byte(s.velocity);
    byte(s.timing); byte(s.fx); byte(s.fxParam); byte(s.probability);
  }
  for (const auto& lane : pattern.lanes) {
    byte(lane.targetParam); byte(lane.nodeCount);
    for (const auto& node : lane.nodes) {
      byte(node.step); floatBytes(node.value); byte(node.curveType);
    }
  }
  floatBytes(pattern.groove.swing); floatBytes(pattern.groove.humanize);
}

int main() {
  for (unsigned mode = 0; mode < kGenerativeModeCount; ++mode) {
    for (uint16_t identity : {uint16_t{0}, uint16_t{41}, uint16_t{819}}) {
      for (uint8_t bars : {uint8_t{1}, uint8_t{2}, uint8_t{4}, uint8_t{8}}) {
        GenreSettings settings{};
        settings.generativeMode = mode;
        settings.recipe = kBaseRecipeId;
        settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
        settings.rhythmArchetypeId = kNoArchetypeId;
        PhraseExecutionMaterializationSettings materialization{};
        materialization.level = RealizationLevel::P2Variation;
        materialization.feelProfile = FeelProfileId::Straight;
        materialization.tonalMaterializationEnabled = true;
        materialization.rootPitchClass = 0;
        materialization.scaleTypeValue = kScaleDorian;
        PhraseExecutionScratch scratch{};
        PreparedPhraseExecution prepared{};
        const auto status = preparePhraseExecution(settings, materialization, identity, bars, scratch, prepared);
        byte(mode); byte(identity); byte(identity >> 8); byte(bars); byte(static_cast<uint8_t>(status));
        if (status != PhraseExecutionStatus::Ready) continue;
        for (uint8_t bar = 0; bar < prepared.length.effectivePhraseBars; ++bar) {
          DrumPatternSet drums{};
          SynthPattern synthA{}, synthB{};
          for (int step = 0; step < SynthPattern::kSteps; ++step) {
            synthA.steps[step].note = 36 + step % 12;
            synthB.steps[step].note = 60 + step % 12;
          }
          const auto result = materializePreparedPhraseBar(prepared, bar, 40 + bar, drums, synthA, synthB);
          assert(result.status == StrongRhythmMigrationStatus::Applied);
          emit(drums); emit(synthA); emit(synthB);
        }
      }
    }
  }
}
