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
    assert(beforeWrap.bpmQ16 == (120u << 16));
    assert(beforeWrap.transportEpoch == 1);
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
    const ProjectTransportBlockSnapshot noLive{};
    assert(scheduleProjectSmfTick(noLive,
                                  ProjectSmfLatePolicy::DropNoteOn,
                                  480,
                                  0,
                                  32.0,
                                  200,
                                  0,
                                  480,
                                  1200,
                                  22050,
                                  512,
                                  quarter) == ProjectSmfScheduleResult::Scheduled);
    const uint64_t quarterFrames =
        static_cast<uint64_t>(quarter.blockSequence - 200) * 512u +
        quarter.frameOffset;
    assert(quarterFrames == 11025u);

    // Changing only project BPM changes absolute time but not musical tick
    // placement. At 90 BPM the same quarter note is 14700 frames.
    ProjectScheduledPosition slower{};
    assert(scheduleProjectSmfTick(noLive,
                                  ProjectSmfLatePolicy::DropNoteOn,
                                  480,
                                  0,
                                  32.0,
                                  200,
                                  0,
                                  480,
                                  900,
                                  22050,
                                  512,
                                  slower) == ProjectSmfScheduleResult::Scheduled);
    const uint64_t slowerFrames =
        static_cast<uint64_t>(slower.blockSequence - 200) * 512u +
        slower.frameOffset;
    assert(slowerFrames == 14700u);

    // Triplet timing stays off the straight 1/16 grid: 160 ticks at 480 PPQN
    // equals one third of a quarter note, not a 120-tick sixteenth.
    ProjectScheduledPosition triplet{};
    assert(scheduleProjectSmfTick(noLive,
                                  ProjectSmfLatePolicy::DropNoteOn,
                                  480,
                                  0,
                                  0.0,
                                  0,
                                  0,
                                  160,
                                  1200,
                                  22050,
                                  512,
                                  triplet) == ProjectSmfScheduleResult::Scheduled);
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

    timeline.publishBlock(103, 512, 0.0f, 128.0f, 22050.0f, true);
    ProjectTransportBlockSnapshot restarted{};
    assert(timeline.trySnapshot(restarted));
    assert(restarted.transportEpoch == 2);
    timeline.publishBlock(104, 512, 6.0f, 123.456f, 22050.0f, true);
    auto fractionalBpm = timeline.snapshot();
    assert(closeEnough(fractionalBpm.bpm(), 123.456, 2.0e-5));
    timeline.publishBlock(105, 512, 2.0f, 123.456f, 22050.0f, true);
    assert(timeline.snapshot().transportEpoch == 3);

    // With the authoritative global transport running, future SMF events are
    // scheduled from its live phase/block anchor rather than from the stale
    // launch projection supplied by the caller. At phase ~= 8 steps, an event
    // at step 10 is about 5512 frames ahead, regardless of the old origin block.
    auto& liveTimeline = projectTransportTimeline();
    liveTimeline.resetPublisher();
    liveTimeline.publishBlock(300, 512, 8.0f, 120.0f, 22050.0f, true);
    ProjectScheduledPosition liveAnchored{};
    const ProjectTransportBlockSnapshot live = liveTimeline.snapshot();
    assert(scheduleProjectSmfTick(live,
                                  ProjectSmfLatePolicy::DropNoteOn,
                                  480,
                                  0,
                                  0.0,
                                  5,
                                  0,
                                  1200,
                                  1200,
                                  22050,
                                  512,
                                  liveAnchored) == ProjectSmfScheduleResult::Scheduled);
    assert(liveAnchored.blockSequence == 310u);
    assert(liveAnchored.frameOffset == 392u);

    // Every event in one scheduling pass uses the same explicit snapshot, so
    // simultaneous chord notes cannot split across different live anchors.
    ProjectScheduledPosition chordA{};
    ProjectScheduledPosition chordB{};
    assert(scheduleProjectSmfTick(live,
                                  ProjectSmfLatePolicy::DropNoteOn,
                                  480,
                                  0.0,
                                  0.0,
                                  5,
                                  0,
                                  1200,
                                  1200,
                                  22050,
                                  512,
                                  chordA) == ProjectSmfScheduleResult::Scheduled);
    assert(scheduleProjectSmfTick(live,
                                  ProjectSmfLatePolicy::DropNoteOn,
                                  480,
                                  0.0,
                                  0.0,
                                  5,
                                  0,
                                  1200,
                                  1200,
                                  22050,
                                  512,
                                  chordB) == ProjectSmfScheduleResult::Scheduled);
    assert(chordA.blockSequence == chordB.blockSequence);
    assert(chordA.frameOffset == chordB.frameOffset);

    ProjectScheduledPosition late{};
    assert(scheduleProjectSmfTick(live,
                                  ProjectSmfLatePolicy::DropNoteOn,
                                  480,
                                  0.0,
                                  0.0,
                                  5,
                                  0,
                                  480,
                                  1200,
                                  22050,
                                  512,
                                  late) == ProjectSmfScheduleResult::DroppedLateNoteOn);
    assert(scheduleProjectSmfTick(
               live,
               ProjectSmfLatePolicy::DispatchNoteOffImmediately,
               480,
               0.0,
               0.0,
               5,
               0,
               480,
               1200,
               22050,
               512,
               late) == ProjectSmfScheduleResult::Scheduled);
    assert(late.blockSequence == live.blockSequence);
    assert(late.frameOffset == 0);

    // Repeated tempo anchors preserve the exact fractional musical position;
    // integer parser seeks no longer become the next scheduling origin.
    double exactTick = 0.0;
    double exactStep = 0.0;
    for (int i = 1; i <= 20; ++i) {
        const double nextStep = static_cast<double>(i) * 0.37;
        exactTick = projectSmfTickAtStep(
            480, exactTick, exactStep, nextStep, 10000);
        exactStep = nextStep;
    }
    assert(closeEnough(exactTick, 888.0, 1.0e-9));

    return 0;
}
