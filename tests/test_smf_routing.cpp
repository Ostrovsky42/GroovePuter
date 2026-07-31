#include <cassert>

#include "src/midi/smf_routing.h"

using namespace GroovePuterMidi;

int main() {
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

    assert(routeSmfNote(SmfRoutingMode::Seqtrak, 0, 64).channel == 7);
    assert(routeSmfNote(SmfRoutingMode::Seqtrak, 1, 64).channel == 8);
    assert(routeSmfNote(SmfRoutingMode::Seqtrak, 2, 64).channel == 9);
    return 0;
}
