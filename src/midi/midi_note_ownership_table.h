#pragma once
#ifndef GROOVEPUTER_MIDI_NOTE_OWNERSHIP_TABLE_H
#define GROOVEPUTER_MIDI_NOTE_OWNERSHIP_TABLE_H

#include <cstddef>
#include <cstdint>

namespace GroovePuterMidi {

// Two-level note ownership for one physical MIDI endpoint.
//
// The previous representation was a pair of dense 16x128 matrices, 4096 B per
// endpoint. That is affordable for a single USB endpoint and not affordable for
// two: the Cardputer DRAM globals budget leaves roughly 4.5 KB of headroom in
// total, and a second endpoint of the old shape consumes almost all of it.
//
// Live notes are bounded by real polyphony, not by the 2048-cell address space,
// so only cells that currently have an owner are stored. Capacity is fixed and
// nothing is ever allocated, so the dispatcher path stays allocation free.
//
// The cell handle deliberately mirrors the previous `uint8_t&` access shape so
// the two-level ownership arithmetic - including the "owners other than SMF"
// subtraction - transcribes without reinterpretation.
//
// Overflow follows the existing NoteOff-criticality invariant. `open()` may
// fail, and a failed `open()` on a NoteOn means nothing reached the wire, so
// nothing needs cleanup. Release paths use `peek()`, which never inserts, so a
// full table can never strand a sounding note.
template <std::size_t Capacity>
class MidiNoteOwnershipTable {
public:
    static constexpr std::size_t kCapacity = Capacity;
    static constexpr uint8_t kChannelCount = 16;
    static constexpr uint8_t kNoteCount = 128;

    struct Cell {
        uint8_t channel;
        uint8_t note;
        uint8_t wire;
        uint8_t smf;
    };

    void clear() { size_ = 0; }

    std::size_t liveCells() const { return size_; }
    bool full() const { return size_ >= kCapacity; }

    uint8_t wireOwnerCount(uint8_t channel, uint8_t note) const {
        const Cell* cell = peek(channel, note);
        return cell ? cell->wire : 0;
    }

    uint8_t smfOwnerCount(uint8_t channel, uint8_t note) const {
        const Cell* cell = peek(channel, note);
        return cell ? cell->smf : 0;
    }

    // Existing cell, or nullptr. Never inserts, so this is the safe accessor
    // for every release and cleanup path.
    const Cell* peek(uint8_t channel, uint8_t note) const {
        if (!valid(channel, note)) return nullptr;
        for (std::size_t i = 0; i < size_; ++i) {
            if (cells_[i].channel == channel && cells_[i].note == note) {
                return &cells_[i];
            }
        }
        return nullptr;
    }

    Cell* peek(uint8_t channel, uint8_t note) {
        return const_cast<Cell*>(
            static_cast<const MidiNoteOwnershipTable*>(this)->peek(channel, note));
    }

    // Existing cell, or a freshly zeroed one. Returns nullptr when the address
    // is invalid or the table is full; callers must treat that as "this NoteOn
    // did not happen".
    Cell* open(uint8_t channel, uint8_t note) {
        if (!valid(channel, note)) return nullptr;
        Cell* existing = peek(channel, note);
        if (existing != nullptr) return existing;
        if (size_ >= kCapacity) return nullptr;
        Cell& cell = cells_[size_++];
        cell.channel = channel;
        cell.note = note;
        cell.wire = 0;
        cell.smf = 0;
        return &cell;
    }

    // Drops every cell that no longer has an owner. Call after a batch of
    // mutations; cell pointers do not survive it.
    void prune() {
        std::size_t i = 0;
        while (i < size_) {
            if (cells_[i].wire == 0 && cells_[i].smf == 0) {
                cells_[i] = cells_[size_ - 1];
                --size_;
                continue;
            }
            ++i;
        }
    }

    bool channelHasWireOwner(uint8_t channel) const {
        for (std::size_t i = 0; i < size_; ++i) {
            if (cells_[i].channel == channel && cells_[i].wire != 0) return true;
        }
        return false;
    }

    // Iteration covers only cells that currently have an owner, which is what
    // every scan in the output path actually wants. Order is unspecified.
    // Mutating `wire`/`smf` through these pointers is allowed; inserting or
    // pruning during iteration is not.
    Cell* begin() { return cells_; }
    Cell* end() { return cells_ + size_; }
    const Cell* begin() const { return cells_; }
    const Cell* end() const { return cells_ + size_; }

private:
    static bool valid(uint8_t channel, uint8_t note) {
        return channel < kChannelCount && note < kNoteCount;
    }

    Cell cells_[kCapacity]{};
    std::size_t size_{0};
};

// Sized for dense SMF material plus pattern and performance lanes with margin,
// and deliberately far below the 2048-cell dense address space.
constexpr std::size_t kMidiEndpointOwnershipCapacity = 128;

using MidiEndpointOwnershipTable =
    MidiNoteOwnershipTable<kMidiEndpointOwnershipCapacity>;

static_assert(sizeof(MidiEndpointOwnershipTable) <= 640,
              "endpoint ownership must stay far below the previous 4096 B "
              "dense representation; two endpoints have to fit the Cardputer "
              "DRAM headroom");

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_NOTE_OWNERSHIP_TABLE_H
