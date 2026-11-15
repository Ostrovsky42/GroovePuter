// Macro -> CC link rules (src/performance/performance_macro_cc.h).
#include <cassert>
#include <cstdint>

#include "src/performance/performance_macro_cc.h"

using namespace GroovePuterPerformance;
using namespace GroovePuterPerformance::MacroCc;

int main() {
    const Macros::State full{100, 100, 100, 100};   // TENSION, ENERGY, SPACE, MUTATION
    int out[GroovePuterMidi::kControllerCount];

    // Off: no offsets at all, whatever the macros say.
    offsetsFor(full, Link::Off, 0, out);
    for (int i = 0; i < GroovePuterMidi::kControllerCount; ++i) assert(out[i] == 0);

    // A reaches SYNTH A only; B reaches SYNTH B only.
    offsetsFor(full, Link::A, 1, out);
    for (int i = 0; i < GroovePuterMidi::kControllerCount; ++i) assert(out[i] == 0);
    offsetsFor(full, Link::B, 0, out);
    for (int i = 0; i < GroovePuterMidi::kControllerCount; ++i) assert(out[i] == 0);

    // A at full macros: cutoff +24, resonance +12, reverb +40, delay +30; ENERGY -43 on EXPR.
    offsetsFor(full, Link::A, 0, out);
    assert(out[kIndexCutoff] == 24 && out[kIndexReso] == 12);
    assert(out[kIndexReverb] == 40 && out[kIndexDelay] == 30);
    assert(out[kIndexExpr] == 0);   // ENERGY +100 is not sent: only the negative side
    Macros::State quiet{0, -100, 0, 0};
    offsetsFor(quiet, Link::A, 0, out);
    assert(out[kIndexExpr] == -43);

    // Negative TENSION moves the cutoff down and leaves resonance alone.
    Macros::State dark{-100, 0, 0, 0};
    offsetsFor(dark, Link::B, 1, out);
    assert(out[kIndexCutoff] == -24 && out[kIndexReso] == 0);

    // ALL: SYNTH A gets half of SPACE, SYNTH B the full share.
    offsetsFor(full, Link::All, 0, out);
    assert(out[kIndexReverb] == 20 && out[kIndexDelay] == 15);
    offsetsFor(full, Link::All, 1, out);
    assert(out[kIndexReverb] == 40 && out[kIndexDelay] == 30);

    // Mode cycle: OFF -> A -> B -> ALL -> OFF.
    assert(nextLink(Link::Off) == Link::A && nextLink(Link::A) == Link::B);
    assert(nextLink(Link::B) == Link::All && nextLink(Link::All) == Link::Off);
    return 0;
}
