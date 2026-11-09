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
    Rig(bool sound) {
        now = 1000;
        loop.setNowFunction(timeNow);
        assert(loop.setRecordSound(sound));
        keys.setInputObserver(&loop);
        loop.service(now, nullptr, 120.0f);
        assert(loop.recDub() == Looper::Result::Done);
    }
    void advance(uint32_t us) { now += us; keys.service(now); loop.service(now, nullptr, 120.0f); }
};

int main() {
    {
        Rig r(false);
        r.keys.setVoiceMode(PerformanceVoiceMode::Poly);
        r.keys.setChordMode(PerformanceChordMode::Fifth);
        assert(r.keys.keyDown('a'));
        assert(r.loop.noteCount() == 1); // physical root, not the chord
        assert(!r.loop.setRecordSound(true));
        r.advance(250000);
        assert(r.keys.keyUp('a'));
        assert(r.loop.layer().note(0).length() == 2);
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
        assert(r.loop.layer().note(0).length() == 2);
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
    std::puts("looper record sound tests passed");
}
