// Where a SMF track actually sounds, as the MIDI hub shows it: AUTO resolved
// through the producer's own rule, so an unmapped track reads OFF, not AUTO.
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/midi/smf_effective_route.h"

using namespace GroovePuterMidi;

namespace {
SmfTrackInfoSnapshot onChannels(uint16_t mask) {
    SmfTrackInfoSnapshot info{};
    info.channelMask = mask;
    info.flags = SmfTrackInfoSnapshot::kAudible;
    return info;
}

const char* label(const SmfEffectiveRoute& route) {
    static char text[8];
    formatEffectiveSmfRoute(route, text, sizeof(text));
    return text;
}
}  // namespace

int main() {
    const SmfTrackInfoSnapshot ch1 = onChannels(1u << 0);
    const SmfTrackInfoSnapshot ch2 = onChannels(1u << 1);
    const SmfTrackInfoSnapshot ch3 = onChannels(1u << 2);
    const SmfTrackInfoSnapshot ch5 = onChannels(1u << 4);
    const SmfTrackInfoSnapshot ch10 = onChannels(1u << 9);
    const SmfTrackInfoSnapshot multi = onChannels((1u << 0) | (1u << 4));
    const SmfTrackInfoSnapshot silent = onChannels(0);

    // AUTO follows routeSmfNote: CH1/2/3 -> SYN1/SYN2/DX, CH10 -> drum split.
    auto r = effectiveSmfTrackRoute(false, &ch1, kSmfTrackOutputRouteAuto);
    assert(r.kind == SmfEffectiveRouteKind::Output && r.output == 7 && !r.overridden);
    assert(std::strcmp(label(r), "SYN1") == 0);
    assert(std::strcmp(label(effectiveSmfTrackRoute(false, &ch2, -1)), "SYN2") == 0);
    assert(std::strcmp(label(effectiveSmfTrackRoute(false, &ch3, -1)), "DX") == 0);
    r = effectiveSmfTrackRoute(false, &ch10, -1);
    assert(r.kind == SmfEffectiveRouteKind::DrumSplit);
    assert(std::strcmp(label(r), "DRUM") == 0);

    // The case that used to be found by ear: AUTO on CH5 reaches nothing.
    r = effectiveSmfTrackRoute(false, &ch5, -1);
    assert(r.kind == SmfEffectiveRouteKind::Off);
    assert(std::strcmp(label(r), "OFF") == 0);
    assert(effectiveSmfTrackRoute(false, &silent, -1).kind == SmfEffectiveRouteKind::Off);
    assert(effectiveSmfTrackRoute(false, nullptr, -1).kind == SmfEffectiveRouteKind::Off);
    assert(effectiveSmfTrackRoute(false, &multi, -1).kind == SmfEffectiveRouteKind::Multi);

    // A per-track choice wins over the source channel and is marked as one.
    r = effectiveSmfTrackRoute(false, &ch5, 8);
    assert(r.kind == SmfEffectiveRouteKind::Output && r.output == 8 && r.overridden);
    assert(std::strcmp(label(r), "SYN2") == 0);
    r = effectiveSmfTrackRoute(false, &ch10, 0);
    assert(std::strcmp(label(r), "KICK") == 0);

    // RAW passes the source channel through; overrides do not apply.
    r = effectiveSmfTrackRoute(true, &ch5, 8);
    assert(r.kind == SmfEffectiveRouteKind::Raw && r.rawChannel == 4);
    assert(std::strcmp(label(r), "C05") == 0);
    assert(std::strcmp(label(effectiveSmfTrackRoute(true, &multi, -1)), "C--") == 0);

    // Labels fit the hub row (five characters at most).
    for (int8_t output = 0; output < static_cast<int8_t>(kSmfSeqtrakOutputChannelCount); ++output) {
        assert(std::strlen(seqtrakOutputName(output)) <= 5u);
    }
    assert(smfOutputIsMelodic(7) && smfOutputIsMelodic(9));
    assert(!smfOutputIsMelodic(0) && !smfOutputIsMelodic(-1));

    std::puts("smf effective route: PASS");
    return 0;
}
