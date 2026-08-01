#include <cassert>
#include <cmath>

#include "src/midi/project_transport_timeline.h"

using namespace GroovePuterMidi;

namespace {
bool closeEnough(double a, double b, double epsilon = 0.002) {
    return std::fabs(a - b) <= epsilon;
}
}

int main() {
    ProjectTransportTimeline timeline;

    timeline.publishBlock(1, 512, 0.0f, 120.0f, 22050.0f, true, true);
    const auto started = timeline.snapshot();
    assert(started.valid);
    assert(started.playing);
    assert(started.restartedFromBeginning);
    assert(started.transportEpoch == 1);
    assert(started.barCounter == 0);

    timeline.publishBlock(2, 512, 15.5f, 120.0f, 22050.0f, true, true);
    timeline.publishBlock(3, 512, 0.5f, 120.0f, 22050.0f, true, true);
    assert(timeline.snapshot().barCounter == 1);

    timeline.publishBlock(4, 512, 0.5f, 120.0f, 22050.0f, false, true);
    timeline.publishBlock(5, 512, 0.5f, 120.0f, 22050.0f, true, false);
    const auto continued = timeline.snapshot();
    assert(continued.playing);
    assert(!continued.restartedFromBeginning);
    assert(continued.transportEpoch == 2);
    assert(continued.barCounter == 1);
    assert(closeEnough(continued.absoluteSteps(), 16.5));

    // Timeline keeps generic bar quantization. Only the SMF service may choose
    // its bounded three-block prefill for a file that was active before Stop.
    assert(closeEnough(nextProjectBarStep(continued), 32.0));

    timeline.publishBlock(6, 512, 0.5f, 120.0f, 22050.0f, false, false);
    timeline.publishBlock(7, 512, 0.5f, 120.0f, 22050.0f, true, true);
    const auto restarted = timeline.snapshot();
    assert(restarted.restartedFromBeginning);
    assert(restarted.transportEpoch == 3);
    assert(restarted.barCounter == 0);
    assert(closeEnough(nextProjectBarStep(restarted), 16.0));

    return 0;
}
