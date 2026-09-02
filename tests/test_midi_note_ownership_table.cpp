#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/midi/midi_note_ownership_table.h"

using GroovePuterMidi::MidiNoteOwnershipTable;
using GroovePuterMidi::MidiEndpointOwnershipTable;

namespace {

void emptyTableReportsNoOwners() {
    MidiEndpointOwnershipTable table;
    assert(table.liveCells() == 0);
    assert(table.wireOwnerCount(0, 60) == 0);
    assert(table.smfOwnerCount(0, 60) == 0);
    assert(table.peek(0, 60) == nullptr);
    assert(!table.channelHasWireOwner(0));
}

void openInsertsZeroedCellAndPeekFindsIt() {
    MidiEndpointOwnershipTable table;
    auto* cell = table.open(3, 64);
    assert(cell != nullptr);
    assert(cell->wire == 0 && cell->smf == 0);
    assert(cell->channel == 3 && cell->note == 64);
    assert(table.liveCells() == 1);

    ++cell->wire;
    assert(table.wireOwnerCount(3, 64) == 1);
    assert(table.peek(3, 64) == cell);
    // A second open must return the same cell, not a duplicate.
    assert(table.open(3, 64) == cell);
    assert(table.liveCells() == 1);
}

void twoLevelOwnershipArithmeticSurvives() {
    // Transcribes the release path: SMF and one other owner hold the same
    // wire note, so dropping SMF must not silence the wire.
    MidiEndpointOwnershipTable table;
    auto* cell = table.open(9, 38);
    assert(cell != nullptr);
    ++cell->wire;            // pattern owner
    ++cell->wire; ++cell->smf;  // SMF owner

    assert(table.wireOwnerCount(9, 38) == 2);
    assert(table.smfOwnerCount(9, 38) == 1);

    const uint8_t otherOwners = cell->wire > cell->smf
        ? static_cast<uint8_t>(cell->wire - cell->smf)
        : 0;
    assert(otherOwners == 1);

    cell->wire = otherOwners;
    cell->smf = 0;
    table.prune();
    assert(table.wireOwnerCount(9, 38) == 1);
    assert(table.liveCells() == 1);
}

void pruneDropsOnlyFullyReleasedCells() {
    MidiEndpointOwnershipTable table;
    table.open(0, 60)->wire = 1;
    table.open(1, 61)->wire = 0;
    table.open(2, 62)->smf = 1;
    assert(table.liveCells() == 3);

    table.prune();
    assert(table.liveCells() == 2);
    assert(table.wireOwnerCount(0, 60) == 1);
    assert(table.peek(1, 61) == nullptr);
    assert(table.smfOwnerCount(2, 62) == 1);
}

void channelScanSeesOnlyWireOwners() {
    MidiEndpointOwnershipTable table;
    table.open(5, 40)->smf = 1;  // SMF-only, no wire owner
    assert(!table.channelHasWireOwner(5));
    table.open(5, 41)->wire = 1;
    assert(table.channelHasWireOwner(5));
    assert(!table.channelHasWireOwner(6));
}

void invalidAddressesAreRejectedNotClamped() {
    // Clamping a bad channel into a valid one would silently move ownership
    // onto a wire that never sounded the note.
    MidiEndpointOwnershipTable table;
    assert(table.open(16, 60) == nullptr);
    assert(table.open(0, 128) == nullptr);
    assert(table.peek(16, 60) == nullptr);
    assert(table.wireOwnerCount(16, 60) == 0);
    assert(table.liveCells() == 0);
}

void fullTableRefusesNewCellsButKeepsExistingOnes() {
    MidiNoteOwnershipTable<4> table;
    for (uint8_t i = 0; i < 4; ++i) {
        auto* cell = table.open(0, static_cast<uint8_t>(60 + i));
        assert(cell != nullptr);
        cell->wire = 1;
    }
    assert(table.full());

    // A NoteOn that cannot be recorded must fail loudly to its caller. It has
    // no cleanup obligation, so refusing it is safe.
    assert(table.open(0, 70) == nullptr);

    // Release paths never insert, so a full table cannot strand a live note.
    auto* existing = table.peek(0, 60);
    assert(existing != nullptr);
    existing->wire = 0;
    table.prune();
    assert(!table.full());
    assert(table.open(0, 70) != nullptr);
}

void iterationVisitsExactlyTheLiveCells() {
    MidiEndpointOwnershipTable table;
    table.open(0, 60)->wire = 2;
    table.open(1, 61)->smf = 1;
    table.open(2, 62)->wire = 1;

    std::size_t visited = 0;
    unsigned wireTotal = 0;
    for (const auto& cell : table) {
        ++visited;
        wireTotal += cell.wire;
    }
    assert(visited == 3);
    assert(wireTotal == 3);
}

void clearForgetsEverything() {
    MidiEndpointOwnershipTable table;
    table.open(0, 60)->wire = 1;
    table.open(1, 61)->smf = 1;
    table.clear();
    assert(table.liveCells() == 0);
    assert(table.wireOwnerCount(0, 60) == 0);
    assert(table.smfOwnerCount(1, 61) == 0);
}

void staysWithinTheDramBudget() {
    // The whole point of the representation: two endpoints must fit where one
    // dense pair used to sit.
    static_assert(sizeof(MidiEndpointOwnershipTable) <= 640, "budget");
    static_assert(2 * sizeof(MidiEndpointOwnershipTable) < 16 * 128 * 2,
                  "two sparse endpoints must cost less than one dense pair");
}

}  // namespace

int main() {
    emptyTableReportsNoOwners();
    openInsertsZeroedCellAndPeekFindsIt();
    twoLevelOwnershipArithmeticSurvives();
    pruneDropsOnlyFullyReleasedCells();
    channelScanSeesOnlyWireOwners();
    invalidAddressesAreRejectedNotClamped();
    fullTableRefusesNewCellsButKeepsExistingOnes();
    iterationVisitsExactlyTheLiveCells();
    clearForgetsEverything();
    staysWithinTheDramBudget();
    return 0;
}
