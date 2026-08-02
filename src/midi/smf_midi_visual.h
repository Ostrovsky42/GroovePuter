#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace GroovePuterMidi {

struct SmfMidiVisualSnapshot {
    uint32_t epoch{0};
    uint32_t pulseCounter{0};
    uint32_t droppedEvents{0};
    uint8_t note{60};
    uint8_t velocity{0};
    uint8_t channel{0};
};

class SmfMidiVisualTimeline {
public:
    static constexpr std::size_t kCapacity = 64;

    void reset() {
        head_ = 0;
        size_ = 0;
        ++snapshot_.epoch;
        snapshot_.pulseCounter = 0;
        snapshot_.droppedEvents = 0;
        snapshot_.note = 60;
        snapshot_.velocity = 0;
        snapshot_.channel = 0;
    }

    void clearPending() {
        head_ = 0;
        size_ = 0;
        snapshot_.velocity = 0;
    }

    void queue(uint32_t tick, uint8_t note, uint8_t velocity, uint8_t channel) {
        if (size_ == kCapacity) {
            head_ = (head_ + 1u) % kCapacity;
            --size_;
            ++snapshot_.droppedEvents;
        }
        const std::size_t index = (head_ + size_) % kCapacity;
        events_[index] = Event{tick, note, velocity, channel};
        ++size_;
    }

    SmfMidiVisualSnapshot advanceTo(uint32_t currentTick) {
        uint32_t consumed = 0;
        uint8_t peakVelocity = 0;
        Event last{};
        while (size_ > 0 && events_[head_].tick <= currentTick) {
            const Event event = events_[head_];
            head_ = (head_ + 1u) % kCapacity;
            --size_;
            ++consumed;
            if (event.velocity >= peakVelocity) peakVelocity = event.velocity;
            last = event;
        }
        if (consumed > 0) {
            snapshot_.pulseCounter += consumed;
            snapshot_.note = last.note;
            snapshot_.velocity = peakVelocity;
            snapshot_.channel = last.channel;
        }
        return snapshot_;
    }

    const SmfMidiVisualSnapshot& snapshot() const { return snapshot_; }
    std::size_t pending() const { return size_; }

private:
    struct Event {
        uint32_t tick{0};
        uint8_t note{60};
        uint8_t velocity{0};
        uint8_t channel{0};
    };

    std::array<Event, kCapacity> events_{};
    std::size_t head_{0};
    std::size_t size_{0};
    SmfMidiVisualSnapshot snapshot_{};
};

static_assert(sizeof(SmfMidiVisualTimeline) < 1024,
              "SMF MIDI visual timeline must remain bounded");

}  // namespace GroovePuterMidi
