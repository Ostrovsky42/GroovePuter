#include "smf_timing.h"

#include <algorithm>
#include <limits>
#include <new>

namespace GroovePuterMidi {

bool SmfTimingMap::reserveForEvents(std::size_t maxTimingEvents) {
    try {
        // Each map has one implicit default point in addition to file events.
        tempo_.reserve(maxTimingEvents + 1u);
        signatures_.reserve(maxTimingEvents + 1u);
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

uint32_t SmfTimingMap::ticksPerBeat(uint16_t division, uint8_t denominatorPow2) {
    if (division == 0 || denominatorPow2 > 7) return 0;
    const uint32_t denominator = 1u << denominatorPow2;
    const uint32_t wholeNoteTicks = static_cast<uint32_t>(division) * 4u;
    const uint32_t ticks = wholeNoteTicks / denominator;
    return ticks == 0 ? 1u : ticks;
}

bool SmfTimingMap::build(const SmfDocument& document) {
    division_ = document.division;
    tempo_.clear();
    signatures_.clear();
    if (division_ == 0 || (division_ & 0x8000u) != 0) return false;

    tempo_.push_back(SmfTempoPoint{0, 0, 500000});
    signatures_.push_back(SmfTimeSignaturePoint{
        0,
        0,
        4,
        2,
        ticksPerBeat(division_, 2),
        ticksPerBeat(division_, 2) * 4u,
    });

    uint32_t tempoTick = 0;
    uint64_t tempoMicros = 0;
    uint32_t currentTempo = 500000;

    for (const SmfEvent& event : document.events) {
        if (event.kind != SmfEventKind::Tempo) continue;
        if (event.value == 0) continue;

        const uint32_t deltaTicks = event.tick - tempoTick;
        tempoMicros += (static_cast<uint64_t>(deltaTicks) * currentTempo) /
                       static_cast<uint64_t>(division_);
        tempoTick = event.tick;
        currentTempo = event.value;

        if (!tempo_.empty() && tempo_.back().tick == event.tick) {
            tempo_.back().microsAtTick = tempoMicros;
            tempo_.back().microsPerQuarter = currentTempo;
        } else {
            tempo_.push_back(SmfTempoPoint{
                event.tick,
                tempoMicros,
                currentTempo,
            });
        }
    }

    for (const SmfEvent& event : document.events) {
        if (event.kind != SmfEventKind::TimeSignature) continue;
        const uint8_t numerator = event.data1 == 0 ? 4 : event.data1;
        const uint8_t denominatorPow2 = event.data2;
        const uint32_t beatTicks = ticksPerBeat(division_, denominatorPow2);
        if (beatTicks == 0) return false;
        if (numerator > std::numeric_limits<uint32_t>::max() / beatTicks) return false;
        const uint32_t barTicks = beatTicks * numerator;

        if (event.tick == 0) {
            signatures_[0] = SmfTimeSignaturePoint{
                0,
                0,
                numerator,
                denominatorPow2,
                beatTicks,
                barTicks,
            };
            continue;
        }

        const SmfTimeSignaturePoint& previous = signatures_.back();
        if (event.tick < previous.tick) return false;
        const uint32_t delta = event.tick - previous.tick;
        uint32_t barsElapsed = delta / previous.ticksPerBar;
        if ((delta % previous.ticksPerBar) != 0) ++barsElapsed;

        const uint32_t nextBarIndex = previous.barIndex + barsElapsed;
        if (event.tick == previous.tick) {
            signatures_.back() = SmfTimeSignaturePoint{
                event.tick,
                previous.barIndex,
                numerator,
                denominatorPow2,
                beatTicks,
                barTicks,
            };
        } else {
            signatures_.push_back(SmfTimeSignaturePoint{
                event.tick,
                nextBarIndex,
                numerator,
                denominatorPow2,
                beatTicks,
                barTicks,
            });
        }
    }

    return true;
}

uint64_t SmfTimingMap::tickToMicros(uint32_t tick) const {
    if (!valid()) return 0;
    const auto it = std::upper_bound(
        tempo_.begin(), tempo_.end(), tick,
        [](uint32_t targetTick, const SmfTempoPoint& point) {
            return targetTick < point.tick;
        });
    const SmfTempoPoint& point = it == tempo_.begin() ? tempo_.front() : *(it - 1);
    const uint32_t deltaTicks = tick - point.tick;
    return point.microsAtTick +
           (static_cast<uint64_t>(deltaTicks) * point.microsPerQuarter) /
               static_cast<uint64_t>(division_);
}

uint32_t SmfTimingMap::microsToTick(uint64_t micros) const {
    if (!valid()) return 0;
    const auto it = std::upper_bound(
        tempo_.begin(), tempo_.end(), micros,
        [](uint64_t targetMicros, const SmfTempoPoint& point) {
            return targetMicros < point.microsAtTick;
        });
    const SmfTempoPoint& point = it == tempo_.begin() ? tempo_.front() : *(it - 1);
    const uint64_t deltaMicros = micros - point.microsAtTick;
    if (point.microsPerQuarter == 0) return point.tick;
    const uint64_t deltaTicks =
        (deltaMicros * static_cast<uint64_t>(division_)) /
        static_cast<uint64_t>(point.microsPerQuarter);
    const uint64_t tick = static_cast<uint64_t>(point.tick) + deltaTicks;
    return tick > std::numeric_limits<uint32_t>::max()
        ? std::numeric_limits<uint32_t>::max()
        : static_cast<uint32_t>(tick);
}

uint32_t SmfTimingMap::microsPerQuarterAtTick(uint32_t tick) const {
    if (!valid()) return 500000;
    const auto it = std::upper_bound(
        tempo_.begin(), tempo_.end(), tick,
        [](uint32_t targetTick, const SmfTempoPoint& point) {
            return targetTick < point.tick;
        });
    return (it == tempo_.begin() ? tempo_.front() : *(it - 1)).microsPerQuarter;
}

SmfBarBeat SmfTimingMap::barBeatForTick(uint32_t tick) const {
    SmfBarBeat result;
    if (!valid()) return result;

    const auto it = std::upper_bound(
        signatures_.begin(), signatures_.end(), tick,
        [](uint32_t targetTick, const SmfTimeSignaturePoint& point) {
            return targetTick < point.tick;
        });
    const SmfTimeSignaturePoint& point =
        it == signatures_.begin() ? signatures_.front() : *(it - 1);

    const uint32_t delta = tick - point.tick;
    const uint32_t barsIntoSegment = delta / point.ticksPerBar;
    const uint32_t tickInBar = delta % point.ticksPerBar;
    const uint32_t beatZeroBased = tickInBar / point.ticksPerBeat;

    result.bar = point.barIndex + barsIntoSegment + 1u;
    result.beat = static_cast<uint16_t>(beatZeroBased + 1u);
    result.tickInBeat = tickInBar % point.ticksPerBeat;
    return result;
}

uint32_t SmfTimingMap::tickForBar(uint32_t oneBasedBar) const {
    if (!valid()) return 0;
    if (oneBasedBar < 1) oneBasedBar = 1;
    const uint32_t targetIndex = oneBasedBar - 1u;

    const SmfTimeSignaturePoint* selected = &signatures_.front();
    for (const SmfTimeSignaturePoint& point : signatures_) {
        if (point.barIndex > targetIndex) break;
        selected = &point;
    }

    const uint32_t barsFromPoint = targetIndex - selected->barIndex;
    const uint64_t tick = static_cast<uint64_t>(selected->tick) +
                          static_cast<uint64_t>(barsFromPoint) *
                              selected->ticksPerBar;
    if (tick > std::numeric_limits<uint32_t>::max()) {
        return std::numeric_limits<uint32_t>::max();
    }
    return static_cast<uint32_t>(tick);
}

}  // namespace GroovePuterMidi
