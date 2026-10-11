#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_SCENE_H
#define GROOVEPUTER_PERFORMANCE_SCENE_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "performance_control.h"

namespace GroovePuterPerformance {

struct PerformanceSceneSnapshot {
    PerformanceControlState controls{};
    uint16_t muteMask{0}; // Synth A/B followed by the eight drum lanes.
    uint8_t effectMask{0}; // Delay A/B and drive A/B, discrete at morph completion.
};

enum class SceneTransition : uint8_t { Cut = 0, OneBar, TwoBars, FourBars, EightBars, Count };

class PerformanceSceneBank {
public:
    static constexpr uint8_t kSceneCount = 8;
    static constexpr uint32_t kTicksPerBar = 384;
    static constexpr std::size_t kSerializedSize = 84;

    bool serialize(uint8_t* output, std::size_t capacity) const {
        if (output == nullptr || capacity < kSerializedSize) return false;
        output[0] = 1; // schema version
        output[1] = validMask_;
        std::size_t offset = 2;
        for (uint8_t slot = 0; slot < kSceneCount; ++slot) {
            for (uint8_t value : scenes_[slot].controls.values) output[offset++] = value;
            output[offset++] = static_cast<uint8_t>(scenes_[slot].muteMask & 0xffu);
            output[offset++] = static_cast<uint8_t>(scenes_[slot].muteMask >> 8u);
            output[offset++] = scenes_[slot].effectMask;
        }
        const uint16_t crc = crc16(output, kSerializedSize - 2);
        output[offset++] = static_cast<uint8_t>(crc & 0xffu);
        output[offset] = static_cast<uint8_t>(crc >> 8u);
        return offset + 1u == kSerializedSize;
    }

    bool deserialize(const uint8_t* input, std::size_t size) {
        if (input == nullptr || size != kSerializedSize || input[0] != 1 ||
            crc16(input, kSerializedSize - 2) !=
                static_cast<uint16_t>(input[kSerializedSize - 2] |
                    (static_cast<uint16_t>(input[kSerializedSize - 1]) << 8u))) return false;
        PerformanceSceneSnapshot loaded[kSceneCount]{};
        std::size_t offset = 2;
        for (uint8_t slot = 0; slot < kSceneCount; ++slot) {
            for (uint8_t& value : loaded[slot].controls.values) {
                value = input[offset++];
                if (value > 127u) return false;
            }
            loaded[slot].muteMask = static_cast<uint16_t>(input[offset] |
                (static_cast<uint16_t>(input[offset + 1]) << 8u));
            loaded[slot].effectMask = input[offset + 2];
            offset += 3;
        }
        for (uint8_t slot = 0; slot < kSceneCount; ++slot) scenes_[slot] = loaded[slot];
        validMask_ = input[1];
        activeSlot_ = -1;
        queuedSlot_ = -1;
        morphing_ = false;
        dirty_ = false;
        return true;
    }

    bool capture(uint8_t slot, const PerformanceSceneSnapshot& snapshot) {
        if (slot >= kSceneCount || !validSnapshot(snapshot)) return false;
        scenes_[slot] = snapshot;
        validMask_ |= static_cast<uint8_t>(1u << slot);
        activeSlot_ = static_cast<int8_t>(slot);
        dirty_ = false;
        queuedSlot_ = -1;
        morphing_ = false;
        return true;
    }

    bool scene(uint8_t slot, PerformanceSceneSnapshot& out) const {
        if (slot >= kSceneCount || !(validMask_ & (1u << slot))) return false;
        out = scenes_[slot];
        return true;
    }

    bool recall(uint8_t slot, PerformanceSceneSnapshot& out) {
        if (!scene(slot, out)) return false;
        activeSlot_ = static_cast<int8_t>(slot);
        dirty_ = false;
        queuedSlot_ = -1;
        morphing_ = false;
        return true;
    }

    bool queueRecall(uint8_t slot) {
        if (slot >= kSceneCount || !(validMask_ & (1u << slot))) return false;
        queuedSlot_ = static_cast<int8_t>(slot);
        return true;
    }

    bool onBarBoundary(PerformanceSceneSnapshot& out) {
        if (queuedSlot_ < 0) return false;
        return recall(static_cast<uint8_t>(queuedSlot_), out);
    }

    bool beginMorph(const PerformanceSceneSnapshot& from, uint8_t targetSlot,
                    SceneTransition transition, uint32_t startTick) {
        if (!validSnapshot(from) || transition >= SceneTransition::Count ||
            targetSlot >= kSceneCount || !(validMask_ & (1u << targetSlot))) return false;
        morphFrom_ = from;
        morphTo_ = scenes_[targetSlot];
        morphTargetSlot_ = static_cast<int8_t>(targetSlot);
        morphStartTick_ = startTick;
        morphDurationTicks_ = transitionTicks(transition);
        morphing_ = morphDurationTicks_ != 0;
        queuedSlot_ = morphing_ ? morphTargetSlot_ : -1;
        if (!morphing_) {
            activeSlot_ = morphTargetSlot_;
            dirty_ = false;
        }
        return true;
    }

    bool updateMorph(uint32_t tick, PerformanceSceneSnapshot& out) {
        if (!morphing_) return false;
        if (static_cast<int32_t>(tick - morphStartTick_) < 0) {
            out = morphFrom_;
            return true;
        }
        const uint32_t elapsed = tick - morphStartTick_;
        const uint32_t bounded = elapsed < morphDurationTicks_ ? elapsed : morphDurationTicks_;
        out = morphFrom_;
        for (uint8_t i = 0; i < PerformanceControlState::kControlCount; ++i) {
            const PerformanceControlId id = static_cast<PerformanceControlId>(i);
            if (!isContinuous(id)) {
                if (bounded == morphDurationTicks_) out.controls.values[i] = morphTo_.controls.values[i];
                continue;
            }
            const int32_t a = morphFrom_.controls.values[i];
            const int32_t b = morphTo_.controls.values[i];
            const int32_t scaled = a * static_cast<int32_t>(morphDurationTicks_ - bounded) +
                                   b * static_cast<int32_t>(bounded);
            out.controls.values[i] = static_cast<uint8_t>(
                (scaled + static_cast<int32_t>(morphDurationTicks_ / 2u)) /
                static_cast<int32_t>(morphDurationTicks_));
        }
        // Discrete state changes only when the transition completes.
        if (bounded == morphDurationTicks_) {
            out.muteMask = morphTo_.muteMask;
            out.effectMask = morphTo_.effectMask;
        } else {
            out.muteMask = morphFrom_.muteMask;
            out.effectMask = morphFrom_.effectMask;
        }
        if (bounded == morphDurationTicks_) {
            morphing_ = false;
            activeSlot_ = morphTargetSlot_;
            dirty_ = false;
            queuedSlot_ = -1;
        }
        return true;
    }

    bool finishMorph(PerformanceSceneSnapshot& out) {
        if (!morphing_) return false;
        out = morphTo_;
        morphing_ = false;
        activeSlot_ = morphTargetSlot_;
        dirty_ = false;
        queuedSlot_ = -1;
        return true;
    }

    void cancelMorph() {
        morphing_ = false;
        queuedSlot_ = -1;
    }

    void markCurrentState(const PerformanceSceneSnapshot& snapshot) {
        dirty_ = activeSlot_ >= 0 &&
            !sameSnapshot(scenes_[static_cast<uint8_t>(activeSlot_)], snapshot);
    }

    int8_t activeSlot() const { return activeSlot_; }
    int8_t queuedSlot() const { return queuedSlot_; }
    bool dirty() const { return dirty_; }
    uint32_t morphEndTick() const { return morphStartTick_ + morphDurationTicks_; }
    bool valid(uint8_t slot) const {
        return slot < kSceneCount && (validMask_ & (1u << slot));
    }
    bool morphing() const { return morphing_; }
    static uint32_t transitionTicks(SceneTransition transition) {
        switch (transition) {
            case SceneTransition::Cut: return 0;
            case SceneTransition::OneBar: return kTicksPerBar;
            case SceneTransition::TwoBars: return 2u * kTicksPerBar;
            case SceneTransition::FourBars: return 4u * kTicksPerBar;
            case SceneTransition::EightBars: return 8u * kTicksPerBar;
            case SceneTransition::Count: break;
        }
        return 0;
    }

private:
    static uint16_t crc16(const uint8_t* bytes, std::size_t size) {
        uint16_t crc = 0xffffu;
        for (std::size_t i = 0; i < size; ++i) {
            crc ^= static_cast<uint16_t>(bytes[i]) << 8u;
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1u) ^ 0x1021u)
                                      : static_cast<uint16_t>(crc << 1u);
        }
        return crc;
    }
    static bool isContinuous(PerformanceControlId id) {
        return id != PerformanceControlId::Density;
    }
    static bool validSnapshot(const PerformanceSceneSnapshot& snapshot) {
        for (uint8_t value : snapshot.controls.values) if (value > 127u) return false;
        return true;
    }
    static bool sameSnapshot(const PerformanceSceneSnapshot& a,
                             const PerformanceSceneSnapshot& b) {
        for (uint8_t i = 0; i < PerformanceControlState::kControlCount; ++i)
            if (a.controls.values[i] != b.controls.values[i]) return false;
        return a.muteMask == b.muteMask && a.effectMask == b.effectMask;
    }

    PerformanceSceneSnapshot scenes_[kSceneCount]{};
    PerformanceSceneSnapshot morphFrom_{};
    PerformanceSceneSnapshot morphTo_{};
    uint8_t validMask_{0};
    int8_t activeSlot_{-1};
    int8_t queuedSlot_{-1};
    int8_t morphTargetSlot_{-1};
    bool dirty_{false};
    bool morphing_{false};
    uint32_t morphStartTick_{0};
    uint32_t morphDurationTicks_{0};
};

static_assert(sizeof(PerformanceSceneBank) <= 144,
              "eight performance scenes and morph state must stay bounded");

static_assert(std::is_trivially_copyable<PerformanceSceneSnapshot>::value,
              "Scene snapshot must be fixed and copyable");
static_assert(sizeof(PerformanceSceneSnapshot) <= 16,
              "Performance scene snapshot exceeded bounded storage");

} // namespace GroovePuterPerformance

#endif // GROOVEPUTER_PERFORMANCE_SCENE_H
