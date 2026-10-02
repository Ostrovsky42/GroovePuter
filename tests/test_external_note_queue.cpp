#include <cassert>
#include <cstdio>

#include "src/midi/external_note_queue.h"

using GroovePuterMidi::ExternalNoteQueue;

int main() {
    ExternalNoteQueue queue;
    ExternalNoteQueue::Event e;
    assert(!queue.pop(e));

    queue.externalNoteOn(60, 90);
    queue.externalNoteOff(60);
    assert(queue.pop(e) && e.on && e.note == 60 && e.velocity == 90);
    assert(queue.pop(e) && !e.on && e.note == 60);
    assert(!queue.pop(e));

    // NoteOn stops at the reserve line; NoteOff can still use the reserve.
    for (unsigned i = 0; i < ExternalNoteQueue::kCapacity; ++i) {
        queue.externalNoteOn(static_cast<uint8_t>(i), 100);
    }
    assert(queue.dropped() == ExternalNoteQueue::kNoteOffReserve);
    for (unsigned i = 0; i < ExternalNoteQueue::kNoteOffReserve; ++i) queue.externalNoteOff(static_cast<uint8_t>(i));
    assert(!queue.takeRecovery());   // every NoteOff found room in the reserve

    // One more NoteOff does not fit: recovery is requested exactly once.
    queue.externalNoteOff(1);
    assert(queue.takeRecovery());
    assert(!queue.takeRecovery());

    // Drain keeps order and wraps.
    unsigned count = 0;
    while (queue.pop(e)) ++count;
    assert(count == ExternalNoteQueue::kCapacity);
    for (int round = 0; round < 100; ++round) {
        queue.externalNoteOn(static_cast<uint8_t>(round), 80);
        assert(queue.pop(e) && e.note == static_cast<uint8_t>(round));
    }
    std::puts("external note queue: PASS");
    return 0;
}
