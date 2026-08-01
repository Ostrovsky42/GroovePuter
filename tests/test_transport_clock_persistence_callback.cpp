#include <cassert>
#include <cstdint>

#include "src/midi/transport_clock_runtime.h"

using namespace GroovePuterMidi;

namespace {
uint32_t g_callbackCount = 0;
TransportClockSource g_lastSource = TransportClockSource::GroovePuterInternal;
bool g_lastFollow = true;

void onChanged(TransportClockSource source, bool followEnabled) {
    ++g_callbackCount;
    g_lastSource = source;
    g_lastFollow = followEnabled;
}
}  // namespace

int main() {
    TransportClockRuntime runtime;

    // Loading persisted state must not immediately rewrite storage.
    runtime.applyPersistedControl(TransportClockSource::SeqtrakExternal, false);
    assert(runtime.source() == TransportClockSource::SeqtrakExternal);
    assert(!runtime.externalFollowEnabled());
    assert(g_callbackCount == 0);

    runtime.setControlChangedCallback(&onChanged);

    // Idempotent setters do not write NVS again.
    runtime.setSource(TransportClockSource::SeqtrakExternal);
    runtime.setExternalFollowEnabled(false);
    assert(g_callbackCount == 0);

    runtime.setSource(TransportClockSource::GroovePuterInternal);
    assert(g_callbackCount == 1);
    assert(g_lastSource == TransportClockSource::GroovePuterInternal);
    assert(!g_lastFollow);

    runtime.setExternalFollowEnabled(true);
    assert(g_callbackCount == 2);
    assert(g_lastSource == TransportClockSource::GroovePuterInternal);
    assert(g_lastFollow);

    assert(runtime.toggleSource() == TransportClockSource::SeqtrakExternal);
    assert(g_callbackCount == 3);
    assert(g_lastSource == TransportClockSource::SeqtrakExternal);
    assert(g_lastFollow);

    assert(!runtime.toggleExternalFollowEnabled());
    assert(g_callbackCount == 4);
    assert(g_lastSource == TransportClockSource::SeqtrakExternal);
    assert(!g_lastFollow);

    return 0;
}
