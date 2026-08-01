#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/midi/transport_clock_source.h"

using namespace GroovePuterMidi;

int main() {
    assert(normalizeTransportClockSource(0) ==
           TransportClockSource::GroovePuterInternal);
    assert(normalizeTransportClockSource(1) ==
           TransportClockSource::SeqtrakExternal);
    assert(normalizeTransportClockSource(255) ==
           TransportClockSource::GroovePuterInternal);

    assert(std::strcmp(
               transportClockSourceName(
                   TransportClockSource::GroovePuterInternal),
               "GP MASTER") == 0);
    assert(std::strcmp(
               transportClockSourceName(
                   TransportClockSource::SeqtrakExternal),
               "SEQ MASTER") == 0);

    assert(transportClockSourcePublishesOutboundClock(
        TransportClockSource::GroovePuterInternal));
    assert(!transportClockSourcePublishesOutboundClock(
        TransportClockSource::SeqtrakExternal));
    return 0;
}
