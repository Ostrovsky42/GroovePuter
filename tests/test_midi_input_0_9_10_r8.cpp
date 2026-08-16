#include <cassert>
#include <cstring>
#include <iostream>

#include "src/ui/midi_input_ui.h"

int main() {
    using namespace GroovePuterUi::MidiInputUi;

    MidiInputRoutingConfig config{};
    assert(std::strcmp(enabledName(config.enabled), "OFF") == 0);
    config = stepEnabled(config);
    assert(config.enabled);
    assert(std::strcmp(enabledName(config.enabled), "ON") == 0);

    char channel[8]{};
    formatChannel(config, channel, sizeof(channel));
    assert(std::strcmp(channel, "OMNI") == 0);

    config = stepChannel(config, 1);
    assert(config.channelMode == MidiInputChannelMode::Single);
    assert(config.channel == 0u);
    formatChannel(config, channel, sizeof(channel));
    assert(std::strcmp(channel, "1") == 0);

    for (int i = 0; i < 15; ++i) config = stepChannel(config, 1);
    assert(config.channel == 15u);
    formatChannel(config, channel, sizeof(channel));
    assert(std::strcmp(channel, "16") == 0);
    config = stepChannel(config, 1);
    assert(config.channelMode == MidiInputChannelMode::Omni);
    config = stepChannel(config, -1);
    assert(config.channelMode == MidiInputChannelMode::Single);
    assert(config.channel == 15u);

    config.target = MidiInputTarget::SynthA;
    assert(std::strcmp(targetName(config.target), "SYN A") == 0);
    config = stepTarget(config, 1);
    assert(config.target == MidiInputTarget::SynthB);
    assert(std::strcmp(targetName(config.target), "SYN B") == 0);
    config = stepTarget(config, 1);
    assert(config.target == MidiInputTarget::Drums);
    assert(std::strcmp(targetName(config.target), "DRUMS") == 0);
    config = stepTarget(config, 1);
    assert(config.target == MidiInputTarget::SynthA);
    config = stepTarget(config, -1);
    assert(config.target == MidiInputTarget::Drums);

    std::cout << "0.9.10 R8 MIDI input UI projection: PASS\n";
    return 0;
}
