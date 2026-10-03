#pragma once
#ifndef GROOVEPUTER_MIDI_LATENCY_HISTOGRAM_H
#define GROOVEPUTER_MIDI_LATENCY_HISTOGRAM_H

#include <cstdint>

namespace GroovePuterMidi {

// Fixed-size latency histogram (acceptance diagnostics). Bucket width 250 us, the last bucket
// collects everything from 3.75 ms up. Percentiles are reported as the upper edge of the bucket
// that contains them, so the resolution is 250 us; max is exact. No allocation, single writer.
class LatencyHistogram {
public:
    static constexpr uint32_t kBuckets = 16;
    static constexpr uint32_t kBucketUs = 250;

    void add(uint32_t microseconds) {
        uint32_t bucket = microseconds / kBucketUs;
        if (bucket >= kBuckets) bucket = kBuckets - 1;
        ++buckets_[bucket];
        ++count_;
        if (microseconds > max_) max_ = microseconds;
    }

    uint32_t count() const { return count_; }
    uint32_t maxUs() const { return max_; }

    // Upper edge of the bucket holding the p-th percentile (0..100); 0 when empty.
    uint32_t percentileUs(uint32_t percent) const {
        if (count_ == 0) return 0;
        const uint32_t target = (count_ * percent + 99u) / 100u;  // ceil
        uint32_t seen = 0;
        for (uint32_t i = 0; i < kBuckets; ++i) {
            seen += buckets_[i];
            if (seen >= target && seen > 0) return (i + 1) * kBucketUs;
        }
        return kBuckets * kBucketUs;
    }

    void reset() { *this = LatencyHistogram{}; }

private:
    uint32_t buckets_[kBuckets]{};
    uint32_t count_{0};
    uint32_t max_{0};
};

}  // namespace GroovePuterMidi

#endif
