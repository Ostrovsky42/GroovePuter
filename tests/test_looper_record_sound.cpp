#include <cassert>
#include <cstdint>
#include <cstdio>
#include "src/input/looper.h"
#include "src/input/performance_keyboard.h"

using namespace GroovePuterLooper;
static uint32_t now = 1000;
static uint32_t timeNow() { return now; }

struct Rig {
    MusicalEventRouter router;
    PerformanceKeyboard keys{router};
    Looper loop;
    Rig(bool sound, uint8_t bars = 2) {
        now = 1000;
        loop.setNowFunction(timeNow);
        assert(loop.setRecordSound(sound));
        assert(bars == 2 || loop.setBars(bars) == Looper::Result::Done);
        keys.setInputObserver(&loop);
        loop.service(now, nullptr, 120.0f);
        assert(loop.recDub() == Looper::Result::Done);
    }
    void advance(uint32_t us) { now += us; keys.service(now); loop.service(now, nullptr, 120.0f); }
};

int main() {
    {
        Rig r(true, 8);
        for (unsigned i = 0; i < 128; ++i) {
            assert(r.keys.keyDown('a'));
            assert(r.keys.keyUp('a'));
            if (i != 127) r.advance(125000);
        }
        assert(r.loop.noteCount() == 128);
        assert(!r.loop.full() && r.loop.layer().dropped() == 0);
    }
    {
        Rig r(true, 8);
        r.keys.setVoiceMode(PerformanceVoiceMode::Poly);
        r.keys.setChordMode(PerformanceChordMode::Fifth);
        for (unsigned i = 0; i < 64; ++i) {
            assert(r.keys.keyDown('a'));
            assert(r.keys.keyUp('a'));
            r.advance(125000);
        }
        assert(r.loop.noteCount() == kLoopCapacity);
        assert(r.keys.keyDown('a'));
        assert(r.keys.keyUp('a'));
        assert(r.loop.noteCount() == kLoopCapacity);
        assert(r.loop.layer().dropped() == 3);
        assert(r.loop.takeFullNotice());
        assert(!r.loop.takeFullNotice());
    }
    {
        Rig r(false);
        r.keys.setVoiceMode(PerformanceVoiceMode::Poly);
        r.keys.setChordMode(PerformanceChordMode::Fifth);
        assert(r.keys.keyDown('a'));
        assert(r.loop.noteCount() == 1); // physical root, not the chord
        assert(!r.loop.setRecordSound(true));
        r.advance(250000);
        assert(r.keys.keyUp('a'));
        assert(r.loop.layer().note(0).lengthTicks() == 2 * kTicksPerStep);
    }
    {
        Rig r(true);
        r.keys.setVoiceMode(PerformanceVoiceMode::Poly);
        r.keys.setChordMode(PerformanceChordMode::Fifth);
        assert(r.keys.keyDown('a'));
        assert(r.loop.noteCount() == 3); // actual chord including generated voices
        assert(r.loop.layer().note(0).note() == 36);
        assert(r.loop.layer().note(1).note() == 43);
        assert(r.loop.layer().note(2).note() == 48);
        assert(r.keys.keyDown('q')); // overlapping 48 remains a single voice
        assert(r.loop.noteCount() == 5);
        r.advance(250000);
        assert(r.keys.keyUp('a'));
        assert(r.loop.layer().note(2).held());
        r.advance(250000);
        assert(r.keys.keyUp('q'));
        assert(!r.loop.layer().note(2).held());
        // Foreign traffic never enters the keyboard's output tap.
        r.router.route(MusicalEvent{MusicalEventType::NoteOn, MusicalEventSource::Arpeggiator,
                                   MusicalEventTarget::Dx, 0, 75, 90, kMusicalEventLoopLane});
        assert(r.loop.noteCount() == 5);
    }
    {
        Rig r(true);
        assert(r.keys.keyDown('a'));
        r.advance(250000);
        assert(r.keys.keyDown('s')); // mono steals the previous sounding note
        assert(r.loop.layer().note(0).lengthTicks() == 2 * kTicksPerStep);
        r.advance(125000);
        assert(r.keys.keyUp('a')); // inactive release does not close the new note
        assert(r.loop.layer().note(1).held());
        r.keys.panic();
        assert(!r.loop.layer().note(1).held());
    }
    {
        Rig r(true);
        r.keys.setArpeggiatorEnabled(true);
        assert(r.keys.keyDown('a'));
        for (int i = 0; i < 100; ++i) r.advance(10000);
        assert(r.loop.noteCount() > 1); // repeated arp attacks, not one held key
        assert(r.keys.keyUp('a'));
        for (uint8_t i = 0; i < r.loop.layer().slotCount(); ++i)
            assert(!r.loop.layer().note(i).held());
    }
    {
        Rig r(true);
        r.keys.setChordMode(PerformanceChordMode::Fifth);
        r.keys.cycleStrum();
        assert(r.keys.strumMs() > 0);
        assert(r.keys.keyDown('a'));
        assert(r.loop.noteCount() == 1);
        for (int i = 0; i < 20; ++i) r.advance(10000);
        assert(r.loop.noteCount() == 3); // delayed generated notes go through the tap too
        assert(r.keys.keyUp('a'));
    }
    // REC: SOUND with the real arpeggiator at 1/16T, 1/32 and 1/8T, gate 50 %,
    // main loop every 1 ms: every arp note is in the loop, on its own tick,
    // the intervals equal (4, 3 and 8 ticks), the gate kept.
    {
        struct Case { PerformanceRate rate; int ticks; };
        const Case cases[] = {{PerformanceRate::SixteenthTriplet, 4}, {PerformanceRate::ThirtySecond, 3},
                              {PerformanceRate::EighthTriplet, 8}, {PerformanceRate::Sixteenth, 6}};
        for (const Case& c : cases) {
            Rig r(true, 1);
            r.keys.setTempoBpm(120.0f);
            r.keys.setArpRate(c.rate);
            r.keys.setGatePercent(50);
            r.keys.setArpeggiatorEnabled(true);
            assert(r.keys.keyDown('a'));
            for (int i = 0; i < 1900; ++i) r.advance(1000);  // 15.2 steps of the 1-bar pass
            assert(r.keys.keyUp('a'));
            for (int i = 0; i < 60; ++i) r.advance(1000);
            const uint8_t count = r.loop.layer().slotCount();
            const int expected = (15 * kTicksPerStep + 1) / c.ticks + 1;  // attacks in 15.2 steps
            std::printf("SOUND %s: %u notes (expected %d), first tick %u\n", performanceRateName(c.rate),
                        count, expected, r.loop.layer().note(0).onsetTicks());
            assert(count == expected && r.loop.layer().dropped() == 0);
            for (uint8_t i = 0; i < count; ++i) {
                const LoopNote& n = r.loop.layer().note(i);
                assert(!n.held());
                assert(n.onsetTicks() == r.loop.layer().note(0).onsetTicks() + i * c.ticks);
                // Half the pulse; 1.5 ticks of a 1/32 is stored as 1 or 2. The
                // last note may be cut by the key release.
                if (i + 1 == count) assert(n.lengthTicks() >= 1 && n.lengthTicks() <= c.ticks / 2 + 1);
                else if (c.ticks % 2 == 0) assert(n.lengthTicks() == c.ticks / 2);
                else assert(n.lengthTicks() == 1 || n.lengthTicks() == 2);
            }
        }
    }
    std::puts("looper record sound tests passed");
}
