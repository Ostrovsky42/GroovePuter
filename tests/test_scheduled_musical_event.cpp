#include "../src/midi/scheduled_musical_event.h"

#include <cassert>
#include <cstdint>
#include <type_traits>

namespace {

ScheduledMusicalEvent eventAt(uint32_t blockSequence,
                              uint16_t frameOffset,
                              uint32_t generation,
                              uint32_t publicationSequence) {
    ScheduledMusicalEvent scheduled{};
    scheduled.event.type = MusicalEventType::NoteOn;
    scheduled.event.source = MusicalEventSource::PatternPlayer;
    scheduled.event.target = MusicalEventTarget::SynthA;
    scheduled.event.note = 60;
    scheduled.event.velocity = 100;
    scheduled.blockSequence = blockSequence;
    scheduled.frameOffset = frameOffset;
    scheduled.generation = generation;
    scheduled.publicationSequence = publicationSequence;
    return scheduled;
}

void testFrameOrdering() {
    const auto early = eventAt(12, 31, 7, 20);
    const auto late = eventAt(12, 400, 7, 21);

    assert(scheduledMusicalEventBefore(early, late));
    assert(!scheduledMusicalEventBefore(late, early));
}

void testPublicationOrderingAtEqualFrame() {
    const auto first = eventAt(12, 128, 7, 100);
    const auto second = eventAt(12, 128, 7, 101);

    assert(scheduledMusicalEventBefore(first, second));
    assert(!scheduledMusicalEventBefore(second, first));
}

void testBlockOrdering() {
    const auto previous = eventAt(44, 511, 7, 200);
    const auto next = eventAt(45, 0, 7, 201);

    assert(scheduledMusicalEventBefore(previous, next));
    assert(!scheduledMusicalEventBefore(next, previous));
}

void testSequenceWrapAround() {
    const auto previous = eventAt(UINT32_MAX, 511, 7, UINT32_MAX);
    const auto next = eventAt(0, 0, 7, 0);

    assert(scheduledMusicalEventBefore(previous, next));
    assert(!scheduledMusicalEventBefore(next, previous));
}

void testFrameValidation() {
    const auto valid = eventAt(1, 511, 3, 1);
    const auto invalid = eventAt(1, 512, 3, 2);

    assert(scheduledMusicalEventFrameIsValid(valid, 512));
    assert(!scheduledMusicalEventFrameIsValid(invalid, 512));
    assert(!scheduledMusicalEventFrameIsValid(valid, 0));
}

void testGenerationValidation() {
    const auto scheduled = eventAt(9, 64, 12, 5);

    assert(scheduledMusicalEventGenerationIsCurrent(scheduled, 12));
    assert(!scheduledMusicalEventGenerationIsCurrent(scheduled, 13));
}

void testIdenticalEventsAreNotOrdered() {
    const auto scheduled = eventAt(5, 20, 2, 8);
    assert(!scheduledMusicalEventBefore(scheduled, scheduled));
}

}  // namespace

int main() {
    static_assert(std::is_trivially_copyable<ScheduledMusicalEvent>::value,
                  "scheduled MIDI events must remain queue-safe POD values");
    static_assert(std::is_standard_layout<ScheduledMusicalEvent>::value,
                  "scheduled MIDI events must remain standard-layout values");

    testFrameOrdering();
    testPublicationOrderingAtEqualFrame();
    testBlockOrdering();
    testSequenceWrapAround();
    testFrameValidation();
    testGenerationValidation();
    testIdenticalEventsAreNotOrdered();
}
