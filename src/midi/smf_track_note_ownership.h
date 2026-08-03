#pragma once
#ifndef GROOVEPUTER_SMF_TRACK_NOTE_OWNERSHIP_H
#define GROOVEPUTER_SMF_TRACK_NOTE_OWNERSHIP_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace GroovePuterMidi {

// Bounded ownership used by immediate SMF track mute. It records only active
// track/channel/note tuples instead of allocating a 64 x 16 x 128 matrix.
template <std::size_t Capacity = 128>
class SmfTrackNoteOwnership {
public:
    struct Entry {
        uint8_t track{0};
        uint8_t channel{0};
        uint8_t note{0};
        uint8_t count{0};
    };

    bool acquire(uint8_t track, uint8_t channel, uint8_t note) {
        channel &= 0x0Fu;
        note &= 0x7Fu;
        for (Entry& entry : entries_) {
            if (entry.count != 0 && entry.track == track &&
                entry.channel == channel && entry.note == note) {
                if (entry.count != 0xFFu) ++entry.count;
                return true;
            }
        }
        for (Entry& entry : entries_) {
            if (entry.count == 0) {
                entry = Entry{track, channel, note, 1};
                ++activeEntries_;
                return true;
            }
        }
        overflowed_ = true;
        return false;
    }

    bool release(uint8_t track, uint8_t channel, uint8_t note) {
        channel &= 0x0Fu;
        note &= 0x7Fu;
        for (Entry& entry : entries_) {
            if (entry.count == 0 || entry.track != track ||
                entry.channel != channel || entry.note != note) {
                continue;
            }
            if (--entry.count == 0) {
                entry = {};
                if (activeEntries_ != 0) --activeEntries_;
            }
            return true;
        }
        return false;
    }

    template <typename ReleaseFn>
    std::size_t releaseTrack(uint8_t track, ReleaseFn&& releaseFn) {
        std::size_t released = 0;
        for (Entry& entry : entries_) {
            if (entry.count == 0 || entry.track != track) continue;
            const uint8_t count = entry.count;
            for (uint8_t i = 0; i < count; ++i) {
                releaseFn(entry.channel, entry.note);
                ++released;
            }
            entry = {};
            if (activeEntries_ != 0) --activeEntries_;
        }
        return released;
    }

    template <typename ReleaseFn>
    std::size_t releaseAll(ReleaseFn&& releaseFn) {
        std::size_t released = 0;
        for (Entry& entry : entries_) {
            if (entry.count == 0) continue;
            const uint8_t count = entry.count;
            for (uint8_t i = 0; i < count; ++i) {
                releaseFn(entry.channel, entry.note);
                ++released;
            }
            entry = {};
        }
        activeEntries_ = 0;
        return released;
    }

    void clearWithoutRelease() {
        for (Entry& entry : entries_) entry = {};
        activeEntries_ = 0;
        overflowed_ = false;
    }

    std::size_t activeEntries() const { return activeEntries_; }
    bool overflowed() const { return overflowed_; }
    void clearOverflow() { overflowed_ = false; }

private:
    std::array<Entry, Capacity> entries_{};
    std::size_t activeEntries_{0};
    bool overflowed_{false};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_TRACK_NOTE_OWNERSHIP_H
