#pragma once
#ifndef GROOVEPUTER_PROJECT_TRANSPORT_TIMELINE_H
#define GROOVEPUTER_PROJECT_TRANSPORT_TIMELINE_H

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

namespace GroovePuterMidi {

inline constexpr double kProjectStepsPerQuarter = 4.0;
inline constexpr double kProjectStepsPerBar = 16.0;
inline constexpr uint32_t kProjectPhaseScale = 65535u;

struct ProjectTransportBlockSnapshot {
    bool valid{false};
    bool playing{false};
    uint32_t blockSequence{0};
    uint16_t blockFrames{0};
    uint32_t sampleRate{0};
    uint16_t bpmX10{1200};
    uint32_t barCounter{0};
    uint16_t phaseQ16{0};

    double phaseSteps() const {
        return static_cast<double>(phaseQ16) * kProjectStepsPerBar /
               static_cast<double>(kProjectPhaseScale);
    }

    double absoluteSteps() const {
        return static_cast<double>(barCounter) * kProjectStepsPerBar +
               phaseSteps();
    }
};

struct ProjectScheduledPosition {
    uint32_t blockSequence{0};
    uint16_t frameOffset{0};
};

class ProjectTransportTimeline {
public:
    void publishBlock(uint32_t blockSequence,
                      uint16_t blockFrames,
                      float phaseSteps,
                      float bpm,
                      float sampleRate,
                      bool playing) {
        const double phase = normalizePhase(phaseSteps);
        if (playing && !previousPlaying_) {
            barCounter_ = 0;
        } else if (playing && previousPlaying_) {
            // One render block cannot span a whole bar at supported tempos.
            // A large backwards phase jump therefore means the 16-step phase
            // wrapped and a new project bar began.
            if (phase + 8.0 < previousPhase_) ++barCounter_;
        }
        previousPlaying_ = playing;
        previousPhase_ = phase;

        const uint16_t encodedPhase = static_cast<uint16_t>(std::lround(
            phase * static_cast<double>(kProjectPhaseScale) /
            kProjectStepsPerBar));
        const uint16_t encodedBpm = static_cast<uint16_t>(std::max<long>(
            1L, std::min<long>(65535L, std::lround(static_cast<double>(bpm) * 10.0))));
        const uint32_t encodedRate = sampleRate > 0.0f
            ? static_cast<uint32_t>(std::lround(sampleRate))
            : 0u;

        sequence_.fetch_add(1, std::memory_order_acq_rel);
        blockSequence_.store(blockSequence, std::memory_order_relaxed);
        blockFrames_.store(blockFrames, std::memory_order_relaxed);
        sampleRate_.store(encodedRate, std::memory_order_relaxed);
        bpmX10_.store(encodedBpm, std::memory_order_relaxed);
        barCounterPublished_.store(barCounter_, std::memory_order_relaxed);
        phaseQ16_.store(encodedPhase, std::memory_order_relaxed);
        flags_.store(static_cast<uint8_t>(kValidFlag | (playing ? kPlayingFlag : 0u)),
                     std::memory_order_relaxed);
        sequence_.fetch_add(1, std::memory_order_release);
    }

    ProjectTransportBlockSnapshot snapshot() const {
        ProjectTransportBlockSnapshot out{};
        for (int attempt = 0; attempt < 4; ++attempt) {
            const uint32_t before = sequence_.load(std::memory_order_acquire);
            if (before & 1u) continue;

            const uint8_t flags = flags_.load(std::memory_order_relaxed);
            out.valid = (flags & kValidFlag) != 0;
            out.playing = (flags & kPlayingFlag) != 0;
            out.blockSequence = blockSequence_.load(std::memory_order_relaxed);
            out.blockFrames = blockFrames_.load(std::memory_order_relaxed);
            out.sampleRate = sampleRate_.load(std::memory_order_relaxed);
            out.bpmX10 = bpmX10_.load(std::memory_order_relaxed);
            out.barCounter = barCounterPublished_.load(std::memory_order_relaxed);
            out.phaseQ16 = phaseQ16_.load(std::memory_order_relaxed);

            const uint32_t after = sequence_.load(std::memory_order_acquire);
            if (before == after && !(after & 1u)) return out;
        }
        return ProjectTransportBlockSnapshot{};
    }

    void resetPublisher() {
        previousPlaying_ = false;
        previousPhase_ = 0.0;
        barCounter_ = 0;
    }

private:
    static constexpr uint8_t kValidFlag = 0x01;
    static constexpr uint8_t kPlayingFlag = 0x02;

    static double normalizePhase(float phaseSteps) {
        if (!std::isfinite(phaseSteps)) return 0.0;
        double phase = std::fmod(static_cast<double>(phaseSteps), kProjectStepsPerBar);
        if (phase < 0.0) phase += kProjectStepsPerBar;
        return phase;
    }

    std::atomic<uint32_t> sequence_{0};
    std::atomic<uint32_t> blockSequence_{0};
    std::atomic<uint16_t> blockFrames_{0};
    std::atomic<uint32_t> sampleRate_{0};
    std::atomic<uint16_t> bpmX10_{1200};
    std::atomic<uint32_t> barCounterPublished_{0};
    std::atomic<uint16_t> phaseQ16_{0};
    std::atomic<uint8_t> flags_{0};

    // Publisher-side state. Only AudioTask calls publishBlock().
    bool previousPlaying_{false};
    double previousPhase_{0.0};
    uint32_t barCounter_{0};
};

inline ProjectTransportTimeline& projectTransportTimeline() {
    static ProjectTransportTimeline timeline;
    return timeline;
}

inline double nextProjectBarStep(const ProjectTransportBlockSnapshot& snapshot) {
    const double current = snapshot.absoluteSteps();
    const double bar = std::floor(current / kProjectStepsPerBar);
    return (bar + 1.0) * kProjectStepsPerBar;
}

inline bool scheduleProjectStep(double originProjectStep,
                                uint32_t originBlockSequence,
                                uint16_t originFrameOffset,
                                double eventProjectStep,
                                uint16_t bpmX10,
                                uint32_t sampleRate,
                                uint16_t blockFrames,
                                ProjectScheduledPosition& out) {
    if (eventProjectStep < originProjectStep || bpmX10 == 0 || sampleRate == 0 ||
        blockFrames == 0) {
        return false;
    }

    const double bpm = static_cast<double>(bpmX10) / 10.0;
    const double framesPerStep =
        static_cast<double>(sampleRate) * 60.0 /
        (bpm * kProjectStepsPerQuarter);
    const double frameDelta = (eventProjectStep - originProjectStep) * framesPerStep;
    if (!std::isfinite(frameDelta) || frameDelta < 0.0) return false;

    const uint64_t deltaFrames = static_cast<uint64_t>(std::llround(frameDelta));
    const uint64_t absoluteFrames = static_cast<uint64_t>(originFrameOffset) + deltaFrames;
    const uint64_t blockOffset = absoluteFrames / blockFrames;
    if (blockOffset > std::numeric_limits<uint32_t>::max()) return false;

    out.blockSequence = originBlockSequence + static_cast<uint32_t>(blockOffset);
    out.frameOffset = static_cast<uint16_t>(absoluteFrames % blockFrames);
    return true;
}

inline bool scheduleProjectSmfTick(uint16_t division,
                                   uint32_t originTick,
                                   double originProjectStep,
                                   uint32_t originBlockSequence,
                                   uint16_t originFrameOffset,
                                   uint32_t eventTick,
                                   uint16_t bpmX10,
                                   uint32_t sampleRate,
                                   uint16_t blockFrames,
                                   ProjectScheduledPosition& out) {
    if (division == 0 || eventTick < originTick) return false;
    const double tickDelta = static_cast<double>(eventTick - originTick);
    const double eventProjectStep = originProjectStep +
        tickDelta * kProjectStepsPerQuarter / static_cast<double>(division);

    // While the real project transport is running, calculate every not-yet-due
    // SMF deadline from the same freshly published phase/block snapshot used by
    // MIDI Clock. The previous implementation projected every event forever
    // from one launch BPM/block anchor. Tiny differences between that idealized
    // projection and the engine's Q32.32 phase could accumulate during a long
    // SEQTRAK recording and feel like notes slowly moving against the groove.
    //
    // Keep the explicit origin fallback for deterministic host tests, stopped
    // transport and already-late cleanup events. NoteOff cleanup therefore keeps
    // its original deadline semantics rather than being pulled forward in a
    // catch-up burst.
    const ProjectTransportBlockSnapshot live = projectTransportTimeline().snapshot();
    constexpr double kLivePhaseEpsilon = 1.0e-6;
    if (live.valid && live.playing && live.bpmX10 > 0 &&
        live.sampleRate > 0 && live.blockFrames > 0 &&
        eventProjectStep + kLivePhaseEpsilon >= live.absoluteSteps()) {
        return scheduleProjectStep(live.absoluteSteps(),
                                   live.blockSequence,
                                   0,
                                   eventProjectStep,
                                   live.bpmX10,
                                   live.sampleRate,
                                   live.blockFrames,
                                   out);
    }

    return scheduleProjectStep(originProjectStep,
                               originBlockSequence,
                               originFrameOffset,
                               eventProjectStep,
                               bpmX10,
                               sampleRate,
                               blockFrames,
                               out);
}

inline uint32_t projectTickAtStep(uint16_t division,
                                  uint32_t originTick,
                                  double originProjectStep,
                                  double currentProjectStep,
                                  uint32_t endTick) {
    if (division == 0 || currentProjectStep <= originProjectStep) return originTick;
    const double stepDelta = currentProjectStep - originProjectStep;
    const double tickDelta = stepDelta * static_cast<double>(division) /
                             kProjectStepsPerQuarter;
    if (!std::isfinite(tickDelta) || tickDelta <= 0.0) return originTick;
    const uint64_t tick = static_cast<uint64_t>(originTick) +
                          static_cast<uint64_t>(std::llround(tickDelta));
    return static_cast<uint32_t>(std::min<uint64_t>(tick, endTick));
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_PROJECT_TRANSPORT_TIMELINE_H
