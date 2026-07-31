#include <cassert>
#include <cstdint>
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
    test96PpqnTo24PpqnMapping();
    testStartStopExactlyOnce();
    testContinuousSongClockDoesNotRestartLifecycle();
    testBpmChangeChangesCadenceWithoutRestart();
    testLifecycleClockAndNoteOrdering();
    testClockOverflowPreservesCriticalReserveAndRecovery();
    testStopInvalidatesQueuedClockGeneration();
    return 0;
}
