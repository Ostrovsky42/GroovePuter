#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "src/output/output_ownership.h"
#include "src/pattern/pattern_address.h"
#include "src/state/scene_revision.h"
#include "ui_location.h"

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
    Phrase,
    PhraseCore,
    Unknown,
};

// U1B: the target's sequenced source and the transport/playback owner are
// independent semantic axes. They remain packed into one byte so the global
// status snapshot does not grow on Cardputer ADV.
enum class UiSequencedSource : uint8_t {
    NotApplicable = 0,
    Pattern,
    Phrase,
};

enum class UiTransportOwner : uint8_t {
    Cycle = 0,
    Song,
    Smf,
};

struct UiStatusRouting {
    uint8_t value{0};

    constexpr UiStatusRouting(
        UiSequencedSource sequencedSource = UiSequencedSource::NotApplicable,
        UiTransportOwner transportOwner = UiTransportOwner::Cycle)
        : value(static_cast<uint8_t>(
              (static_cast<uint8_t>(sequencedSource) & 0x03u) |
              ((static_cast<uint8_t>(transportOwner) & 0x03u) << 2u))) {}

    constexpr UiSequencedSource sequencedSource() const {
        return static_cast<UiSequencedSource>(value & 0x03u);
    }

    constexpr UiTransportOwner transportOwner() const {
        return static_cast<UiTransportOwner>((value >> 2u) & 0x03u);
    }

    constexpr bool operator==(const UiStatusRouting& other) const {
        return value == other.value;
    }

    constexpr bool operator!=(const UiStatusRouting& other) const {
        return !(*this == other);
    }
};

static_assert(sizeof(UiStatusRouting) == 1,
              "status routing must remain one byte on Cardputer ADV");

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
    InternalAudio = InternalAndMidi,
    Both = InternalAndMidi,
    Midi = 1,
    Unknown = 2,
    Internal = 3,
    Layer = 4,
    Legacy = 5,
    TrackMidi = 6,
};

inline UiStatusContext uiStatusContextForLocation(const UiLocation& location) {
    switch (location.target) {
        case UiTarget::Performance:
            return UiStatusContext::Perform;
        case UiTarget::MidiPlayer:
            return UiStatusContext::Player;
        case UiTarget::Generation:
            return location.surface == UiSurface::Feel
                ? UiStatusContext::Feel
                : UiStatusContext::Genre;
        case UiTarget::Overview:
            return UiStatusContext::Overview;
        case UiTarget::SynthA:
            return UiStatusContext::SynthA;
        case UiTarget::SynthB:
            return UiStatusContext::SynthB;
        case UiTarget::Drums:
            return UiStatusContext::Drums;
        case UiTarget::Song:
            return UiStatusContext::Song;
        case UiTarget::Phrase:
            return UiStatusContext::Phrase;
        case UiTarget::PhraseCore:
            return UiStatusContext::PhraseCore;
        case UiTarget::Project:
            return UiStatusContext::Project;
    }
    return UiStatusContext::Unknown;
}

inline uint16_t normalizeUiStatusBpm(int bpm) {
    if (bpm < 1) return 1;
    if (bpm > 999) return 999;
    return static_cast<uint16_t>(bpm);
}

inline uint16_t& uiStatusBpmStorage() {
    static uint16_t bpm = 120;
    return bpm;
}

inline void setUiStatusBpm(int bpm) {
    uiStatusBpmStorage() = normalizeUiStatusBpm(bpm);
}

inline uint16_t uiStatusBpm() {
    return uiStatusBpmStorage();
}

// Keep the existing one-byte `dirty` member API while also carrying a tiny
// revision fingerprint. OutputMode changes mark the Scene mutated; encoding the
// low revision bits here makes the cached status line refresh on every explicit
// mode transition, not only on the first clean -> dirty transition.
struct UiStatusDirtyStamp {
    uint8_t value{0};

    UiStatusDirtyStamp() {
        const GroovePuterState::SceneRevisionState revision =
            GroovePuterState::sceneRevisionSnapshot();
        value = static_cast<uint8_t>(
            ((revision.currentRevision & 0x7Fu) << 1u) |
            (revision.dirty() ? 1u : 0u));
    }

    UiStatusDirtyStamp& operator=(bool isDirty) {
        value = static_cast<uint8_t>((value & 0xFEu) | (isDirty ? 1u : 0u));
        return *this;
    }

    operator bool() const {
        return (value & 1u) != 0u;
    }

    bool operator==(const UiStatusDirtyStamp& other) const {
        return value == other.value;
    }

    bool operator!=(const UiStatusDirtyStamp& other) const {
        return !(*this == other);
    }
};

static_assert(sizeof(UiStatusDirtyStamp) == 1,
              "status dirty/revision stamp must remain one byte");

struct UiStatusSnapshot {
    UiStatusContext context{UiStatusContext::Unknown};
    UiStatusRouting routing{};
    UiStatusState state{UiStatusState::Stop};
    UiStatusClock clock{UiStatusClock::Internal};
    UiStatusOutput output{UiStatusOutput::InternalAndMidi};
    bool liveMixLocked{false};
    UiStatusDirtyStamp dirty{};
    uint8_t patternPage{0xFF};
    uint8_t patternBank{0xFF};
    uint8_t patternSlot{0xFF};
    uint16_t bpm{uiStatusBpm()};
    uint16_t bar{1};
    uint16_t totalBars{1};

    bool hasPatternAddress() const {
        return patternAddressFromParts(patternPage, patternBank, patternSlot).valid();
    }
};

static_assert(sizeof(UiStatusSnapshot) <= 16,
              "status chrome snapshot must stay tiny on Cardputer ADV");

inline bool operator==(const UiStatusSnapshot& lhs,
                       const UiStatusSnapshot& rhs) {
    return lhs.context == rhs.context &&
           lhs.routing == rhs.routing &&
           lhs.state == rhs.state &&
           lhs.clock == rhs.clock &&
           lhs.output == rhs.output &&
           lhs.liveMixLocked == rhs.liveMixLocked &&
           lhs.dirty == rhs.dirty &&
           lhs.patternPage == rhs.patternPage &&
           lhs.patternBank == rhs.patternBank &&
           lhs.patternSlot == rhs.patternSlot &&
           lhs.bpm == rhs.bpm &&
           lhs.bar == rhs.bar &&
           lhs.totalBars == rhs.totalBars;
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
        case UiStatusContext::Phrase: return "PHR";
        case UiStatusContext::PhraseCore: return "PCOR";
        case UiStatusContext::Unknown: return "PAGE";
    }
    return "PAGE";
}

inline const char* uiSequencedSourceToken(UiSequencedSource source) {
    switch (source) {
        case UiSequencedSource::NotApplicable: return "";
        case UiSequencedSource::Pattern: return "PAT";
        case UiSequencedSource::Phrase: return "PHR";
    }
    return "";
}

inline const char* uiTransportOwnerToken(UiTransportOwner owner) {
    switch (owner) {
        case UiTransportOwner::Cycle: return "";
        case UiTransportOwner::Song: return "SONG";
        case UiTransportOwner::Smf: return "SMF";
    }
    return "";
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
        case UiStatusOutput::Internal: return "[I]";
        case UiStatusOutput::Layer: return "[L]";
        case UiStatusOutput::Legacy: return "[-]";
        case UiStatusOutput::TrackMidi: return "[M]";
    }
    return "OUT?";
}

inline UiStatusOutput uiStatusCanonicalTrackOutput(
    const UiStatusSnapshot& status) {
    // SMF owns an external-only playback path independently from the logical
    // GroovePuter track owner. Preserve the transport snapshot in that case.
    if (status.routing.transportOwner() == UiTransportOwner::Smf) {
        return status.output;
    }

    GroovePuterOutput::Track track = GroovePuterOutput::Track::Count;
    switch (status.context) {
        case UiStatusContext::SynthA:
        case UiStatusContext::SoundA:
            track = GroovePuterOutput::Track::SynthA;
            break;
        case UiStatusContext::SynthB:
        case UiStatusContext::SoundB:
            track = GroovePuterOutput::Track::SynthB;
            break;
        case UiStatusContext::Drums:
            track = GroovePuterOutput::Track::Drums;
            break;
        default:
            return status.output;
    }

    if (!GroovePuterOutput::hasExplicitMode(track)) {
        return UiStatusOutput::Legacy;
    }

    switch (GroovePuterOutput::mode(track)) {
        case GroovePuterOutput::Mode::Internal:
            return UiStatusOutput::Internal;
        case GroovePuterOutput::Mode::Midi:
            return UiStatusOutput::TrackMidi;
        case GroovePuterOutput::Mode::Layer:
            return UiStatusOutput::Layer;
    }
    return UiStatusOutput::Unknown;
}

inline void formatUiStatusLine(const UiStatusSnapshot& status,
                               char* destination,
                               std::size_t capacity) {
    if (destination == nullptr || capacity == 0) return;

    const unsigned bpm = status.bpm == 0 ? 1u : status.bpm;
    const unsigned bar = status.bar == 0 ? 1u : status.bar;
    const unsigned total = status.totalBars == 0 ? 1u : status.totalBars;

    char route[20]{};
    char source[12]{};
    const UiSequencedSource sequencedSource = status.routing.sequencedSource();
    if (sequencedSource == UiSequencedSource::Pattern && status.hasPatternAddress()) {
        formatPatternAddressParts(source, sizeof(source),
                                  status.patternPage,
                                  status.patternBank,
                                  status.patternSlot);
    } else {
        std::snprintf(source, sizeof(source), "%s",
                      uiSequencedSourceToken(sequencedSource));
    }

    const char* transport = uiTransportOwnerToken(status.routing.transportOwner());
    if (source[0] != '\0' && transport[0] != '\0') {
        std::snprintf(route, sizeof(route), "%s %s", source, transport);
    } else if (source[0] != '\0') {
        std::snprintf(route, sizeof(route), "%s", source);
    } else if (transport[0] != '\0') {
        std::snprintf(route, sizeof(route), "%s", transport);
    }

    std::snprintf(destination,
                  capacity,
                  "%s%s%s %s %u BPM B%u/%u %s %s%s%s",
                  uiStatusContextToken(status.context),
                  route[0] != '\0' ? " " : "",
                  route,
                  uiStatusStateToken(status.state),
                  bpm,
                  bar,
                  total,
                  uiStatusClockToken(status.clock),
                  uiStatusOutputToken(uiStatusCanonicalTrackOutput(status)),
                  status.liveMixLocked ? " LM" : "",
                  status.dirty ? " *" : "");
}

}  // namespace UI
