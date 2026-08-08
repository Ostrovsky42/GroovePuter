#include <cassert>

#include "src/midi/smf_routing.h"
#include "src/midi/smf_session_generation.h"
#include "src/midi/smf_track_output_route.h"

using namespace GroovePuterMidi;

int main() {
    assert(applySmfVelocityBoost(0, 16) == 0);
    assert(applySmfVelocityBoost(64, 0) == 64);
    assert(applySmfVelocityBoost(64, 8) == 72);
    assert(applySmfVelocityBoost(20, 48) == 68);
    assert(applySmfVelocityBoost(64, 48) == 112);
    assert(applySmfVelocityBoost(96, 48) == 127);
    assert(applySmfVelocityBoost(127, 48) == 127);

    uint8_t boost = 0;
    constexpr uint8_t kExpectedBoosts[] = {8, 16, 24, 32, 48, 0};
    for (uint8_t expected : kExpectedBoosts) {
        boost = nextSmfVelocityBoost(boost);
        assert(boost == expected);
    }
    assert(nextSmfVelocityBoost(7) == 0);

    const SmfRoutedNote raw = routeSmfNote(SmfRoutingMode::Raw, 9, 36);
    assert(raw.mapped && raw.channel == 9 && raw.note == 36);

    const SmfRoutedNote kick = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 36);
    const SmfRoutedNote snare = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 38);
    const SmfRoutedNote clap = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 39);
    const SmfRoutedNote closedHat = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 42);
    const SmfRoutedNote openHat = routeSmfNote(SmfRoutingMode::Seqtrak, 9, 46);
    assert(kick.mapped && kick.channel == 0 && kick.note == 60);
    assert(snare.mapped && snare.channel == 1 && snare.note == 60);
    assert(clap.mapped && clap.channel == 2 && clap.note == 60);
    assert(closedHat.mapped && closedHat.channel == 3 && closedHat.note == 60);
    assert(openHat.mapped && openHat.channel == 4 && openHat.note == 60);

    const SmfRoutedNote synth1 = routeSmfNote(SmfRoutingMode::Seqtrak, 0, 64);
    const SmfRoutedNote synth2 = routeSmfNote(SmfRoutingMode::Seqtrak, 1, 64);
    const SmfRoutedNote dx = routeSmfNote(SmfRoutingMode::Seqtrak, 2, 64);
    const SmfRoutedNote unmapped = routeSmfNote(SmfRoutingMode::Seqtrak, 3, 64);
    const SmfRoutedNote extraMelodic = routeSmfNote(SmfRoutingMode::Seqtrak, 14, 67);

    assert(synth1.mapped && synth1.channel == 7 && synth1.note == 64);
    assert(synth2.mapped && synth2.channel == 8 && synth2.note == 64);
    assert(dx.mapped && dx.channel == 9 && dx.note == 64);

    // DX is a dedicated CH10 destination, never the fallback bucket. Extra
    // melodic lanes remain filtered until a track gets an explicit destination.
    assert(!unmapped.mapped && unmapped.note == 64);
    assert(!extraMelodic.mapped && extraMelodic.note == 67);

    const SmfRoutedNote forcedKick =
        routeSmfTrackNote(SmfRoutingMode::Seqtrak, 4, 41, 0);
    const SmfRoutedNote forcedCymbal =
        routeSmfTrackNote(SmfRoutingMode::Seqtrak, 4, 41, 6);
    const SmfRoutedNote forcedSynth1 =
        routeSmfTrackNote(SmfRoutingMode::Seqtrak, 14, 67, 7);
    const SmfRoutedNote forcedDx =
        routeSmfTrackNote(SmfRoutingMode::Seqtrak, 14, 67, 9);
    const SmfRoutedNote invalidDestination =
        routeSmfTrackNote(SmfRoutingMode::Seqtrak, 0, 64, 10);
    const SmfRoutedNote rawIgnoresOverride =
        routeSmfTrackNote(SmfRoutingMode::Raw, 9, 36, 7);

    assert(forcedKick.mapped && forcedKick.channel == 0 && forcedKick.note == 60);
    assert(forcedCymbal.mapped && forcedCymbal.channel == 6 && forcedCymbal.note == 60);
    assert(forcedSynth1.mapped && forcedSynth1.channel == 7 && forcedSynth1.note == 67);
    assert(forcedDx.mapped && forcedDx.channel == 9 && forcedDx.note == 67);
    assert(!invalidDestination.mapped);
    assert(rawIgnoresOverride.mapped &&
           rawIgnoresOverride.channel == 9 &&
           rawIgnoresOverride.note == 36);

    const uint32_t generation = smfBeginSessionOpen();
    assert(generation != 0u);
    assert(smfCompleteSessionOpen(generation));

    SmfTrackOutputRouteState& routes = smfTrackOutputRouteState();
    SmfTrackOutputRouteSnapshot routeSnapshot = routes.snapshot(4);
    assert(routeSnapshot.generation == generation);
    assert(routeSnapshot.trackCount == 4);
    for (uint16_t track = 0; track < routeSnapshot.trackCount; ++track) {
        assert(routeSnapshot.destinationFor(track) == kSmfTrackOutputRouteAuto);
        assert(routes.revisionTagForRealtime(track) == 0u);
    }

    assert(routes.setDestination(2, 7, generation, 4));
    assert(routes.destinationFor(2, 4) == 7);
    assert(routes.revisionTagForRealtime(2) == 1u);

    uint8_t releaseTrack = 0xFFu;
    assert(routes.takePendingReleaseTrack(releaseTrack));
    assert(releaseTrack == 2u);
    assert(!routes.takePendingReleaseTrack(releaseTrack));

    // Re-applying the same route is a no-op: it must not cut a sounding note,
    // advance the per-track revision, or publish another cleanup request.
    assert(routes.setDestination(2, 7, generation, 4));
    assert(routes.revisionTagForRealtime(2) == 1u);
    assert(!routes.takePendingReleaseTrack(releaseTrack));

    // The scheduler-side lookup captures destination and revision atomically;
    // the next queue publication consumes exactly that one-event stamp.
    assert(routes.destinationForProducer(2, 4) == 7);
    assert(routes.consumeProducerRevisionTag(2) == 1u);

    assert(!routes.setDestination(4, 7, generation, 4));
    assert(!routes.setDestination(2, 10, generation, 4));

    routeSnapshot = routes.snapshot(4);
    assert(routeSnapshot.generation == generation);
    assert(routeSnapshot.destinationFor(2) == 7);
    assert(routeSnapshot.overridden(2));

    int8_t restored[4] = {
        kSmfTrackOutputRouteAuto, 8, 9, kSmfTrackOutputRouteAuto};
    assert(routes.replaceDestinations(restored, 4, generation));
    routeSnapshot = routes.snapshot(4);
    assert(routeSnapshot.destinationFor(0) == kSmfTrackOutputRouteAuto);
    assert(routeSnapshot.destinationFor(1) == 8);
    assert(routeSnapshot.destinationFor(2) == 9);
    assert(routeSnapshot.destinationFor(3) == kSmfTrackOutputRouteAuto);
    assert(routes.revisionTagForRealtime(1) == 0u);
    assert(routes.revisionTagForRealtime(2) == 0u);
    assert(!routes.takePendingReleaseTrack(releaseTrack));

    restored[2] = 10;
    assert(!routes.replaceDestinations(restored, 4, generation));
    assert(routes.snapshot(4).destinationFor(2) == 9);

    const uint32_t nextGeneration = smfBeginSessionOpen();
    assert(nextGeneration != 0u && nextGeneration != generation);
    assert(smfCompleteSessionOpen(nextGeneration));
    routeSnapshot = routes.snapshot(2);
    assert(routeSnapshot.generation == nextGeneration);
    assert(routeSnapshot.trackCount == 2);
    assert(routeSnapshot.destinationFor(0) == kSmfTrackOutputRouteAuto);
    assert(routeSnapshot.destinationFor(1) == kSmfTrackOutputRouteAuto);
    assert(routes.revisionTagForRealtime(0) == 0u);
    assert(routes.revisionTagForRealtime(1) == 0u);

    return 0;
}
