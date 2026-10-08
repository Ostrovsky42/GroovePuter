#include <cassert>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/tonal_profile.h"

using namespace GroovePuterRhythm;

int main() {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
  const auto base = tonalGenerationProfileFor(settings);
  assert((base.melodicPolicy.allowedContours &
      melodicContourBit(MelodicContourId::MotifAnswer)) != 0);
  for (uint8_t recipe : {6, 7}) {
    settings.recipe = recipe;
    const auto atlas = tonalGenerationProfileFor(settings);
    assert((atlas.melodicPolicy.allowedContours &
        melodicContourBit(MelodicContourId::MotifAnswer)) == 0);
    assert(atlas.melodicPolicy.preferredContours == (
        melodicContourBit(MelodicContourId::Neighbor) |
        melodicContourBit(MelodicContourId::RepeatThenUp) |
        melodicContourBit(MelodicContourId::LeapReturn) |
        melodicContourBit(MelodicContourId::StepUp)));
    assert(atlas.melodicPolicy.allowedMotifOperations ==
        melodicMotifOperationBit(MelodicMotifOperationId::None));
  }
}
