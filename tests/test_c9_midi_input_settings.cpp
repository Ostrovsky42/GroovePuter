#include <cassert>
#include <cstdint>

#include "src/midi/midi_input_settings.h"
#include "src/ui/midi_input_ui.h"

using namespace GroovePuterMidi;

namespace {
void defaultIsFailClosed() {
    const auto config = MidiInputSettings::defaultRoutingConfig();
    assert(!config.enabled);
    assert(config.channelMode == MidiInputChannelMode::Omni);
    assert(config.channel == 0u);
    assert(config.target == MidiInputTarget::SynthA);
}

void roundTripsRepresentativePolicies() {
    for (MidiInputTarget target : {MidiInputTarget::SynthA,
                                  MidiInputTarget::SynthB,
                                  MidiInputTarget::Drums}) {
        MidiInputRoutingConfig source{};
        source.enabled = true;
        source.channelMode = MidiInputChannelMode::Single;
        source.channel = 15u;
        source.target = target;
        MidiInputRoutingConfig decoded{};
        assert(MidiInputSettings::decodeRoutingConfig(
            MidiInputSettings::encodeRoutingConfig(source), decoded));
        assert(decoded.enabled == source.enabled);
        assert(decoded.channelMode == source.channelMode);
        assert(decoded.channel == source.channel);
        assert(decoded.target == source.target);
    }
}

void corruptionFailsClosed() {
    MidiInputRoutingConfig decoded{};
    decoded.enabled = true;
    decoded.target = MidiInputTarget::Drums;
    assert(!MidiInputSettings::decodeRoutingConfig(0u, decoded));
    assert(!decoded.enabled && decoded.target == MidiInputTarget::SynthA);

    MidiInputRoutingConfig valid{};
    const uint32_t word = MidiInputSettings::encodeRoutingConfig(valid);
    assert(!MidiInputSettings::decodeRoutingConfig(word | 0x00000100u, decoded));
    assert(!decoded.enabled);

    const uint32_t invalidTarget =
        (static_cast<uint32_t>(MidiInputSettings::kSettingsMagic) << 24u) |
        (static_cast<uint32_t>(MidiInputSettings::kSettingsVersion) << 16u) |
        0xC0u;
    assert(!MidiInputSettings::decodeRoutingConfig(invalidTarget, decoded));
    assert(decoded.target == MidiInputTarget::SynthA);
}

void uiCyclesAllPublicValues() {
    MidiInputRoutingConfig config{};
    config = GroovePuterUi::MidiInputUi::stepEnabled(config);
    assert(config.enabled);

    config = GroovePuterUi::MidiInputUi::stepChannel(config, -1);
    assert(config.channelMode == MidiInputChannelMode::Single);
    assert(config.channel == 15u);
    config = GroovePuterUi::MidiInputUi::stepChannel(config, 1);
    assert(config.channelMode == MidiInputChannelMode::Omni);

    config.target = MidiInputTarget::SynthA;
    config = GroovePuterUi::MidiInputUi::stepTarget(config, -1);
    assert(config.target == MidiInputTarget::Drums);
    config = GroovePuterUi::MidiInputUi::stepTarget(config, 1);
    assert(config.target == MidiInputTarget::SynthA);
}
}  // namespace

int main() {
    defaultIsFailClosed();
    roundTripsRepresentativePolicies();
    corruptionFailsClosed();
    uiCyclesAllPublicValues();
    return 0;
}
