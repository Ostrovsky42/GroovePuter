#include <cassert>
#include <cstring>

#include "src/ui/midi_output_mode_ui.h"

int main() {
    namespace OutputUi = GroovePuterUi::MidiOutputModeUi;

    assert(std::strcmp(OutputUi::outputModeName(false), "INTERNAL") == 0);
    assert(std::strcmp(OutputUi::outputModeName(true), "INT + MIDI") == 0);
    return 0;
}
