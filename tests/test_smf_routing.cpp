#include <cassert>

#include "src/midi/smf_routing.h"

using namespace GroovePuterMidi;

int main() {
    assert(applySmfVelocityBoost(0, 16) == 0);
    assert(applySmfVelocityBoost(64, 0) == 64);
    assert(applySmfVelocityBoost(64, 8) == 72);
    assert(applySmfVelocityBoost(120, 16) == 127);

    const SmfRoutedNote raw = routeSmfNote(SmfRoutingMode::Raw, 9, 36);
    assert(raw.channel == 9 && raw.note == 36);

    const SmfRoutedNote kick = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 36);
    const SmfRoutedNote snare = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 38);
    const SmfRoutedNote clap = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 39);
    const SmfRoutedNote closedHat = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 42);
    const SmfRoutedNote openHat = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 46);
    assert(kick.channel == 0 && kick.note == 60);
    assert(snare.channel == 1 && snare.note == 60);
    assert(clap.channel == 2 && clap.note == 60);
    assert(closedHat.channel == 3 && closedHat.note == 60);
    assert(openHat.channel == 4 && openHat.note == 60);

    const SmfRoutedNote synth1 = routeSmfNote(SmfRoutingMode::Seqtrak, 0, 64);
    const SmfRoutedNote synth2 = routeSmfNote(SmfRoutingMode::Seqtrak, 1, 64);
    const SmfRoutedNote dx = routeSmfNote(SmfRoutingMode::Seqtrak, 2, 64);
    const SmfRoutedNote sampler = routeSmfNote(SmfRoutingMode::Seqtrak, 3, 64);
    const SmfRoutedNote extraMelodic = routeSmfNote(SmfRoutingMode::Seqtrak, 14, 67);

    assert(synth1.channel == 7 && synth1.note == 64);
    assert(synth2.channel == 8 && synth2.note == 64);
    assert(dx.channel == 9 && dx.note == 64);
    assert(sampler.channel == 10 && sampler.note == 64);

    // DX is a dedicated CH10 destination, never the fallback bucket. Until
    // per-track CUSTOM routing exists, additional melodic channels use the
    // distinct SEQTRAK SAMPLER destination on CH11.
    assert(extraMelodic.channel == 10 && extraMelodic.note == 67);
    assert(extraMelodic.channel != dx.channel);

    return 0;
}
