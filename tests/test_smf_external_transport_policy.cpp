#include <cassert>

#include "src/midi/smf_player_service.h"

using namespace GroovePuterMidi;

int main() {
    assert(smfExternalRelaunchMode(false, true) ==
           SmfExternalRelaunchMode::Normal);
    assert(smfExternalRelaunchMode(false, false) ==
           SmfExternalRelaunchMode::Normal);

    const auto seqtrakStart = smfExternalRelaunchMode(true, true);
    assert(seqtrakStart == SmfExternalRelaunchMode::Restart);
    assert(smfExternalRelaunchUsesBoundedPrefill(seqtrakStart));

    const auto midiContinue = smfExternalRelaunchMode(true, false);
    assert(midiContinue == SmfExternalRelaunchMode::Continue);
    assert(smfExternalRelaunchUsesBoundedPrefill(midiContinue));

    assert(!smfExternalRelaunchUsesBoundedPrefill(
        SmfExternalRelaunchMode::Normal));
    return 0;
}
