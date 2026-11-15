#ifndef GROOVEPUTER_TILT_AXIS_H
#define GROOVEPUTER_TILT_AXIS_H
// Tilt to MIDI CC offset: pure, host-testable pieces. No sensor, no SDL, no
// allocation. Used only by the GROOVEPUTER_TILT experiment; nothing includes
// this header without that flag.
//
// Chain per axis: accelerometer vector -> angle (deg) -> one-pole low pass
// (~120 ms) -> delta from the zero captured at enable -> dead zone +-5 deg ->
// full scale at +-30 deg -> integer offset -63..+63 -> hysteresis of one step.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

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

// TILT SENS: degrees of tilt that give full scale, one per step (index 0..3).
inline constexpr float kSensRangeDeg[5] = {60.0f, 45.0f, 30.0f, 20.0f, 12.0f};
inline constexpr uint8_t kSensDefault = 2;   // 30 deg, the first experiment's value

// Delta angle (degrees from the captured zero) to a CC offset -63..+63.
// Zero inside the dead zone, continuous from its edge, saturating at rangeDeg.
inline int offsetForDelta(float deltaDeg, float rangeDeg = kRangeDeg) {
    const float mag = deltaDeg < 0.0f ? -deltaDeg : deltaDeg;
    if (mag <= kDeadzoneDeg) return 0;
    float r = (mag - kDeadzoneDeg) / (rangeDeg - kDeadzoneDeg);
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
    int update(float angleDeg, uint32_t dtMs, float rangeDeg = kRangeDeg) {
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
        const int cand = offsetForDelta(filt_ - zero_, rangeDeg);
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
    // Filtered angle from the captured zero, degrees (0 before the zero is captured).
    float deltaDeg() const { return captured_ ? filt_ - zero_ : 0.0f; }

private:
    float zero_ = 0.0f;
    float filt_ = 0.0f;
    int8_t last_ = 0;
    bool captured_ = false;
};

// Which synth the offsets reach. Bit 1 = SYNTH A (voice 0), bit 2 = SYNTH B (voice 1).
enum class TiltTarget : uint8_t { A = 1, B = 2, All = 3 };
inline TiltTarget nextTarget(TiltTarget t) {
    if (t == TiltTarget::A) return TiltTarget::B;
    if (t == TiltTarget::B) return TiltTarget::All;
    return TiltTarget::A;
}
inline const char* targetName(TiltTarget t) {
    if (t == TiltTarget::A) return "A";
    if (t == TiltTarget::B) return "B";
    return "ALL";
}

// Shared word: bit 0 active, bits 1..2 target mask, bits 8..15 X offset (int8),
// bits 16..23 Y offset, bits 24..31 Z offset (gyro). Written by loop(), read by the
// MIDI dispatcher and the header.
inline uint32_t packOffsets(bool active, int x, int y, TiltTarget target = TiltTarget::All,
                            int z = 0) {
    if (!active) return 0u;
    return 1u | (static_cast<uint32_t>(target) << 1u) |
           (static_cast<uint32_t>(static_cast<uint8_t>(static_cast<int8_t>(x))) << 8u) |
           (static_cast<uint32_t>(static_cast<uint8_t>(static_cast<int8_t>(y))) << 16u) |
           (static_cast<uint32_t>(static_cast<uint8_t>(static_cast<int8_t>(z))) << 24u);
}
inline bool wordActive(uint32_t w) { return (w & 1u) != 0u; }
inline int wordX(uint32_t w) { return static_cast<int8_t>(static_cast<uint8_t>(w >> 8u)); }
inline int wordY(uint32_t w) { return static_cast<int8_t>(static_cast<uint8_t>(w >> 16u)); }
inline int wordZ(uint32_t w) { return static_cast<int8_t>(static_cast<uint8_t>(w >> 24u)); }
// True when the offsets in w reach the given voice (0 = SYNTH A, 1 = SYNTH B).
inline bool wordTargetsVoice(uint32_t w, uint8_t voice) {
    return wordActive(w) && (((w >> 1u) & 3u) >> voice & 1u) != 0u;
}

// Quick flick: a swing from rest to a fixed angle on either axis within a short window
// fires once. Thresholds are in degrees, so TILT SENS does not change how easy a flick
// is. Rest must be re-entered before the next flick. A slow hold never fires.
inline constexpr float kFlickDeg = 25.0f;      // a deliberate flick (about the old 50 at 30 deg)
inline constexpr float kRestDeg = 11.0f;       // back inside this counts as rest
inline constexpr uint32_t kFlickWindowMs = 300;
class FlickDetector {
public:
    void reset() { swinging_ = false; spent_ = false; }
    // Feeds the current axis angles (degrees from zero); true on the step that completes a flick.
    bool update(float xDeg, float yDeg, uint32_t nowMs) {
        const float ax = xDeg < 0.0f ? -xDeg : xDeg;
        const float ay = yDeg < 0.0f ? -yDeg : yDeg;
        const float m = ax > ay ? ax : ay;
        if (m <= kRestDeg) { swinging_ = false; return false; }
        if (!swinging_) { swinging_ = true; spent_ = false; startMs_ = nowMs; }
        if (spent_) return false;
        const bool inWindow = static_cast<uint32_t>(nowMs - startMs_) <= kFlickWindowMs;
        if (m >= kFlickDeg) { spent_ = true; return inWindow; }
        if (!inWindow) spent_ = true;   // slow hold: no flick until rest again
        return false;
    }
private:
    uint32_t startMs_ = 0;
    bool swinging_ = false;
    bool spent_ = false;
};

// Process-wide tilt settings, set on PERFORM > INPUT and read by loop() and the dispatcher.
// TILT SENS index into kSensRangeDeg.
inline volatile uint8_t gSensIndex = kSensDefault;
// Controller assigned to each axis (X, Y, Z): see kTiltChoiceMax below.
// Defaults: X = CUTOFF, Y = none, Z = none.
// Choices for an axis: 0 none, 1 CUTOFF, 2 REVERB, 3 DELAY, 5 ATTACK, 6 RELEASE,
// 7 VOLUME, 8 PAN. Number 4 (RESO) is no longer offered and reads as none. EXPRESSION
// and EQ are left out on purpose. The numbers are stored on the SD file, so they stay.
inline constexpr uint8_t kTiltChoiceMax = 8;
inline volatile uint8_t gAxisChoice[3] = {1, 0, 0};
// TILT FLICK: on = a quick flick cycles the target; off = the target changes only in the menu.
inline volatile bool gFlickEnabled = true;
// TILT ZERO request from PERFORM > INPUT; loop() applies it to the controller.
inline volatile bool gZeroRequested = false;
// Shared settings (not per project) are written by loop() when a setting changes.
inline volatile bool gSaveRequested = false;

// Mode state: enable/disable, zero capture on enable, the |a| ~ 1 g guard.
class TiltController {
public:
    bool enabled() const { return enabled_; }
    int offsetX() const { return x_.offset(); }
    int offsetY() const { return y_.offset(); }
    TiltTarget target() const { return target_; }
    void setTarget(TiltTarget t) { target_ = t; }
    // TILT ZERO: the current pose becomes zero on X, Y and Z (the next sample is captured).
    void recalibrate() {
        x_.reset();
        y_.reset();
        zAngle_ = 0.0f;
        z_ = 0;
        flick_.reset();
        bad_ = false;
    }
    // True once after each flick that moved the target (cleared on read).
    bool takeTargetChanged() {
        const bool changed = targetChanged_;
        targetChanged_ = false;
        return changed;
    }

    void enable(uint32_t nowMs) {
        enabled_ = true;
        x_.reset();
        y_.reset();
        zAngle_ = 0.0f;
        z_ = 0;
        flick_.reset();
        lastMs_ = nowMs;
        badSince_ = 0;
        bad_ = false;
    }
    void disable() {
        enabled_ = false;
        x_.reset();
        y_.reset();
        zAngle_ = 0.0f;
        z_ = 0;
        bad_ = false;
    }

    // True when a sensor read is due (mode on and >= kReadIntervalMs since the last sample).
    bool due(uint32_t nowMs) const {
        return enabled_ && static_cast<uint32_t>(nowMs - lastMs_) >= kReadIntervalMs;
    }

    // One accelerometer sample in g. Returns false if the mode switched itself
    // off because the device was not held steady (|a| far from 1 g for too long).
    // gzDps: yaw rate about the vertical axis (gyro Z, degrees per second). It turns
    // into an angle that leaks back to zero (kGyroLeakMs), so a long hold drifts home.
    bool feed(float ax, float ay, float az, uint32_t nowMs, float gzDps = 0.0f) {
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
        const float range = kSensRangeDeg[gSensIndex < 5 ? gSensIndex : kSensDefault];
        x_.update(angleXDeg(ax, ay, az), dt, range);
        y_.update(angleYDeg(ax, ay, az), dt, range);
        // Yaw: integrate the gyro, leak toward zero, then the same dead zone and scale.
        const float dtS = static_cast<float>(dt) * 0.001f;
        zAngle_ = zAngle_ * (1.0f - static_cast<float>(dt) / kGyroLeakMs) + gzDps * dtS;
        z_ = static_cast<int8_t>(offsetForDelta(zAngle_, range));
        if (gFlickEnabled && flick_.update(x_.deltaDeg(), y_.deltaDeg(), nowMs)) {
            target_ = nextTarget(target_);
            targetChanged_ = true;
        }
        return true;
    }

    uint32_t word() const { return packOffsets(enabled_, x_.offset(), y_.offset(), target_, z_); }
    int offsetZ() const { return z_; }
    // Angle of an axis from its zero, degrees: 0 = X, 1 = Y, 2 = Z (gyro turn).
    float angleDeg(uint8_t axis) const {
        if (!enabled_) return 0.0f;
        if (axis == 0) return x_.deltaDeg();
        if (axis == 1) return y_.deltaDeg();
        return zAngle_;
    }

private:
    static constexpr float kGyroLeakMs = 9000.0f;   // a held yaw drifts back to zero over ~9 s
    TiltAxis x_;
    TiltAxis y_;
    float zAngle_ = 0.0f;
    int8_t z_ = 0;
    FlickDetector flick_;
    TiltTarget target_ = TiltTarget::All;   // kept across off/on
    bool targetChanged_ = false;
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

// Shared TILT settings, kept across projects: TILT SENS, the X/Y/Z controllers and
// TILT FLICK. The target and the zero are not stored (the target starts at ALL, the
// zero is a pose). Plain text, one key per line; unknown or bad values keep defaults.
struct StoredSettings {
    uint8_t sens = kSensDefault;
    uint8_t choice[3] = {1, 0, 0};
    bool flick = true;
};

// Writes the file body into out; returns the number of characters (not counting the NUL).
inline int formatSettings(const StoredSettings& v, char* out, size_t size) {
    return std::snprintf(out, size, "sens=%u\nx=%u\ny=%u\nz=%u\nflick=%u\n",
                         static_cast<unsigned>(v.sens), static_cast<unsigned>(v.choice[0]),
                         static_cast<unsigned>(v.choice[1]), static_cast<unsigned>(v.choice[2]),
                         v.flick ? 1u : 0u);
}

// Reads the text back. Starts from the defaults, so a missing or partial file is harmless.
inline StoredSettings parseSettings(const char* text) {
    StoredSettings v{};
    const char* line = text;
    while (line != nullptr && *line != '\0') {
        unsigned value = 0;
        if (std::sscanf(line, "sens=%u", &value) == 1 && value < 5u) v.sens = static_cast<uint8_t>(value);
        if (std::sscanf(line, "x=%u", &value) == 1 && value <= kTiltChoiceMax && value != 4u) v.choice[0] = static_cast<uint8_t>(value);
        if (std::sscanf(line, "y=%u", &value) == 1 && value <= kTiltChoiceMax && value != 4u) v.choice[1] = static_cast<uint8_t>(value);
        if (std::sscanf(line, "z=%u", &value) == 1 && value <= kTiltChoiceMax && value != 4u) v.choice[2] = static_cast<uint8_t>(value);
        if (std::sscanf(line, "flick=%u", &value) == 1 && value <= 1u) v.flick = value != 0u;
        const char* next = std::strchr(line, '\n');
        line = next == nullptr ? nullptr : next + 1;
    }
    return v;
}

inline StoredSettings currentSettings() {
    StoredSettings v{};
    v.sens = static_cast<uint8_t>(gSensIndex);
    for (uint8_t i = 0; i < 3; ++i) v.choice[i] = static_cast<uint8_t>(gAxisChoice[i]);
    v.flick = gFlickEnabled;
    return v;
}

inline void applySettings(const StoredSettings& v) {
    gSensIndex = v.sens;
    for (uint8_t i = 0; i < 3; ++i) gAxisChoice[i] = v.choice[i];
    gFlickEnabled = v.flick;
}

}  // namespace GroovePuterTilt

#endif  // GROOVEPUTER_TILT_AXIS_H
