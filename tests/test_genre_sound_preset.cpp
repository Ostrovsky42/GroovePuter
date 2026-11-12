// H7: the genre sound presets. Ten steps per genre (5 controllers x 2 voices), each
// value already clamped for its controller; an unknown genre gives no steps.
#include <cassert>
#include <cstdio>

#include "src/midi/genre_sound_preset.h"

using namespace GenreSound;

int main() {
    const uint8_t modes[] = {9, 13, 10, 0, 5, 6};  // House, UKG, Techno, Acid, Dub, Trip-Hop
    for (uint8_t mode : modes) {
        Step steps[kStepsPerGenre];
        const size_t n = stepsFor(mode, steps, kStepsPerGenre);
        assert(n == 10);
        for (size_t i = 0; i < n; ++i) {
            assert(steps[i].voice < kVoiceCount);
            assert(steps[i].controller < kPresetControllers);
            // The value passes the controller's clamp unchanged (it is already usable).
            assert(steps[i].value == GroovePuterMidi::clampForController(steps[i].controller, steps[i].value));
            assert(steps[i].value <= 127);
        }
        // Voice A first, controllers in order CUTOFF ATTACK RELEASE REVERB DELAY.
        for (uint8_t c = 0; c < kPresetControllers; ++c) {
            assert(steps[c].voice == 0 && steps[c].controller == c);
            assert(steps[kPresetControllers + c].voice == 1 && steps[kPresetControllers + c].controller == c);
        }
    }
    // Spot checks against the table.
    Step house[kStepsPerGenre];
    assert(stepsFor(9, house, kStepsPerGenre) == 10);
    assert(house[0].value == 70);   // A CUTOFF
    assert(house[3].value == 0);    // A REVERB
    assert(house[5].value == 90);   // B CUTOFF
    assert(house[8].value == 30);   // B REVERB
    Step dub[kStepsPerGenre];
    stepsFor(5, dub, kStepsPerGenre);
    assert(dub[9].value == 80);     // B DELAY, Dub = Reggae (5)
    // Unknown genre: no steps. Short buffer: no steps.
    Step any[kStepsPerGenre];
    assert(stepsFor(2, any, kStepsPerGenre) == 0);
    assert(stepsFor(9, any, kStepsPerGenre - 1) == 0);
    assert(presetFor(3) == nullptr && presetFor(9) != nullptr);
    std::printf("genre sound presets: OK (6 genres x 10 steps)\n");
    return 0;
}
