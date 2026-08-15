#include "midi_companion_settings.h"

namespace GroovePuterMidi {
namespace {

constexpr uint8_t clampChannel(int channel) {
    return static_cast<uint8_t>(
        channel < static_cast<int>(kMidiChannelMin)
            ? kMidiChannelMin
            : (channel > static_cast<int>(kMidiChannelMax)
                   ? kMidiChannelMax
                   : channel));
}

constexpr uint8_t clampNote(int note) {
    return static_cast<uint8_t>(
        note < static_cast<int>(kMidiNoteMin)
            ? kMidiNoteMin
            : (note > static_cast<int>(kMidiNoteMax)
                   ? kMidiNoteMax
                   : note));
}

constexpr uint16_t clampDrumGate(int gateMs) {
    return static_cast<uint16_t>(
        gateMs < static_cast<int>(kDrumGateMinMs)
            ? kDrumGateMinMs
            : (gateMs > static_cast<int>(kDrumGateMaxMs)
                   ? kDrumGateMaxMs
                   : gateMs));
}

constexpr DrumMidiRoute route(uint8_t channel, uint8_t note) {
    return DrumMidiRoute{true, channel, note};
}

std::array<DrumMidiRoute, kMidiDrumVoiceCount> seqtrakNativeDrumRoutes() {
    std::array<DrumMidiRoute, kMidiDrumVoiceCount> routes{};
    routes[drumVoiceIndex(MidiDrumVoice::Kick)] = route(0, 60);
    routes[drumVoiceIndex(MidiDrumVoice::Snare)] = route(1, 60);
    routes[drumVoiceIndex(MidiDrumVoice::ClosedHat)] = route(3, 60);
    routes[drumVoiceIndex(MidiDrumVoice::OpenHat)] = route(4, 60);
    routes[drumVoiceIndex(MidiDrumVoice::MidTom)] = route(5, 60);
    routes[drumVoiceIndex(MidiDrumVoice::HighTom)] = route(6, 60);
    routes[drumVoiceIndex(MidiDrumVoice::Rim)] = route(5, 60);
    routes[drumVoiceIndex(MidiDrumVoice::Clap)] = route(2, 60);
    return routes;
}

std::array<DrumMidiRoute, kMidiDrumVoiceCount> generalMidiDrumRoutes() {
    constexpr uint8_t kGeneralMidiDrumChannel = 9;
    std::array<DrumMidiRoute, kMidiDrumVoiceCount> routes{};
    routes[drumVoiceIndex(MidiDrumVoice::Kick)] = route(kGeneralMidiDrumChannel, 36);
    routes[drumVoiceIndex(MidiDrumVoice::Snare)] = route(kGeneralMidiDrumChannel, 38);
    routes[drumVoiceIndex(MidiDrumVoice::ClosedHat)] = route(kGeneralMidiDrumChannel, 42);
    routes[drumVoiceIndex(MidiDrumVoice::OpenHat)] = route(kGeneralMidiDrumChannel, 46);
    routes[drumVoiceIndex(MidiDrumVoice::MidTom)] = route(kGeneralMidiDrumChannel, 43);
    routes[drumVoiceIndex(MidiDrumVoice::HighTom)] = route(kGeneralMidiDrumChannel, 47);
    routes[drumVoiceIndex(MidiDrumVoice::Rim)] = route(kGeneralMidiDrumChannel, 37);
    routes[drumVoiceIndex(MidiDrumVoice::Clap)] = route(kGeneralMidiDrumChannel, 39);
    return routes;
}

std::array<DrumMidiRoute, kMidiDrumVoiceCount> genericMidiDrumRoutes() {
    std::array<DrumMidiRoute, kMidiDrumVoiceCount> routes{};
    for (DrumMidiRoute& drumRoute : routes) {
        drumRoute.enabled = false;
        drumRoute.channel = 0;
        drumRoute.note = 60;
    }
    return routes;
}

bool validProfile(MidiDeviceProfile profile) {
    switch (profile) {
        case MidiDeviceProfile::SeqtrakNative:
        case MidiDeviceProfile::GeneralMidi:
        case MidiDeviceProfile::Custom:
        case MidiDeviceProfile::GenericMidi:
            return true;
    }
    return false;
}

bool validLiveTarget(MidiLiveTarget target) {
    switch (target) {
        case MidiLiveTarget::SynthA:
        case MidiLiveTarget::SynthB:
        case MidiLiveTarget::Drums:
            return true;
    }
    return false;
}

bool validTransportClockSource(TransportClockSource source) {
    switch (source) {
        case TransportClockSource::GroovePuterInternal:
        case TransportClockSource::SeqtrakExternal:
            return true;
    }
    return false;
}

}  // namespace

const char* midiDeviceProfileName(MidiDeviceProfile profile) {
    switch (profile) {
        case MidiDeviceProfile::SeqtrakNative:
            return "SEQTRAK NATIVE";
        case MidiDeviceProfile::GeneralMidi:
            return "GENERAL MIDI";
        case MidiDeviceProfile::Custom:
            return "CUSTOM";
        case MidiDeviceProfile::GenericMidi:
            return "GENERIC MIDI";
    }
    return "UNKNOWN";
}

const char* midiDrumVoiceName(MidiDrumVoice voice) {
    switch (voice) {
        case MidiDrumVoice::Kick:
            return "KICK";
        case MidiDrumVoice::Snare:
            return "SNARE";
        case MidiDrumVoice::ClosedHat:
            return "CLOSED HAT";
        case MidiDrumVoice::OpenHat:
            return "OPEN HAT";
        case MidiDrumVoice::MidTom:
            return "MID TOM";
        case MidiDrumVoice::HighTom:
            return "HIGH TOM";
        case MidiDrumVoice::Rim:
            return "RIM";
        case MidiDrumVoice::Clap:
            return "CLAP";
    }
    return "UNKNOWN";
}

uint8_t zeroBasedMidiChannelFromUi(int channelOneBased) {
    if (channelOneBased <= 1) return 0;
    if (channelOneBased >= 16) return 15;
    return static_cast<uint8_t>(channelOneBased - 1);
}

uint8_t uiMidiChannelFromZeroBased(uint8_t zeroBasedChannel) {
    return static_cast<uint8_t>(clampChannel(zeroBasedChannel) + 1);
}

MidiOutputSettings makeDefaultMidiOutputSettings(MidiDeviceProfile profile) {
    MidiOutputSettings settings{};
    if (profile == MidiDeviceProfile::Custom || !validProfile(profile)) {
        applyMidiDeviceProfile(MidiDeviceProfile::SeqtrakNative, settings);
        settings.profile = MidiDeviceProfile::Custom;
        return settings;
    }
    applyMidiDeviceProfile(profile, settings);
    return settings;
}

void applyMidiDeviceProfile(MidiDeviceProfile profile,
                            MidiOutputSettings& settings) {
    if (profile == MidiDeviceProfile::Custom || !validProfile(profile)) {
        settings.profile = MidiDeviceProfile::Custom;
        sanitizeMidiOutputSettings(settings);
        return;
    }

    const bool enabled = settings.enabled;
    const bool liveEnabled = settings.liveEnabled;
    const bool patternSynthAEnabled = settings.patternSynthAEnabled;
    const bool patternSynthBEnabled = settings.patternSynthBEnabled;
    const bool drumsEnabled = settings.drumsEnabled;
    const MidiLiveTarget liveTarget = settings.liveTarget;
    const TransportClockSource transportClockSource =
        settings.transportClockSource;
    const bool externalFollowEnabled = settings.externalFollowEnabled;

    settings = MidiOutputSettings{};
    settings.enabled = enabled;
    settings.liveEnabled = liveEnabled;
    settings.patternSynthAEnabled = patternSynthAEnabled;
    settings.patternSynthBEnabled = patternSynthBEnabled;
    settings.drumsEnabled = drumsEnabled;
    settings.liveTarget = liveTarget;
    settings.transportClockSource = normalizeTransportClockSource(
        static_cast<uint8_t>(transportClockSource));
    settings.externalFollowEnabled = externalFollowEnabled;
    settings.profile = profile;
    settings.drumGateMs = kDefaultDrumGateMs;

    switch (profile) {
        case MidiDeviceProfile::SeqtrakNative:
            settings.liveChannel = 7;
            settings.synthAChannel = 7;
            settings.synthBChannel = 8;
            settings.drumRoutes = seqtrakNativeDrumRoutes();
            break;
        case MidiDeviceProfile::GeneralMidi:
            settings.liveChannel = 0;
            settings.synthAChannel = 0;
            settings.synthBChannel = 1;
            settings.drumRoutes = generalMidiDrumRoutes();
            break;
        case MidiDeviceProfile::GenericMidi:
            settings.liveChannel = 0;
            settings.synthAChannel = 0;
            settings.synthBChannel = 1;
            settings.drumRoutes = genericMidiDrumRoutes();
            break;
        case MidiDeviceProfile::Custom:
            break;
    }
}

bool isValidMidiOutputSettings(const MidiOutputSettings& settings) {
    if (!validProfile(settings.profile) ||
        !validLiveTarget(settings.liveTarget) ||
        !validTransportClockSource(settings.transportClockSource)) {
        return false;
    }
    if (settings.liveChannel > kMidiChannelMax ||
        settings.synthAChannel > kMidiChannelMax ||
        settings.synthBChannel > kMidiChannelMax) {
        return false;
    }
    if (settings.drumGateMs < kDrumGateMinMs ||
        settings.drumGateMs > kDrumGateMaxMs) {
        return false;
    }
    for (const DrumMidiRoute& drumRoute : settings.drumRoutes) {
        if (drumRoute.channel > kMidiChannelMax ||
            drumRoute.note > kMidiNoteMax) {
            return false;
        }
    }
    return true;
}

void sanitizeMidiOutputSettings(MidiOutputSettings& settings) {
    if (!validProfile(settings.profile)) {
        settings.profile = MidiDeviceProfile::Custom;
    }
    if (!validLiveTarget(settings.liveTarget)) {
        settings.liveTarget = MidiLiveTarget::SynthA;
    }
    settings.transportClockSource = normalizeTransportClockSource(
        static_cast<uint8_t>(settings.transportClockSource));
    settings.liveChannel = clampChannel(settings.liveChannel);
    settings.synthAChannel = clampChannel(settings.synthAChannel);
    settings.synthBChannel = clampChannel(settings.synthBChannel);
    settings.drumGateMs = clampDrumGate(settings.drumGateMs);
    for (DrumMidiRoute& drumRoute : settings.drumRoutes) {
        drumRoute.channel = clampChannel(drumRoute.channel);
        drumRoute.note = clampNote(drumRoute.note);
    }
}

bool operator==(const DrumMidiRoute& lhs, const DrumMidiRoute& rhs) {
    return lhs.enabled == rhs.enabled && lhs.channel == rhs.channel &&
           lhs.note == rhs.note;
}

bool operator==(const MidiOutputSettings& lhs,
                const MidiOutputSettings& rhs) {
    return lhs.profile == rhs.profile && lhs.enabled == rhs.enabled &&
           lhs.liveEnabled == rhs.liveEnabled &&
           lhs.patternSynthAEnabled == rhs.patternSynthAEnabled &&
           lhs.patternSynthBEnabled == rhs.patternSynthBEnabled &&
           lhs.drumsEnabled == rhs.drumsEnabled &&
           lhs.liveTarget == rhs.liveTarget &&
           lhs.liveChannel == rhs.liveChannel &&
           lhs.synthAChannel == rhs.synthAChannel &&
           lhs.synthBChannel == rhs.synthBChannel &&
           lhs.drumRoutes == rhs.drumRoutes &&
           lhs.drumGateMs == rhs.drumGateMs &&
           lhs.transportClockSource == rhs.transportClockSource &&
           lhs.externalFollowEnabled == rhs.externalFollowEnabled;
}

}  // namespace GroovePuterMidi
