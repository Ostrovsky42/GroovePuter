#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_INPUT_FANOUT_H
#define GROOVEPUTER_PERFORMANCE_INPUT_FANOUT_H

#include <cstdint>

#include "performance_input_observer.h"

// PerformanceKeyboard has one input observer slot. The PARTNER
// (CallResponseListener) and the LOOPER both listen to PERFORM input, so the
// slot holds this fixed two-way fan-out: same call, same thread (main loop),
// first then second. No allocation; either pointer may be null.
class PerformanceInputFanout final : public PerformanceInputObserver {
public:
    constexpr PerformanceInputFanout() = default;
    void set(PerformanceInputObserver* first, PerformanceInputObserver* second) {
        first_ = first;
        second_ = second;
    }
    void onPerformanceInputNote(bool on, uint16_t tag, uint8_t note,
                                uint8_t velocity) override {
        if (first_) first_->onPerformanceInputNote(on, tag, note, velocity);
        if (second_) second_->onPerformanceInputNote(on, tag, note, velocity);
    }

private:
    PerformanceInputObserver* first_{nullptr};
    PerformanceInputObserver* second_{nullptr};
};

#endif  // GROOVEPUTER_PERFORMANCE_INPUT_FANOUT_H
