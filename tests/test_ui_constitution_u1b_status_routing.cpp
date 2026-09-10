#include <cassert>
#include <cstring>

#include "src/ui/ui_status_chrome.h"

using namespace UI;

int main() {
    static_assert(sizeof(UiStatusRouting) == 1,
                  "sequenced source + transport owner must stay one byte");
    static_assert(sizeof(UiStatusSnapshot) <= 16,
                  "U1B must not grow the tiny Cardputer status snapshot");

    const UiStatusRouting phraseSong{
        UiSequencedSource::Phrase,
        UiTransportOwner::Song,
    };
    assert(phraseSong.sequencedSource() == UiSequencedSource::Phrase);
    assert(phraseSong.transportOwner() == UiTransportOwner::Song);

    const UiStatusRouting patternCycle{
        UiSequencedSource::Pattern,
        UiTransportOwner::Cycle,
    };
    assert(patternCycle.sequencedSource() == UiSequencedSource::Pattern);
    assert(patternCycle.transportOwner() == UiTransportOwner::Cycle);
    assert(patternCycle != phraseSong);

    char line[64]{};

    // A screen with no applicable per-target source must not claim PATTERN as
    // global composition truth. Default cycle ownership is intentionally quiet.
    UiStatusSnapshot genre{};
    genre.context = UiStatusContext::Genre;
    genre.routing = UiStatusRouting{
        UiSequencedSource::NotApplicable,
        UiTransportOwner::Cycle,
    };
    genre.state = UiStatusState::Play;
    genre.bpm = 128;
    genre.bar = 3;
    genre.totalBars = 4;
    genre.clock = UiStatusClock::Internal;
    genre.output = UiStatusOutput::InternalAndMidi;
    formatUiStatusLine(genre, line, sizeof(line));
    assert(std::strcmp(line, "GEN PLAY 128 BPM B3/4 INT BOTH") == 0);

    // Pattern address remains the strongest compact Pattern identity for a
    // target that is actually sequenced by Pattern.
    UiStatusSnapshot synthPattern{};
    synthPattern.context = UiStatusContext::SynthB;
    synthPattern.routing = UiStatusRouting{
        UiSequencedSource::Pattern,
        UiTransportOwner::Cycle,
    };
    synthPattern.state = UiStatusState::Stop;
    synthPattern.bpm = 120;
    synthPattern.patternPage = 1;
    synthPattern.patternBank = 1;
    synthPattern.patternSlot = 6;
    formatUiStatusLine(synthPattern, line, sizeof(line));
    assert(std::strcmp(line, "S-B 2B7 STOP 120 BPM B1/1 INT [-]") == 0);

    // Phrase is target source truth and must stay visible independently from
    // transport ownership.
    UiStatusSnapshot synthPhrase{};
    synthPhrase.context = UiStatusContext::SynthA;
    synthPhrase.routing = UiStatusRouting{
        UiSequencedSource::Phrase,
        UiTransportOwner::Cycle,
    };
    synthPhrase.state = UiStatusState::Play;
    synthPhrase.bpm = 128;
    synthPhrase.bar = 2;
    synthPhrase.totalBars = 8;
    formatUiStatusLine(synthPhrase, line, sizeof(line));
    assert(std::strcmp(line, "S-A PHR PLAY 128 BPM B2/8 INT [-]") == 0);

    synthPhrase.routing = UiStatusRouting{
        UiSequencedSource::Phrase,
        UiTransportOwner::Song,
    };
    formatUiStatusLine(synthPhrase, line, sizeof(line));
    assert(std::strcmp(line, "S-A PHR SONG PLAY 128 BPM B2/8 INT [-]") == 0);

    synthPhrase.routing = UiStatusRouting{
        UiSequencedSource::Phrase,
        UiTransportOwner::Smf,
    };
    synthPhrase.state = UiStatusState::Armed;
    synthPhrase.clock = UiStatusClock::External;
    synthPhrase.output = UiStatusOutput::Midi;
    synthPhrase.liveMixLocked = true;
    formatUiStatusLine(synthPhrase, line, sizeof(line));
    assert(std::strcmp(line, "S-A PHR SMF ARM 128 BPM B2/8 EXT MIDI LM") == 0);

    // When no target sequenced source applies, non-default transport remains
    // observable rather than borrowing a source from some other track.
    UiStatusSnapshot songScreen{};
    songScreen.context = UiStatusContext::Song;
    songScreen.routing = UiStatusRouting{
        UiSequencedSource::NotApplicable,
        UiTransportOwner::Song,
    };
    songScreen.state = UiStatusState::Play;
    songScreen.bpm = 110;
    songScreen.bar = 5;
    songScreen.totalBars = 16;
    formatUiStatusLine(songScreen, line, sizeof(line));
    assert(std::strcmp(line, "SONG SONG PLAY 110 BPM B5/16 INT BOTH") == 0);

    UiStatusSnapshot player{};
    player.context = UiStatusContext::Player;
    player.routing = UiStatusRouting{
        UiSequencedSource::NotApplicable,
        UiTransportOwner::Smf,
    };
    player.state = UiStatusState::Armed;
    player.bpm = 96;
    player.bar = 8;
    player.totalBars = 128;
    player.clock = UiStatusClock::External;
    player.output = UiStatusOutput::Midi;
    formatUiStatusLine(player, line, sizeof(line));
    assert(std::strcmp(line, "PLYR SMF ARM 96 BPM B8/128 EXT MIDI") == 0);

    return 0;
}
