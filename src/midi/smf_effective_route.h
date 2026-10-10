#pragma once
#ifndef GROOVEPUTER_SMF_EFFECTIVE_ROUTE_H
#define GROOVEPUTER_SMF_EFFECTIVE_ROUTE_H

#include <cstdint>
#include <cstdio>

#include "smf_routing.h"
#include "smf_track_inspector.h"
#include "smf_track_output_route.h"

// Where a SMF track actually sounds, for display: AUTO resolved through the
// same rule the producer applies (routeSmfNote), so a track whose source
// channel has no SEQTRAK destination reads OFF instead of AUTO. Display only;
// routing itself stays in smf_routing.h / the producer.
namespace GroovePuterMidi {

enum class SmfEffectiveRouteKind : uint8_t {
    Off = 0,     // nothing reaches the wire (SEQTRAK, unmapped source channel)
    Output,      // one SEQTRAK output 0..9
    DrumSplit,   // GM drums (CH10) split over the drum outputs 0..6
    Multi,       // several source channels, routed per channel
    Raw,         // RAW routing: source channel passes through
};

struct SmfEffectiveRoute {
    SmfEffectiveRouteKind kind{SmfEffectiveRouteKind::Off};
    int8_t output{-1};        // Output: 0..9
    int8_t rawChannel{-1};    // Raw: 0..15, -1 when several channels
    bool overridden{false};   // a per-track choice, not AUTO
};

inline SmfEffectiveRoute effectiveSmfTrackRoute(bool rawRouting,
                                                const SmfTrackInfoSnapshot* info,
                                                int8_t destinationOverride) {
    SmfEffectiveRoute route{};
    const int primary = info ? info->primaryChannel() : -1;
    const bool multi = info && info->usesMultipleChannels();
    if (rawRouting) {
        route.kind = SmfEffectiveRouteKind::Raw;
        route.rawChannel = static_cast<int8_t>(primary);
        return route;
    }
    if (destinationOverride >= 0 &&
        destinationOverride < static_cast<int8_t>(kSmfSeqtrakOutputChannelCount)) {
        route.kind = SmfEffectiveRouteKind::Output;
        route.output = destinationOverride;
        route.overridden = true;
        return route;
    }
    if (multi) {
        route.kind = SmfEffectiveRouteKind::Multi;
        return route;
    }
    if (primary < 0) return route;
    if (primary == 9) {
        route.kind = SmfEffectiveRouteKind::DrumSplit;
        return route;
    }
    const SmfRoutedNote routed =
        routeSmfNote(SmfRoutingMode::Seqtrak, static_cast<uint8_t>(primary), 60);
    if (!routed.mapped) return route;
    route.kind = SmfEffectiveRouteKind::Output;
    route.output = static_cast<int8_t>(routed.channel);
    return route;
}

inline const char* seqtrakOutputName(int8_t output) {
    static constexpr const char* kNames[kSmfSeqtrakOutputChannelCount] = {
        "KICK", "SNARE", "CLAP", "HAT-C", "HAT-O",
        "PERC", "CYM", "SYN1", "SYN2", "DX",
    };
    if (output < 0 || output >= static_cast<int8_t>(kSmfSeqtrakOutputChannelCount)) {
        return "?";
    }
    return kNames[output];
}

// Short label (at most 5 characters): SYN1, DX, KICK, DRUM, MULTI, OFF, C03.
inline void formatEffectiveSmfRoute(const SmfEffectiveRoute& route,
                                    char* output,
                                    std::size_t outputSize) {
    if (!output || outputSize == 0u) return;
    switch (route.kind) {
        case SmfEffectiveRouteKind::Output:
            std::snprintf(output, outputSize, "%s", seqtrakOutputName(route.output));
            return;
        case SmfEffectiveRouteKind::DrumSplit:
            std::snprintf(output, outputSize, "DRUM");
            return;
        case SmfEffectiveRouteKind::Multi:
            std::snprintf(output, outputSize, "MULTI");
            return;
        case SmfEffectiveRouteKind::Raw:
            if (route.rawChannel < 0) std::snprintf(output, outputSize, "C--");
            else std::snprintf(output, outputSize, "C%02d", route.rawChannel + 1);
            return;
        case SmfEffectiveRouteKind::Off:
        default:
            std::snprintf(output, outputSize, "OFF");
            return;
    }
}

// SYN1/SYN2/DX play one part each: two unmuted tracks on one of them mix two
// parts into one instrument, which is what used to be found by ear.
inline bool smfOutputIsMelodic(int8_t output) {
    return output >= 7 && output <= 9;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_EFFECTIVE_ROUTE_H
