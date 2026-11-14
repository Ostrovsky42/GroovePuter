#pragma once
// Tilt to MIDI CC offset: pure, host-testable pieces. No sensor, no SDL, no
// allocation. Used only by the GROOVEPUTER_TILT experiment; nothing includes
// this header without that flag.
//
// Chain per axis: accelerometer vector -> angle (deg) -> one-pole low pass
// (~120 ms) -> delta from the zero captured at enable -> dead zone +-5 deg ->
// full scale at +-30 deg -> integer offset -63..+63 -> hysteresis of one step.
#include <cmath>
#include <cstdint>

#include "src/midi/midi_realtime_word.h"

namespace GroovePuterTilt {

inline constexpr float kDeadzoneDeg = 5.0f;
inline constexpr float kRangeDeg = 30.0f;     // delta that gives full scale
inline constexpr int kMaxOffset = 63;
inline constexpr float kLpfTauMs = 120.0f;
inline constexpr uint32_t kReadIntervalMs = 20;
inline constexpr float kGravityTolerance = 0.35f;   // | |a| - 1 g | above this is not a held tilt
inline constexpr uint32_t kGravityHoldMs = 300;     // ... for this long: switch off
inline constexpr float kRadToDeg = 57.29577951f;

// Angle of the device about one axis, in degrees (-90..90). `a` is the
// component along the axis, `p` and `q` the other two. Independent of the
// others and free of wrap-around, unlike atan2(a, p).
inline float axisAngleDeg(float a, float p, float q) {
    return std::atan2(a, std::sqrt(p * p + q * q)) * kRadToDeg;
}
inline float angleXDeg(float ax, float ay, float az) { return axisAngleDeg(ax, ay, az); }
inline float angleYDeg(float ax, float ay, float az) { return axisAngleDeg(ay, ax, az); }

// Delta angle (degrees from the captured zero) to a CC offset -63..+63.
// Zero inside the dead zone, continuous from its edge, saturating at kRangeDeg.
inline int offsetForDelta(float deltaDeg) {
    const float mag = deltaDeg < 0.0f ? -deltaDeg : deltaDeg;
    if (mag <= kDeadzoneDeg) return 0;
    float r = (mag - kDeadzoneDeg) / (kRangeDeg - kDeadzoneDeg);
    if (r > 1.0f) r = 1.0f;
    const int steps = static_cast<int>(r * static_cast<float>(kMaxOffset) + 0.5f);
    return deltaDeg < 0.0f ? -steps : steps;
}

class TiltAxis {
public:
    // Forget the zero and the filter; the next sample is captured as zero.
    void reset() { captured_ = false; last_ = 0; }

    // Feed one angle sample taken dtMs after the previous one. Returns the
    // current offset step (-63..+63).
    int update(float angleDeg, uint32_t dtMs) {
        if (!captured_) {
            captured_ = true;
            zero_ = angleDeg;
            filt_ = angleDeg;
            last_ = 0;
            return 0;
        }
        const float dt = static_cast<float>(dtMs);
        const float alpha = dt / (kLpfTauMs + dt);
        filt_ += alpha * (angleDeg - filt_);
        const int cand = offsetForDelta(filt_ - zero_);
        const int diff = cand - last_;
        // Hysteresis: a change of one step is ignored, except at the ends
        // (zero and full scale), which must always be reachable exactly.
        if (cand == 0 || cand == kMaxOffset || cand == -kMaxOffset || diff >= 2 || diff <= -2) {
            last_ = cand;
        }
        return last_;
    }

    bool captured() const { return captured_; }
    int offset() const { return last_; }
    float zero() const { return zero_; }

private:
    float zero_ = 0.0f;
    float filt_ = 0.0f;
    int8_t last_ = 0;
    bool captured_ = false;
};

// Shared word: bit 0 active, bits 8..15 X offset (int8), bits 16..23 Y offset.
// Written by loop(), read by the MIDI dispatcher and the header.
inline uint32_t packOffsets(bool active, int x, int y) {
    if (!active) return 0u;
    return 1u | (static_cast<uint32_t>(static_cast<uint8_t>(static_cast<int8_t>(x))) << 8u) |
           (static_cast<uint32_t>(static_cast<uint8_t>(static_cast<int8_t>(y))) << 16u);
}
inline bool wordActive(uint32_t w) { return (w & 1u) != 0u; }
inline int wordX(uint32_t w) { return static_cast<int8_t>(static_cast<uint8_t>(w >> 8u)); }
inline int wordY(uint32_t w) { return static_cast<int8_t>(static_cast<uint8_t>(w >> 16u)); }

// Mode state: enable/disable, zero capture on enable, the |a| ~ 1 g guard.
class TiltController {
public:
    bool enabled() const { return enabled_; }
    int offsetX() const { return x_.offset(); }
    int offsetY() const { return y_.offset(); }

    void enable(uint32_t nowMs) {
        enabled_ = true;
        x_.reset();
        y_.reset();
        lastMs_ = nowMs;
        badSince_ = 0;
        bad_ = false;
    }
    void disable() {
        enabled_ = false;
        x_.reset();
        y_.reset();
        bad_ = false;
    }

    // True when a sensor read is due (mode on and >= kReadIntervalMs since the last sample).
    bool due(uint32_t nowMs) const {
        return enabled_ && static_cast<uint32_t>(nowMs - lastMs_) >= kReadIntervalMs;
    }

    // One accelerometer sample in g. Returns false if the mode switched itself
    // off because the device was not held steady (|a| far from 1 g for too long).
    bool feed(float ax, float ay, float az, uint32_t nowMs) {
        if (!enabled_) return false;
        const uint32_t dt = static_cast<uint32_t>(nowMs - lastMs_);
        lastMs_ = nowMs;
        const float mag = std::sqrt(ax * ax + ay * ay + az * az);
        const float dev = mag > 1.0f ? mag - 1.0f : 1.0f - mag;
        if (dev > kGravityTolerance) {
            if (!bad_) { bad_ = true; badSince_ = nowMs; }
            if (static_cast<uint32_t>(nowMs - badSince_) >= kGravityHoldMs) {
                disable();
                return false;
            }
            return true;   // hold the last offsets while the sample is unreliable
        }
        bad_ = false;
        x_.update(angleXDeg(ax, ay, az), dt);
        y_.update(angleYDeg(ax, ay, az), dt);
        return true;
    }

    uint32_t word() const { return packOffsets(enabled_, x_.offset(), y_.offset()); }

private:
    TiltAxis x_;
    TiltAxis y_;
    uint32_t lastMs_ = 0;
    uint32_t badSince_ = 0;
    bool enabled_ = false;
    bool bad_ = false;
};

// Process-wide state (about 40 bytes of DRAM). The controller and toggle flag
// belong to loop(); the word is the only thing the dispatcher task reads.
inline TiltController gController;
inline MidiRealtimeWord gWord;
inline volatile bool gToggleRequested = false;

}  // namespace GroovePuterTilt
