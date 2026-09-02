#pragma once

#include <cstdint>

namespace GroovePuterMidi {

enum class SmfRoutingMode : uint8_t {
    Seqtrak = 0,
    Raw,
};

struct SmfRoutedNote {
    uint8_t channel{0};
    uint8_t note{0};
    bool mapped{true};
};

inline constexpr uint8_t applySmfVelocityBoost(uint8_t velocity,
                                                uint8_t boost) {
    if (velocity == 0) return 0;
    const uint16_t boosted = static_cast<uint16_t>(velocity) + boost;
    return static_cast<uint8_t>(boosted > 127u ? 127u : boosted);
}

inline constexpr uint8_t nextSmfVelocityBoost(uint8_t current) {
    switch (current) {
        case 0: return 8;
        case 8: return 16;
        case 16: return 24;
        case 24: return 32;
        case 32: return 48;
        default: return 0;
    }
}

inline constexpr SmfRoutedNote routeSmfNote(SmfRoutingMode mode,
                                             uint8_t sourceChannel,
                                             uint8_t sourceNote) {
    sourceChannel = sourceChannel > 15 ? 15 : sourceChannel;
    if (mode == SmfRoutingMode::Raw) {
        return SmfRoutedNote{sourceChannel, sourceNote};
    }

    // General MIDI drums use CH10. SEQTRAK Native uses one fixed-note track
    // per drum role on CH1..7.
    if (sourceChannel == 9) {
        switch (sourceNote) {
            case 35:
            case 36: return SmfRoutedNote{0, 60};  // kick
            case 38:
            case 40: return SmfRoutedNote{1, 60};  // snare
            case 39: return SmfRoutedNote{2, 60};  // clap
            case 42:
            case 44: return SmfRoutedNote{3, 60};  // closed/pedal hat
            case 46: return SmfRoutedNote{4, 60};  // open hat
            case 47:
            case 48:
            case 50:
            case 49:
            case 51:
            case 52:
            case 53:
            case 55:
            case 57:
            case 59: return SmfRoutedNote{6, 60};  // high tom/cymbal
            default: return SmfRoutedNote{5, 60};  // tom/percussion
        }
    }

    // SEQTRAK melodic destinations are distinct instruments. DX is never a
    // generic catch-all: source CH1/CH2/CH3 explicitly select SYNTH1/SYNTH2/DX.
    // Extra source channels have no deterministic SEQTRAK destination. Drop
    // them before the scheduler queue instead of spending USB bandwidth on an
    // unused channel. A per-track override can expose them explicitly.
    if (sourceChannel == 0) return SmfRoutedNote{7, sourceNote};   // SYNTH1
    if (sourceChannel == 1) return SmfRoutedNote{8, sourceNote};   // SYNTH2
    if (sourceChannel == 2) return SmfRoutedNote{9, sourceNote};   // DX
    return SmfRoutedNote{0, sourceNote, false};                    // UNMAPPED
}

inline constexpr SmfRoutedNote routeSmfNoteToSeqtrakDestination(
        int8_t destinationChannel,
        uint8_t sourceNote) {
    if (destinationChannel < 0 || destinationChannel > 9) {
        return SmfRoutedNote{0, sourceNote, false};
    }
    const uint8_t channel = static_cast<uint8_t>(destinationChannel);
    const uint8_t note = channel <= 6u ? 60u : sourceNote;
    return SmfRoutedNote{channel, note};
}

inline constexpr SmfRoutedNote routeSmfTrackNote(
        SmfRoutingMode mode,
        uint8_t sourceChannel,
        uint8_t sourceNote,
        int8_t destinationOverride) {
    if (mode == SmfRoutingMode::Seqtrak && destinationOverride >= 0) {
        return routeSmfNoteToSeqtrakDestination(destinationOverride, sourceNote);
    }
    return routeSmfNote(mode, sourceChannel, sourceNote);
}

}  // namespace GroovePuterMidi
