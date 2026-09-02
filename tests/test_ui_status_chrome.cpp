#include <cassert>
#include <cstring>

#include "src/ui/ui_active_page_title.h"
#include "src/ui/ui_status_chrome.h"

using namespace UI;

int main() {
    char line[64]{};

    publishActivePageTitle("SYNTH A TB303 P2");
    assert(std::strcmp(activePageTitle(), "SYNTH A TB303 P2") == 0);

    setUiStatusBpm(128);
    assert(uiStatusBpm() == 128);
    assert(normalizeUiStatusBpm(0) == 1);
    assert(normalizeUiStatusBpm(1000) == 999);

    UiStatusSnapshot pattern{};
    pattern.context = UiStatusContext::Genre;
    pattern.source = UiStatusSource::Pattern;
    pattern.state = UiStatusState::Play;
    pattern.bpm = 128;
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

    constexpr GroovePuterOutput::Track synthBTrack =
        GroovePuterOutput::Track::SynthB;
    GroovePuterOutput::restoreLegacyCompatibility(synthBTrack);

    UiStatusSnapshot synth{};
    synth.context = UiStatusContext::SynthB;
    synth.source = UiStatusSource::Pattern;
    synth.state = UiStatusState::Stop;
    synth.bpm = 120;
    synth.patternPage = 1;
    synth.patternBank = 1;
    synth.patternSlot = 6;
    formatUiStatusLine(synth, line, sizeof(line));
    assert(std::strcmp(line, "S-B 2B7 STOP 120 BPM B1/1 INT [-]") == 0);
    assert(synth.hasPatternAddress());

    assert(GroovePuterOutput::setMode(
        synthBTrack, GroovePuterOutput::Mode::Internal));
    formatUiStatusLine(synth, line, sizeof(line));
    assert(std::strcmp(line, "S-B 2B7 STOP 120 BPM B1/1 INT [I]") == 0);

    assert(GroovePuterOutput::setMode(
        synthBTrack, GroovePuterOutput::Mode::Midi));
    formatUiStatusLine(synth, line, sizeof(line));
    assert(std::strcmp(line, "S-B 2B7 STOP 120 BPM B1/1 INT [M]") == 0);

    assert(GroovePuterOutput::setMode(
        synthBTrack, GroovePuterOutput::Mode::Layer));
    formatUiStatusLine(synth, line, sizeof(line));
    assert(std::strcmp(line, "S-B 2B7 STOP 120 BPM B1/1 INT [L]") == 0);

    GroovePuterOutput::restoreLegacyCompatibility(synthBTrack);

    UiStatusSnapshot invalidAddress = synth;
    invalidAddress.patternBank = 0xFF;
    formatUiStatusLine(invalidAddress, line, sizeof(line));
    assert(std::strcmp(line, "S-B PAT STOP 120 BPM B1/1 INT [-]") == 0);
    assert(!invalidAddress.hasPatternAddress());

    UiStatusSnapshot smf{};
    smf.context = UiStatusContext::Player;
    smf.source = UiStatusSource::Smf;
    smf.state = UiStatusState::Armed;
    smf.bpm = 96;
    smf.bar = 8;
    smf.totalBars = 128;
    smf.clock = UiStatusClock::External;
    smf.output = UiStatusOutput::Midi;
    smf.liveMixLocked = true;
    formatUiStatusLine(smf, line, sizeof(line));
    assert(std::strcmp(line, "PLYR SMF ARM 96 BPM B8/128 EXT MIDI LM") == 0);
    assert(std::strlen(line) < sizeof(line));

    UiStatusSnapshot safeDefaults{};
    safeDefaults.bpm = 0;
    safeDefaults.bar = 0;
    safeDefaults.totalBars = 0;
    formatUiStatusLine(safeDefaults, line, sizeof(line));
    assert(std::strstr(line, "1 BPM B1/1") != nullptr);

    UiStatusSnapshot changed = pattern;
    assert(changed == pattern);
    changed.bpm = 129;
    assert(changed != pattern);
    changed = pattern;
    changed.bar = 4;
    assert(changed != pattern);
    changed = synth;
    assert(changed == synth);
    changed.patternSlot = 7;
    assert(changed != synth);

    char tiny[8]{};
    formatUiStatusLine(smf, tiny, sizeof(tiny));
    assert(tiny[sizeof(tiny) - 1] == '\0');

    return 0;
}
