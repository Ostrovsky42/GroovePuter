#pragma once

#include <stdint.h>

class DeterministicRng {
public:
    explicit DeterministicRng(uint32_t seed) : state_(normalizeSeed(seed)) {}

    // xorshift32 by George Marsaglia. All operations use uint32_t so
    // wraparound is defined and identical on host and Xtensa builds.
    uint32_t next() {
        uint32_t value = state_;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state_ = value;
        return value;
    }

    // Rejection sampling avoids the modulo bias of next() % upperExclusive.
    // A zero bound has no valid output range, so it returns zero without
    // advancing the stream.
    uint32_t bounded(uint32_t upperExclusive) {
        if (upperExclusive == 0) {
            return 0;
        }

        const uint32_t threshold =
            static_cast<uint32_t>(-upperExclusive) % upperExclusive;
        for (;;) {
            const uint32_t value = next();
            if (value >= threshold) {
                return value % upperExclusive;
            }
        }
    }

private:
    // xorshift32 has an absorbing all-zero state. Normalize zero to a fixed,
    // non-zero constant so every accepted seed produces a usable stream.
    static constexpr uint32_t normalizeSeed(uint32_t seed) {
        return seed == 0 ? 0x6D2B79F5u : seed;
    }

    uint32_t state_;
};

static_assert(sizeof(DeterministicRng) == 4,
              "DeterministicRng must keep exactly four bytes of state");
