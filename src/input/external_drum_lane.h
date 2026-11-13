#ifndef GROOVEPUTER_EXTERNAL_DRUM_LANE_H
#define GROOVEPUTER_EXTERNAL_DRUM_LANE_H

#include <cstdint>

namespace GroovePuterInput {

// One note-class -> SEQTRAK drum lane layout for every external keyboard path
// (PERFORM, Host MIDI IN dispatcher, USB output thru). Pure, no state.
inline bool externalDrumLaneForNote(uint8_t note, uint8_t& zeroBasedLane) {
    switch (note % 12) {
        case 0: case 1: zeroBasedLane = 0; return true;              // kick
        case 2: zeroBasedLane = 1; return true;                      // snare
        case 3: case 4: zeroBasedLane = 2; return true;              // clap
        case 5: case 6: zeroBasedLane = 3; return true;              // hat 1
        case 7: case 8: case 10: zeroBasedLane = 4; return true;     // hat 2 / open hat
        case 9: zeroBasedLane = 5; return true;                      // perc 1
        case 11: zeroBasedLane = 6; return true;                     // perc 2
        default: return false;
    }
}

}  // namespace GroovePuterInput

#endif  // GROOVEPUTER_EXTERNAL_DRUM_LANE_H
