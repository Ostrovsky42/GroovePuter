// Host test for src/input/tilt_axis.h (not registered in run_host_tests.sh).
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "src/input/tilt_axis.h"

using namespace GroovePuterTilt;

static int g_fail = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++g_fail;                                                        \
        }                                                                    \
    } while (0)

static void settle(TiltAxis& a, float angle, int ms = 1500) {
    for (int t = 0; t < ms; t += 20) a.update(angle, 20);
}

static void testAngle() {
    CHECK(std::fabs(angleXDeg(0, 0, 1)) < 1e-4f);
    CHECK(std::fabs(angleXDeg(1, 0, 0) - 90.0f) < 1e-3f);
    CHECK(std::fabs(angleXDeg(0.5f, 0, 0.866f) - 30.0f) < 0.1f);
    CHECK(angleXDeg(-0.5f, 0, 0.866f) < -29.0f);          // sign
    CHECK(std::fabs(angleYDeg(0, 0.5f, 0.866f) - 30.0f) < 0.1f);
    CHECK(angleYDeg(0, -0.5f, 0.866f) < -29.0f);
    CHECK(std::fabs(angleYDeg(0.5f, 0, 0.866f)) < 1e-3f);  // axes independent
    CHECK(angleXDeg(0, 0, -1) == 0.0f);                    // face down: no wrap
}

static void testOffsetMap() {
    CHECK(offsetForDelta(0.0f) == 0);
    CHECK(offsetForDelta(5.0f) == 0);
    CHECK(offsetForDelta(-5.0f) == 0);
    CHECK(offsetForDelta(5.5f) > 0);
    CHECK(offsetForDelta(-5.5f) < 0);
    CHECK(offsetForDelta(5.5f) == -offsetForDelta(-5.5f));
    CHECK(offsetForDelta(30.0f) == 63);
    CHECK(offsetForDelta(-30.0f) == -63);
    CHECK(offsetForDelta(80.0f) == 63);                   // clamp
    CHECK(offsetForDelta(-80.0f) == -63);
    CHECK(offsetForDelta(17.5f) >= 31 && offsetForDelta(17.5f) <= 32);   // midpoint
    int prev = 0;                                         // monotonic
    for (float d = 0; d <= 40.0f; d += 0.25f) {
        const int o = offsetForDelta(d);
        CHECK(o >= prev);
        prev = o;
    }
}

static void testCaptureZero() {
    TiltAxis a;
    CHECK(a.update(20.0f, 20) == 0);                      // first sample is zero
    CHECK(a.captured());
    settle(a, 20.0f);
    CHECK(a.offset() == 0);                               // held pose stays zero
    settle(a, 20.0f + 30.0f);
    CHECK(a.offset() == 63);
    settle(a, 20.0f - 30.0f);
    CHECK(a.offset() == -63);
    a.reset();
    CHECK(!a.captured());
    CHECK(a.update(-45.0f, 20) == 0);                     // new zero
    settle(a, -45.0f);
    CHECK(a.offset() == 0);
}

static void testLowPass() {
    TiltAxis a;
    a.update(0.0f, 20);
    // Step to 30 deg: after one 20 ms sample the output must still be far from full scale.
    const int first = a.update(30.0f, 20);
    CHECK(first < 40);   // 4.3 deg after one sample: still in the dead zone
    // After ~120 ms (one tau) filtered angle is ~63% of 30 = ~19 deg.
    TiltAxis b;
    b.update(0.0f, 20);
    int o = 0;
    for (int i = 0; i < 6; ++i) o = b.update(30.0f, 20);
    const int expected = offsetForDelta(30.0f * (1.0f - std::exp(-120.0f / 140.0f)));
    CHECK(std::abs(o - expected) <= 6);
    // Eventually reaches full scale.
    settle(b, 30.0f);
    CHECK(b.offset() == 63);
}

static void testDeadzoneNoise() {
    TiltAxis a;
    a.update(10.0f, 20);
    for (int i = 0; i < 200; ++i) a.update(10.0f + ((i & 1) ? 3.0f : -3.0f), 20);
    CHECK(a.offset() == 0);                               // jitter inside +-5 deg never moves it
}

static void testHysteresis() {
    TiltAxis a;
    a.update(0.0f, 20);
    // Settle at a delta giving a mid-range step, then wobble by less than a step.
    settle(a, 15.0f);
    const int base = a.offset();
    CHECK(base > 10);
    int changes = 0;
    int last = base;
    for (int i = 0; i < 400; ++i) {
        // +-0.1 deg is well under one step (one step = 25/63 = 0.4 deg)
        const int o = a.update(15.0f + ((i & 1) ? 0.1f : -0.1f), 20);
        if (o != last) { ++changes; last = o; }
    }
    CHECK(changes == 0);
    // A move of several steps does pass through.
    settle(a, 17.0f);
    CHECK(a.offset() - base >= 4);
    // Return to center reaches exactly zero (no stuck +-1).
    settle(a, 0.0f);
    CHECK(a.offset() == 0);
}

static void testPack() {
    const uint32_t w = packOffsets(true, -63, 63);
    CHECK(wordActive(w) && wordX(w) == -63 && wordY(w) == 63);
    CHECK(packOffsets(false, 12, 12) == 0u);
    const uint32_t z = packOffsets(true, 0, -1);
    CHECK(wordActive(z) && wordX(z) == 0 && wordY(z) == -1);
}

static void testController() {
    TiltController c;
    CHECK(!c.enabled() && c.word() == 0u);
    CHECK(!c.due(1000));
    c.enable(1000);
    CHECK(c.enabled() && !c.due(1010) && c.due(1020));
    // Held flat, steady at 1 g: zero offsets, word active.
    uint32_t t = 1000;
    for (int i = 0; i < 20; ++i) { t += 20; CHECK(c.feed(0.0f, 0.0f, 1.0f, t)); }
    CHECK(wordActive(c.word()) && c.offsetX() == 0 && c.offsetY() == 0);
    // Tilt +30 deg on X (about 0.5 g): positive X offset, Y untouched.
    for (int i = 0; i < 100; ++i) { t += 20; CHECK(c.feed(0.5f, 0.0f, 0.866f, t)); }
    CHECK(c.offsetX() == 63 && c.offsetY() == 0);
    // Y negative.
    for (int i = 0; i < 100; ++i) { t += 20; CHECK(c.feed(0.0f, -0.5f, 0.866f, t)); }
    CHECK(c.offsetX() == 0 && c.offsetY() == -63);
    // Zero was captured at enable: re-enable while tilted makes the tilted pose the new zero.
    c.disable();
    CHECK(c.word() == 0u);
    c.enable(t);
    for (int i = 0; i < 20; ++i) { t += 20; c.feed(0.5f, 0.0f, 0.866f, t); }
    CHECK(c.offsetX() == 0);
}

static void testGravityGuard() {
    TiltController c;
    c.enable(0);
    uint32_t t = 0;
    for (int i = 0; i < 10; ++i) { t += 20; c.feed(0, 0, 1.0f, t); }
    // A short shake (200 ms) does not switch it off.
    for (int i = 0; i < 10; ++i) { t += 20; CHECK(c.feed(0, 0, 2.0f, t)); }
    CHECK(c.enabled());
    t += 20; c.feed(0, 0, 1.0f, t);                       // steady again, timer resets
    // A long shake does.
    bool off = false;
    for (int i = 0; i < 40; ++i) { t += 20; if (!c.feed(0, 0, 2.0f, t)) { off = true; break; } }
    CHECK(off && !c.enabled() && c.word() == 0u);
    CHECK(!c.feed(0, 0, 1.0f, t + 20));                   // ignored while off
}

int main() {
    testAngle();
    testOffsetMap();
    testCaptureZero();
    testLowPass();
    testDeadzoneNoise();
    testHysteresis();
    testPack();
    testController();
    testGravityGuard();
    if (g_fail) { std::printf("%d failure(s)\n", g_fail); return 1; }
    std::printf("tilt_axis: all tests passed\n");
    return 0;
}
