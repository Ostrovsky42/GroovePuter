#include <cassert>
#include <cstring>

#include "src/ui/midi_file_name_policy.h"

int main() {
    using namespace GroovePuterUi;

    assert(midiFilenameIsVisibleAndSupported("beat.mid"));
    assert(midiFilenameIsVisibleAndSupported("BEAT.MID"));
    assert(!midiFilenameIsVisibleAndSupported(".hidden.mid"));
    assert(!midiFilenameIsVisibleAndSupported("beat.midi"));
    assert(!midiFilenameIsVisibleAndSupported("beat.txt"));

    char renamed[64]{};
    assert(buildMidiFilenameFromStem("  New Beat  ", renamed, sizeof(renamed)));
    assert(std::strcmp(renamed, "New Beat.mid") == 0);
    assert(buildMidiFilenameFromStem("Track_01.MID", renamed, sizeof(renamed)));
    assert(std::strcmp(renamed, "Track_01.mid") == 0);
    assert(!buildMidiFilenameFromStem("../escape", renamed, sizeof(renamed)));
    assert(!buildMidiFilenameFromStem("bad/name", renamed, sizeof(renamed)));
    assert(!buildMidiFilenameFromStem("   ", renamed, sizeof(renamed)));
    assert(!buildMidiFilenameFromStem(".mid", renamed, sizeof(renamed)));
    return 0;
}
