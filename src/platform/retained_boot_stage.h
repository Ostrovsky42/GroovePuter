#pragma once

#include <cstdint>

// POD with no initializer: place in RTC_NOINIT_ATTR so software/panic resets
// cannot erase the evidence before setup reads it. Cold boots must ignore it.
struct RetainedBootStage {
    uint32_t magic;
    uint32_t stage;
    uint32_t complement;

    uint32_t previous(bool retainedReset) const {
        return retainedReset && magic == kMagic && complement == ~stage
            ? stage : 0;
    }

    void record(uint32_t nextStage) volatile {
        magic = 0;
        stage = nextStage;
        complement = ~nextStage;
        magic = kMagic;
    }

private:
    static constexpr uint32_t kMagic = 0x47504253u;  // GPBS
};
