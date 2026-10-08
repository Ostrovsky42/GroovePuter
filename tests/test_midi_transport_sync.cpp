#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "src/midi/midi_transport_clock_publisher.h"
#include "src/midi/scheduled_midi_transport_event.h"
#include "src/midi/scheduled_midi_transport_event_queue.h"

namespace {
std::vector<ScheduledMidiTransportEvent> drain(
        ScheduledMidiTransportEventQueue& queue) {
    std::vector<ScheduledMidiTransportEvent> events;
    ScheduledMidiTransportEvent event{};
    while (queue.tryPop(event)) events.push_back(event);
    return events;
}

std::size_t countType(const std::vector<ScheduledMidiTransportEvent>& events,
                      MidiTransportEventType type) {
    std::size_t count = 0;
    for (const auto& event : events) {
        if (event.type == type) ++count;
    }
    return count;
}

std::vector<uint16_t> clockFrames(
        const std::vector<ScheduledMidiTransportEvent>& events) {
    std::vector<uint16_t> frames;
    for (const auto& event : events) {
        if (event.type == MidiTransportEventType::Clock) {
            frames.push_back(event.frameOffset);
        }
    }
    return frames;
}

void testClockInsideBlockSurvivesRoundingPastItsEnd() {
    ScheduledMidiTransportEventQueue queue;
    MidiTransportClockPublisher publisher;
    constexpr unsigned blockFrames = 512;
    constexpr double samplesPerStep = 6615.0; // 50 BPM / 22050 Hz
    // The next pulse is due at frame 511.75: inside this block, even though
    // nearest-frame rounding produces 512. It must not disappear or duplicate.
    const double phase = (1.0 - 511.75 / 1102.5) / 6.0;
    publisher.beginBlock(queue, 0, blockFrames, static_cast<float>(phase),
                         50.0f, 22050.0f, true);
    auto events = drain(queue);
    publisher.beginBlock(queue, 1, blockFrames,
                         static_cast<float>(phase + blockFrames / samplesPerStep),
                         50.0f, 22050.0f, true);
    const auto next = drain(queue);
    events.insert(events.end(), next.begin(), next.end());
    assert(countType(events, MidiTransportEventType::Clock) == 1);
    for (const auto& event : events) {
        assert(event.frameOffset < blockFrames);
        if (event.type == MidiTransportEventType::Clock) {
            assert(event.blockSequence == 1 && event.frameOffset == 0);
        }
    }
    assert(queue.droppedClockCount() == 0);
}

void testClockExactlyAtBlockEndBelongsToNextBlock() {
    ScheduledMidiTransportEventQueue queue;
    MidiTransportClockPublisher publisher;
    publisher.beginBlock(queue, 0, 1000, 0.0f, 120.0f, 48000.0f, true);
    auto frames = clockFrames(drain(queue));
    assert(frames.size() == 1 && frames[0] == 0);
    publisher.beginBlock(queue, 1, 1000, 1.0f / 6.0f,
                         120.0f, 48000.0f, true);
    frames = clockFrames(drain(queue));
    assert(frames.size() == 1 && frames[0] == 0);
}

void testDeferredClockSurvivesTempoChangeButNotStopOrReset() {
    constexpr double phase = (1.0 - 511.75 / 1102.5) / 6.0;
    const float nextPhase = static_cast<float>(phase + 512.0 / 6615.0);
    for (const int action : {0, 1, 2}) {
        ScheduledMidiTransportEventQueue queue;
        MidiTransportClockPublisher publisher;
        publisher.beginBlock(queue, 0, 512, static_cast<float>(phase),
                             50.0f, 22050.0f, true);
        drain(queue);
        if (action == 0) {
            publisher.beginBlock(queue, 1, 512, nextPhase,
                                 128.0f, 22050.0f, true);
            const auto frames = clockFrames(drain(queue));
            // The carried pulse at zero plus one pulse at the new cadence.
            assert(frames.size() == 2 && frames[0] == 0 && frames[1] > 0);
        } else {
            if (action == 1) {
                publisher.beginBlock(queue, 1, 512, nextPhase,
                                     50.0f, 22050.0f, false);
                const auto events = drain(queue);
                assert(countType(events, MidiTransportEventType::Stop) == 1);
                assert(clockFrames(events).empty());
            } else {
                publisher.reset();
            }
            publisher.beginBlock(queue, 2, 512, 0.0f,
                                 50.0f, 22050.0f, true);
            const auto events = drain(queue);
            assert(countType(events, MidiTransportEventType::Start) == 1);
            const auto frames = clockFrames(events);
            assert(frames.size() == 1 && frames[0] == 0);
        }
    }
}

void testMinuteOfClockHasNoMissingOrDuplicatePulses() {
    struct Case { float bpm; unsigned pulses; };
    // 60 seconds * BPM / 60 * 24 PPQN, independently hand-counted.
    constexpr Case cases[] = {{48.0f, 1152}, {50.0f, 1200}, {50.5f, 1212},
                              {120.0f, 2880}, {128.0f, 3072}, {182.0f, 4368}};
    for (const unsigned rate : {22050u, 44100u}) {
        for (const unsigned blockFrames : {256u, 512u}) {
            for (const auto& test : cases) {
                ScheduledMidiTransportEventQueue queue;
                MidiTransportClockPublisher publisher;
                const unsigned totalFrames = rate * 60;
                unsigned pulses = 0, starts = 0;
                uint64_t previousFrame = 0;
                const double samplesPerStep = rate * 60.0 / (test.bpm * 4.0);
                for (unsigned at = 0, block = 0; at < totalFrames;
                     at += blockFrames, ++block) {
                    const unsigned length = std::min(blockFrames, totalFrames - at);
                    const float phase = static_cast<float>(
                        std::fmod(at / samplesPerStep, 16.0));
                    publisher.beginBlock(queue, block, length, phase,
                                         test.bpm, rate, true);
                    for (const auto& event : drain(queue)) {
                        assert(event.frameOffset < length);
                        if (event.type == MidiTransportEventType::Start) ++starts;
                        if (event.type != MidiTransportEventType::Clock) continue;
                        const uint64_t frame = at + event.frameOffset;
                        if (pulses > 0) {
                            assert(frame > previousFrame);
                            // Allow sample rounding, not an absent/doubled pulse.
                            if (std::abs(static_cast<double>(frame - previousFrame)
                                         - samplesPerStep / 6.0) >= 2.0) {
                                std::fprintf(stderr, "Clock gap: rate=%u block=%u bpm=%g at=%llu gap=%llu\n",
                                             rate, blockFrames, test.bpm,
                                             static_cast<unsigned long long>(frame),
                                             static_cast<unsigned long long>(frame - previousFrame));
                            }
                            assert(std::abs(static_cast<double>(frame - previousFrame)
                                            - samplesPerStep / 6.0) < 2.0);
                        }
                        previousFrame = frame;
                        ++pulses;
                    }
                }
                assert(pulses == test.pulses);
                assert(starts == 1);
                assert(queue.droppedClockCount() == 0);
            }
        }
    }
}

void test96PpqnTo24PpqnMapping() {
    ScheduledMidiTransportEventQueue queue;
    MidiTransportClockPublisher publisher;

    // One quarter note at 120 BPM / 48 kHz is 24,000 frames. GroovePuter's
    // 96-PPQN timeline maps one MIDI clock to every four internal ticks, so the
    // quarter must contain exactly 24 F8 pulses.
    publisher.beginBlock(queue, 1, 24000, 0.0f, 120.0f, 48000.0f, true);
    const auto events = drain(queue);

    assert(countType(events, MidiTransportEventType::Start) == 1);
    assert(countType(events, MidiTransportEventType::Clock) == 24);
    const auto frames = clockFrames(events);
    assert(frames.size() == 24);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        assert(frames[i] == static_cast<uint16_t>(i * 1000));
    }
}

void testStartStopExactlyOnce() {
    ScheduledMidiTransportEventQueue queue;
    MidiTransportClockPublisher publisher;

    publisher.beginBlock(queue, 10, 4000, 0.0f, 120.0f, 48000.0f, true);
    publisher.beginBlock(queue, 11, 4000, 0.6666667f, 120.0f, 48000.0f, true);
    publisher.beginBlock(queue, 12, 4000, 1.3333334f, 120.0f, 48000.0f, true);
    auto events = drain(queue);
    assert(countType(events, MidiTransportEventType::Start) == 1);
    assert(countType(events, MidiTransportEventType::Stop) == 0);

    publisher.beginBlock(queue, 13, 4000, 2.0f, 120.0f, 48000.0f, false);
    publisher.beginBlock(queue, 14, 4000, 0.0f, 120.0f, 48000.0f, false);
    events = drain(queue);
    assert(countType(events, MidiTransportEventType::Start) == 0);
    assert(countType(events, MidiTransportEventType::Stop) == 1);
}

void testContinuousSongClockDoesNotRestartLifecycle() {
    ScheduledMidiTransportEventQueue queue;
    MidiTransportClockPublisher publisher;

    publisher.beginBlock(queue, 20, 2000, 0.0f, 120.0f, 48000.0f, true);
    drain(queue);
    const uint32_t generation = queue.generation();

    // A Song row transition changes pattern material, not transport state. The
    // phase can cross the bar boundary while the same generation continues.
    publisher.beginBlock(queue, 21, 2000, 15.8f, 120.0f, 48000.0f, true);
    publisher.beginBlock(queue, 22, 2000, 0.1333333f, 120.0f, 48000.0f, true);
    const auto events = drain(queue);

    assert(countType(events, MidiTransportEventType::Start) == 0);
    assert(countType(events, MidiTransportEventType::Stop) == 0);
    assert(countType(events, MidiTransportEventType::Clock) > 0);
    assert(queue.generation() == generation);
}

void testBpmChangeChangesCadenceWithoutRestart() {
    ScheduledMidiTransportEventQueue queue;
    MidiTransportClockPublisher publisher;

    publisher.beginBlock(queue, 30, 24000, 0.0f, 120.0f, 48000.0f, true);
    auto events = drain(queue);
    auto frames = clockFrames(events);
    assert(frames.size() == 24);
    assert(frames[1] - frames[0] == 1000);

    // Continue at the next quarter with doubled BPM. No Start is emitted, and
    // the clock interval halves immediately on the new block anchor.
    publisher.beginBlock(queue, 31, 12000, 4.0f, 240.0f, 48000.0f, true);
    events = drain(queue);
    frames = clockFrames(events);
    assert(countType(events, MidiTransportEventType::Start) == 0);
    assert(countType(events, MidiTransportEventType::Stop) == 0);
    assert(frames.size() == 24);
    assert(frames[1] - frames[0] == 500);
}

void testLifecycleClockAndNoteOrdering() {
    const ScheduledMidiTransportEvent start{
        MidiTransportEventType::Start, 100, 0, 1, 2};
    const ScheduledMidiTransportEvent clock{
        MidiTransportEventType::Clock, 100, 0, 1, 1};
    const ScheduledMidiTransportEvent stop{
        MidiTransportEventType::Stop, 100, 0, 2, 4};
    const ScheduledMusicalEvent note{
        MusicalEvent{
            MusicalEventType::NoteOn,
            MusicalEventSource::PatternPlayer,
            MusicalEventTarget::SynthA,
            0,
            60,
            100,
        },
        100,
        0,
        1,
        1,
    };

    // Equal sample timestamp contract: lifecycle before Clock, and all
    // scheduled transport traffic before scheduled musical traffic.
    assert(scheduledMidiTransportEventBefore(start, clock));
    assert(scheduledMidiTransportEventBefore(stop, clock));
    assert(!scheduledMidiTransportEventBefore(clock, start));
    assert(scheduledMidiTransportEventBeforeMusical(start, note));
    assert(scheduledMidiTransportEventBeforeMusical(clock, note));

    const ScheduledMidiTransportEvent laterClock{
        MidiTransportEventType::Clock, 100, 1, 1, 3};
    assert(!scheduledMidiTransportEventBeforeMusical(laterClock, note));
}

void testClockOverflowPreservesCriticalReserveAndRecovery() {
    ScheduledMidiTransportEventQueue queue;

    std::size_t acceptedClocks = 0;
    while (queue.tryPushClock(1, static_cast<uint16_t>(acceptedClocks % 512))) {
        ++acceptedClocks;
    }
    assert(acceptedClocks ==
           ScheduledMidiTransportEventQueue::kCapacity -
           ScheduledMidiTransportEventQueue::kCriticalReserve);
    assert(queue.droppedClockCount() == 1);

    // Clock pressure cannot consume the reserved lifecycle capacity.
    for (std::size_t i = 0;
         i < ScheduledMidiTransportEventQueue::kCriticalReserve;
         ++i) {
        const auto type = (i & 1u)
            ? MidiTransportEventType::Start
            : MidiTransportEventType::Stop;
        assert(queue.tryPushLifecycle(type, 2, 0));
    }
    assert(queue.criticalOverflowCount() == 0);

    // One more lifecycle event exceeds even the reserved ring slots. It is not
    // silently lost: the bounded critical recovery mailbox records it.
    assert(queue.tryPushLifecycle(MidiTransportEventType::Stop, 3, 0));
    assert(queue.criticalOverflowCount() == 1);

    ScheduledMidiTransportEvent event{};
    while (queue.tryPop(event)) {}
    assert(queue.takePendingCriticalRecovery(event));
    assert(event.type == MidiTransportEventType::Stop);
    assert(queue.criticalRecoveryCount() == 1);
}

void testStopInvalidatesQueuedClockGeneration() {
    ScheduledMidiTransportEventQueue queue;

    assert(queue.tryPushLifecycle(MidiTransportEventType::Start, 1, 0));
    assert(queue.tryPushClock(1, 100));
    assert(queue.tryPushLifecycle(MidiTransportEventType::Stop, 2, 0));

    ScheduledMidiTransportEvent event{};
    assert(queue.tryPop(event));
    assert(event.type == MidiTransportEventType::Start);

    assert(queue.tryPop(event));
    assert(event.type == MidiTransportEventType::Clock);
    assert(!scheduledMidiTransportEventGenerationIsCurrent(
        event, queue.generation()));

    assert(queue.tryPop(event));
    assert(event.type == MidiTransportEventType::Stop);
}
}  // namespace

int main() {
    testClockInsideBlockSurvivesRoundingPastItsEnd();
    testClockExactlyAtBlockEndBelongsToNextBlock();
    testDeferredClockSurvivesTempoChangeButNotStopOrReset();
    testMinuteOfClockHasNoMissingOrDuplicatePulses();
    test96PpqnTo24PpqnMapping();
    testStartStopExactlyOnce();
    testContinuousSongClockDoesNotRestartLifecycle();
    testBpmChangeChangesCadenceWithoutRestart();
    testLifecycleClockAndNoteOrdering();
    testClockOverflowPreservesCriticalReserveAndRecovery();
    testStopInvalidatesQueuedClockGeneration();
    return 0;
}
