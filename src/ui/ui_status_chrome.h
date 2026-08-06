#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "src/pattern/pattern_address.h"
#include "src/state/scene_revision.h"

namespace UI {

enum class UiStatusContext : uint8_t {
    Perform = 0,
    Player,
    Genre,
    Mode,
    Feel,
    Overview,
    SynthA,
    SynthB,
    Drums,
    SoundA,
    SoundB,
    Song,
    Project,
    Generator,
    Unknown,
};

enum class UiStatusSource : uint8_t {
    Pattern = 0,
    Song,
    Smf,
};

enum class UiStatusState : uint8_t {
    Stop = 0,
    Play,
    Pause,
    Armed,
    Loading,
    Error,
};

enum class UiStatusClock : uint8_t {
    Internal = 0,
    File,
    External,
};

enum class UiStatusOutput : uint8_t {
    InternalAndMidi = 0,
    // Compatibility aliases for the first A1 implementation. Pattern/Song
    // events fan out to the internal synth and the registered USB-MIDI sink.
    InternalAudio = InternalAndMidi,
    Both = InternalAndMidi,
    Midi = 1,
    Unknown = 2,
};

struct UiStatusSnapshot {
    UiStatusContext context{UiStatusContext::Unknown};
    UiStatusSource source{UiStatusSource::Pattern};
    UiStatusState state{UiStatusState::Stop};
    UiStatusClock clock{UiStatusClock::Internal};
    UiStatusOutput output{UiStatusOutput::InternalAndMidi};
    uint16_t bar{1};
    uint16_t totalBars{1};
    bool liveMixLocked{false};
    bool dirty{GroovePuterState::sceneDirty()};
    uint8_t patternPage{0xFF};
    uint8_t patternBank{0xFF};
    uint8_t patternSlot{0xFF};

    bool hasPatternAddress() const {
        return patternAddressFromParts(patternPage, patternBank, patternSlot).valid();
    }
};

static_assert(sizeof(UiStatusSnapshot) <= 16,
              "status chrome snapshot must stay tiny on Cardputer ADV");

inline bool operator==(const UiStatusSnapshot& lhs,
                       const UiStatusSnapshot& rhs) {
    return lhs.context == rhs.context &&
           lhs.source == rhs.source &&
           lhs.state == rhs.state &&
           lhs.clock == rhs.clock &&
           lhs.output == rhs.output &&
           lhs.bar == rhs.bar &&
           lhs.totalBars == rhs.totalBars &&
           lhs.liveMixLocked == rhs.liveMixLocked &&
           lhs.dirty == rhs.dirty &&
           lhs.patternPage == rhs.patternPage &&
           lhs.patternBank == rhs.patternBank &&
           lhs.patternSlot == rhs.patternSlot;
}

inline bool operator!=(const UiStatusSnapshot& lhs,
                       const UiStatusSnapshot& rhs) {
    return !(lhs == rhs);
}

inline const char* uiStatusContextToken(UiStatusContext context) {
    switch (context) {
        case UiStatusContext::Perform: return "KEY";
        case UiStatusContext::Player: return "PLYR";
        case UiStatusContext::Genre: return "GEN";
        case UiStatusContext::Mode: return "MODE";
        case UiStatusContext::Feel: return "FEEL";
        case UiStatusContext::Overview: return "OVR";
        case UiStatusContext::SynthA: return "S-A";
        case UiStatusContext::SynthB: return "S-B";
        case UiStatusContext::Drums: return "DRM";
        case UiStatusContext::SoundA: return "SAFX";
        case UiStatusContext::SoundB: return "SBFX";
        case UiStatusContext::Song: return "SONG";
        case UiStatusContext::Project: return "PROJ";
        case UiStatusContext::Generator: return "ADV";
        case UiStatusContext::Unknown: return "PAGE";
    }
    return "PAGE";
}

inline const char* uiStatusSourceToken(UiStatusSource source) {
    switch (source) {
        case UiStatusSource::Pattern: return "PAT";
        case UiStatusSource::Song: return "SONG";
        case UiStatusSource::Smf: return "SMF";
    }
    return "PAT";
}

inline const char* uiStatusStateToken(UiStatusState state) {
    switch (state) {
        case UiStatusState::Stop: return "STOP";
        case UiStatusState::Play: return "PLAY";
        case UiStatusState::Pause: return "PAUS";
        case UiStatusState::Armed: return "ARM";
        case UiStatusState::Loading: return "LOAD";
        case UiStatusState::Error: return "ERR";
    }
    return "STOP";
}

inline const char* uiStatusClockToken(UiStatusClock clock) {
    switch (clock) {
        case UiStatusClock::Internal: return "INT";
        case UiStatusClock::File: return "FILE";
        case UiStatusClock::External: return "EXT";
    }
    return "INT";
}

inline const char* uiStatusOutputToken(UiStatusOutput output) {
    switch (output) {
        case UiStatusOutput::InternalAndMidi: return "BOTH";
        case UiStatusOutput::Midi: return "MIDI";
        case UiStatusOutput::Unknown: return "OUT?";
    }
    return "OUT?";
}

inline void formatUiStatusLine(const UiStatusSnapshot& status,
                               char* destination,
                               std::size_t capacity) {
    if (destination == nullptr || capacity == 0) return;

    const unsigned bar = status.bar == 0 ? 1u : status.bar;
    const unsigned total = status.totalBars == 0 ? 1u : status.totalBars;
    char sourceOrAddress[12];
    if (status.source == UiStatusSource::Pattern && status.hasPatternAddress()) {
        formatPatternAddressParts(sourceOrAddress, sizeof(sourceOrAddress),
                                  status.patternPage,
                                  status.patternBank,
                                  status.patternSlot);
    } else {
        std::snprintf(sourceOrAddress, sizeof(sourceOrAddress), "%s",
                      uiStatusSourceToken(status.source));
    }

    std::snprintf(destination,
                  capacity,
                  "%s %s %s B%u/%u %s %s%s%s",
                  uiStatusContextToken(status.context),
                  sourceOrAddress,
                  uiStatusStateToken(status.state),
                  bar,
                  total,
                  uiStatusClockToken(status.clock),
                  uiStatusOutputToken(status.output),
                  status.liveMixLocked ? " LM" : "",
                  status.dirty ? " *" : "");
}

}  // namespace UI
