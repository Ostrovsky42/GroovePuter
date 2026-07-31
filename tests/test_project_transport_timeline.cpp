#include <cassert>
#include <cmath>
#include <cstdint>

#include "src/midi/project_transport_timeline.h"

using namespace GroovePuterMidi;

namespace {
bool closeEnough(double a, double b, double epsilon = 0.002) {
    return std::fabs(a - b) <= epsilon;
}
}

int main() {
    ProjectTransportTimeline timeline;

    timeline.publishBlock(100, 512, 15.5f, 120.0f, 22050.0f, true);
    auto beforeWrap = timeline.snapshot();
    assert(beforeWrap.valid);
    assert(beforeWrap.playing);
    assert(beforeWrap.blockSequence == 100);
    assert(beforeWrap.blockFrames == 512);
    assert(beforeWrap.sampleRate == 22050);
    assert(beforeWrap.bpmX10 == 1200);
    assert(beforeWrap.barCounter == 0);
    assert(closeEnough(beforeWrap.phaseSteps(), 15.5));

    timeline.publishBlock(101, 512, 0.25f, 120.0f, 22050.0f, true);
    auto afterWrap = timeline.snapshot();
    assert(afterWrap.barCounter == 1);
    assert(closeEnough(afterWrap.absoluteSteps(), 16.25));
    assert(closeEnough(nextProjectBarStep(afterWrap), 32.0));

    // A quarter note in a 480 PPQN file is four project steps. At 120 BPM
    // and 22.05 kHz that is 11025 frames, independent of the SMF tempo map.
    ProjectScheduledPosition quarter{};
    assert(scheduleProjectSmfTick(480,
                                  0,
                                  32.0,
                                  200,
                                  0,
                                  480,
                                  1200,
                                  22050,
                                  512,
                                  quarter));
    const uint64_t quarterFrames =
        static_cast<uint64_t>(quarter.blockSequence - 200) * 512u +
        quarter.frameOffset;
    assert(quarterFrames == 11025u);

    // Changing only project BPM changes absolute time but not musical tick
    // placement. At 90 BPM the same quarter note is 14700 frames.
    ProjectScheduledPosition slower{};
    assert(scheduleProjectSmfTick(480,
                                  0,
                                  32.0,
                                  200,
                                  0,
                                  480,
                                  900,
                                  22050,
                                  512,
                                  slower));
    const uint64_t slowerFrames =
        static_cast<uint64_t>(slower.blockSequence - 200) * 512u +
        slower.frameOffset;
    assert(slowerFrames == 14700u);

    // Triplet timing stays off the straight 1/16 grid: 160 ticks at 480 PPQN
    // equals one third of a quarter note, not a 120-tick sixteenth.
    ProjectScheduledPosition triplet{};
    assert(scheduleProjectSmfTick(480,
                                  0,
                                  0.0,
                                  0,
                                  0,
                                  160,
                                  1200,
                                  22050,
                                  512,
                                  triplet));
    const uint64_t tripletFrames =
        static_cast<uint64_t>(triplet.blockSequence) * 512u +
        triplet.frameOffset;
    assert(tripletFrames == 3675u);
    assert(tripletFrames != 2756u);  // straight 1/16 at 120 BPM

    // An origin frame inside a render block must be carried across block
    // boundaries instead of being silently rounded to the block start.
    ProjectScheduledPosition framed{};
    assert(scheduleProjectStep(0.0,
                               10,
                               500,
                               1.0,
                               1200,
                               22050,
                               512,
                               framed));
    const uint64_t expectedFrames = 500u + 2756u;
    assert(framed.blockSequence == 10u + expectedFrames / 512u);
    assert(framed.frameOffset == expectedFrames % 512u);

    // Tick reconstruction is based on musical step distance, so it is
    // independent of BPM and can be used to re-anchor safely after a tempo
    // change.
    assert(projectTickAtStep(480, 960, 16.0, 20.0, 10000) == 1440u);
    assert(projectTickAtStep(480, 960, 16.0, 16.0, 10000) == 960u);

    timeline.publishBlock(102, 512, 1.0f, 128.0f, 22050.0f, false);
    auto stopped = timeline.snapshot();
    assert(stopped.valid);
    assert(!stopped.playing);

    return 0;
}
