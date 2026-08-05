#pragma once
#ifndef GROOVEPUTER_SMF_SESSION_GENERATION_H
#define GROOVEPUTER_SMF_SESSION_GENERATION_H

#include <atomic>
#include <cstdint>

namespace GroovePuterMidi {
namespace SmfSessionGenerationDetail {

constexpr uint32_t kActiveBit = uint32_t{1} << 31u;
constexpr uint32_t kMutationBit = uint32_t{1} << 30u;
constexpr uint32_t kGenerationMask = kMutationBit - 1u;

inline std::atomic<uint32_t>& sessionWord() {
    static std::atomic<uint32_t> word{0u};
    return word;
}

}  // namespace SmfSessionGenerationDetail

static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t),
              "SMF session identity must use one 32-bit global word");

inline uint32_t smfSessionGeneration() {
    using namespace SmfSessionGenerationDetail;
    const uint32_t word = sessionWord().load(std::memory_order_acquire);
    return (word & kActiveBit) != 0u && (word & kMutationBit) == 0u
        ? word & kGenerationMask
        : 0u;
}

inline uint32_t smfBeginSessionOpen() {
    using namespace SmfSessionGenerationDetail;
    std::atomic<uint32_t>& word = sessionWord();
    uint32_t current = word.load(std::memory_order_acquire);
    while (true) {
        if ((current & kMutationBit) != 0u) {
            current = word.load(std::memory_order_acquire);
            continue;
        }
        uint32_t next = (current & kGenerationMask) + 1u;
        if (next == 0u || next > kGenerationMask) next = 1u;
        if (word.compare_exchange_weak(current, next,
                                       std::memory_order_acq_rel,
                                       std::memory_order_acquire)) {
            return next;
        }
    }
}

inline bool smfCompleteSessionOpen(uint32_t generation) {
    using namespace SmfSessionGenerationDetail;
    if (generation == 0u || generation > kGenerationMask) return false;
    uint32_t expected = generation;
    return sessionWord().compare_exchange_strong(
        expected, generation | kActiveBit,
        std::memory_order_release,
        std::memory_order_acquire);
}

class SmfSessionMutationGuard {
public:
    explicit SmfSessionMutationGuard(uint32_t generation)
        : generation_(generation), locked_(lock(generation)) {}

    ~SmfSessionMutationGuard() {
        if (!locked_) return;
        using namespace SmfSessionGenerationDetail;
        uint32_t expected = generation_ | kMutationBit;
        sessionWord().compare_exchange_strong(
            expected, generation_ | kActiveBit,
            std::memory_order_release,
            std::memory_order_relaxed);
    }

    SmfSessionMutationGuard(const SmfSessionMutationGuard&) = delete;
    SmfSessionMutationGuard& operator=(const SmfSessionMutationGuard&) = delete;

    explicit operator bool() const { return locked_; }

private:
    static bool lock(uint32_t generation) {
        using namespace SmfSessionGenerationDetail;
        if (generation == 0u || generation > kGenerationMask) return false;
        uint32_t expected = generation | kActiveBit;
        return sessionWord().compare_exchange_strong(
            expected, generation | kMutationBit,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    uint32_t generation_{0u};
    bool locked_{false};
};

inline bool smfSnapshotGenerationsMatch(uint32_t current,
                                        uint32_t first,
                                        uint32_t second,
                                        uint32_t third = 0u) {
    if (current == 0u || first != current || second != current) return false;
    return third == 0u || third == current;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_SESSION_GENERATION_H
