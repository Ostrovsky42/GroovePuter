#pragma once

#include <cstdint>
#include <vector>

#include "smf_document.h"

namespace GroovePuterMidi {

struct SmfTempoPoint {
    uint32_t tick{0};
    uint64_t microsAtTick{0};
    uint32_t microsPerQuarter{500000};
};

struct SmfTimeSignaturePoint {
    uint32_t tick{0};
    uint32_t barIndex{0}; // zero-based bar containing this signature start
    uint8_t numerator{4};
    uint8_t denominatorPow2{2};
    uint32_t ticksPerBeat{0};
    uint32_t ticksPerBar{0};
};

struct SmfBarBeat {
    uint32_t bar{1};      // one-based for UI
    uint16_t beat{1};     // one-based for UI
    uint32_t tickInBeat{0};
};

class SmfTimingMap {
public:
    bool build(const SmfDocument& document);

    bool valid() const { return division_ != 0 && !tempo_.empty() && !signatures_.empty(); }
    uint16_t division() const { return division_; }

    uint64_t tickToMicros(uint32_t tick) const;
    uint32_t microsToTick(uint64_t micros) const;
    uint32_t microsPerQuarterAtTick(uint32_t tick) const;
    SmfBarBeat barBeatForTick(uint32_t tick) const;
    uint32_t tickForBar(uint32_t oneBasedBar) const;

    const std::vector<SmfTempoPoint>& tempoPoints() const { return tempo_; }
    const std::vector<SmfTimeSignaturePoint>& timeSignaturePoints() const {
        return signatures_;
    }

private:
    static uint32_t ticksPerBeat(uint16_t division, uint8_t denominatorPow2);

    uint16_t division_{0};
    std::vector<SmfTempoPoint> tempo_;
    std::vector<SmfTimeSignaturePoint> signatures_;
};

}  // namespace GroovePuterMidi
