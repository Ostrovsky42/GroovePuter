#include <cassert>
#include <cstring>

#include "src/ui/ui_status_chrome.h"

using namespace UI;

int main() {
    char line[64]{};

    setUiStatusBpm(128);
    UiStatusSnapshot pattern{};
    pattern.context = UiStatusContext::Genre;
    pattern.source = UiStatusSource::Pattern;
    pattern.state = UiStatusState::Play;
    pattern.bar = 3;
    pattern.totalBars = 4;
    pattern.clock = UiStatusClock::Internal;
    pattern.output = UiStatusOutput::InternalAndMidi;
    formatUiStatusLine(pattern, line, sizeof(line));
    assert(std::strcmp(line, "GEN PAT PLAY 128 BPM B3/4 INT BOTH") == 0);

    pattern.dirty = true;
    formatUiStatusLine(pattern, line, sizeof(line));
    assert(std::strcmp(line, "GEN PAT PLAY 128 BPM B3/4 INT BOTH *") == 0);
    pattern.dirty = false;

    setUiStatusBpm(96);
    UiStatusSnapshot smf{};
    smf.context = UiStatusContext::Player;
    smf.source = UiStatusSource::Smf;
    smf.state = UiStatusState::Armed;
    smf.bar = 8;
    smf.totalBars = 128;
    smf.clock = UiStatusClock::External;
    smf.output = UiStatusOutput::Midi;
    smf.liveMixLocked = true;
    formatUiStatusLine(smf, line, sizeof(line));
    assert(std::strcmp(line, "PLYR SMF ARM 96 BPM B8/128 EXT MIDI LM") == 0);

    setUiStatusBpm(0);
    UiStatusSnapshot safeDefaults{};
    safeDefaults.bar = 0;
    safeDefaults.totalBars = 0;
    formatUiStatusLine(safeDefaults, line, sizeof(line));
    assert(std::strstr(line, "1 BPM") != nullptr);
    assert(std::strstr(line, "B1/1") != nullptr);

    UiStatusSnapshot changed = pattern;
    assert(changed == pattern);
    changed.bpm = 129;
    assert(changed != pattern);
    changed = pattern;
    changed.bar = 4;
    assert(changed != pattern);

    char tiny[8]{};
    formatUiStatusLine(smf, tiny, sizeof(tiny));
    assert(tiny[sizeof(tiny) - 1] == '\0');

    return 0;
}
