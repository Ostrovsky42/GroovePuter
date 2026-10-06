#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>

#include "src/midi/external_midi_clock_follower.h"

using namespace GroovePuterMidi;

// Closed-loop follow: the real tracker/follower drive a modelled sequencer
// through the same ProjectTransportTimeline snapshot AudioTask publishes.
//
// The external master is exact. Its pulses reach the follower the way USB
// delivers them on hardware: on 1 ms frame boundaries plus dispatcher delay.
// GroovePuter's own audio clock may run slightly fast or slow. Following must
// hold phase anyway: on 2026-10-06 the device drifted half a step in 30 s.
namespace {

constexpr float kSampleRate = 22050.0f;
constexpr uint16_t kBlockFrames = 512;
// Notes rendered in a block leave the dispatcher one block later (the
// Cardputer transport's kOutputLatencyUs). What the master hears is the note,
// so the error is measured at emission time, not at render time.
constexpr uint32_t kOutputLatencyUs = 23220;

struct Case {
    double bpm;
    double localClockSkew;  // +0.005 = GroovePuter audio clock 0.5% fast
    double usbJitterUs;
};

struct Outcome {
    double maxAbsErrorMs{0.0};
    double meanErrorMs{0.0};
    uint32_t stopsAfterStart{0};
    uint32_t unlockedBlocks{0};
};

Outcome run(const Case& c, double seconds) {
    projectTransportTimeline().resetPublisher();
    ExternalMidiTransportEventQueue queue;
    ExternalMidiClockFollower follower;
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> jitter(0.0, c.usbJitterUs);

    const double pulseUs = 60.0e6 / (c.bpm * 24.0);
    const double blockUs =
        kBlockFrames / kSampleRate * 1.0e6 / (1.0 + c.localClockSkew);
    const double firstPulseUs = 500000.0;  // master sends FA, then F8 at beat 1
    const double settleUs = firstPulseUs + 10.0e6;

    double localSteps = 0.0;
    double driveBpm = 120.0;
    bool playing = false;
    bool startSent = false;
    uint32_t ordinal = 0;
    long pulse = 0;

    Outcome out{};
    double errorSum = 0.0;
    long errorCount = 0;

    for (uint32_t block = 1;; ++block) {
        const double now = 1000.0 + block * blockUs;
        if (now > firstPulseUs + seconds * 1.0e6) break;

        if (!startSent && now >= firstPulseUs - 2000.0) {
            queue.tryPushCritical(ExternalMidiTransportEventType::Start,
                                  static_cast<uint32_t>(firstPulseUs - 1000.0),
                                  ordinal);
            startSent = true;
        }
        while (startSent) {
            const double due = firstPulseUs + pulse * pulseUs;
            const double arrival = std::ceil(due / 1000.0) * 1000.0 +
                                   jitter(rng);
            if (arrival > now) break;
            queue.tryPushClock(static_cast<uint32_t>(arrival), ++ordinal);
            ++pulse;
        }

        const auto result = follower.processBlock(
            queue, TransportClockSource::SeqtrakExternal,
            static_cast<uint32_t>(now), true, kOutputLatencyUs);
        if (result.estimate.validTempo) {
            driveBpm = static_cast<double>(result.estimate.bpmQ16) / 65536.0;
        }
        if (result.command == ExternalTransportCommand::Start) {
            playing = true;
            localSteps = 0.0;
        } else if (result.command == ExternalTransportCommand::Stop) {
            if (playing) ++out.stopsAfterStart;
            playing = false;
        }

        projectTransportTimeline().publishBlock(
            block, kBlockFrames,
            static_cast<float>(std::fmod(localSteps, 16.0)),
            static_cast<float>(driveBpm), kSampleRate, playing, true);

        if (playing && now >= settleUs) {
            // First F8 after Start is position 0. The block's first note
            // leaves kOutputLatencyUs after `now`.
            const double externalSteps =
                (now + kOutputLatencyUs - firstPulseUs) / pulseUs / 6.0;
            double errorSteps = localSteps - externalSteps;
            errorSteps -= 16.0 * std::round(errorSteps / 16.0);
            const double errorMs = errorSteps * 60.0e3 / (c.bpm * 4.0);
            out.maxAbsErrorMs = std::max(out.maxAbsErrorMs,
                                         std::fabs(errorMs));
            errorSum += errorMs;
            ++errorCount;
            if (result.estimate.state != ExternalClockLockState::Locked) {
                ++out.unlockedBlocks;
            }
        }

        if (playing) {
            localSteps += kBlockFrames * driveBpm * 4.0 /
                          (60.0 * kSampleRate);
        }
    }
    out.meanErrorMs = errorCount ? errorSum / errorCount : 0.0;
    return out;
}

}  // namespace

int main() {
    const Case cases[] = {
        {128.0, 0.0, 300.0},
        {128.0, 0.005, 300.0},
        {128.0, -0.005, 300.0},
        {90.0, 0.005, 300.0},
        {174.0, -0.005, 600.0},
    };

    int failures = 0;
    for (const Case& c : cases) {
        const Outcome o = run(c, 120.0);
        // 1/16 of a sixteenth note: 7.3 ms at 128 BPM, 10.4 ms at 90 BPM.
        const double toleranceMs = 60.0e3 / (c.bpm * 4.0) / 16.0;
        // USB receive latency (frame wait + dispatch) biases the mean by about
        // a millisecond and cannot be measured from F8 alone. A whole-pulse
        // offset is >= 8.3 ms at any supported tempo, far outside this.
        constexpr double kMeanToleranceMs = 3.0;
        const bool ok = o.maxAbsErrorMs <= toleranceMs &&
                        std::fabs(o.meanErrorMs) <= kMeanToleranceMs &&
                        o.stopsAfterStart == 0 && o.unlockedBlocks == 0;
        std::printf("%s follow %.0f BPM skew %+.1f%% jitter %.0f us: "
                    "max |error| %.2f ms (limit %.2f), mean %+.2f ms, "
                    "stops %u, unlocked blocks %u\n",
                    ok ? "PASS" : "FAIL", c.bpm, c.localClockSkew * 100.0,
                    c.usbJitterUs, o.maxAbsErrorMs, toleranceMs,
                    o.meanErrorMs, o.stopsAfterStart, o.unlockedBlocks);
        if (!ok) ++failures;
    }
    return failures == 0 ? 0 : 1;
}
