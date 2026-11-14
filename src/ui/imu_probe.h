#pragma once
// Experimental BMI270 accelerometer probe. Compiled only with
// -DGROOVEPUTER_IMU_PROBE; without the flag this header is empty.
#ifdef GROOVEPUTER_IMU_PROBE
#include <cstdint>
#include <cstdio>

namespace ImuProbe {
inline int16_t ax10 = 0, ay10 = 0, az10 = 0;  // tenths of g
inline uint16_t fps = 0;                      // sensor updates per second
inline uint16_t winCount = 0;
inline uint32_t winStartMs = 0;
inline uint16_t lastCostUs = 0;               // last M5.Imu call pair cost
inline uint16_t peakCostUs = 0;               // peak since last [IMU] log
inline bool enabled = false;

inline void formatLine(char* out, size_t n) {
    if (!enabled) { snprintf(out, n, "IMU OFF"); return; }
    auto f = [](int16_t t, char* s) {
        snprintf(s, 8, "%c%d.%d", t < 0 ? '-' : '+', (t < 0 ? -t : t) / 10,
                 (t < 0 ? -t : t) % 10);
    };
    char x[8], y[8], z[8];
    f(ax10, x); f(ay10, y); f(az10, z);
    snprintf(out, n, "AX%s AY%s AZ%s %uHz", x, y, z, (unsigned)fps);
}
}  // namespace ImuProbe
#endif
