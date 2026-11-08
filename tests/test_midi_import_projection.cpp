// SMF notes of one window/source -> Melody (GRAB, and the workspace import's Task 2).
#include <cassert>
#include <cstdio>

#include "src/audio/midi_import_projection.h"

using GroovePuterMidi::SmfEvent;
using GroovePuterMidi::SmfEventKind;
using namespace MidiImport;

namespace {
constexpr uint16_t kPpqn = 480;
constexpr uint32_t kBar = kPpqn * 4u;

SmfEvent on(uint32_t tick, uint8_t note, uint8_t velocity = 100, uint8_t channel = 0) {
    SmfEvent e{};
    e.tick = tick;
    e.kind = SmfEventKind::NoteOn;
    e.channel = channel;
    e.data1 = note;
    e.data2 = velocity;
    return e;
}

SmfEvent off(uint32_t tick, uint8_t note, uint8_t channel = 0) {
    SmfEvent e = on(tick, note, 0, channel);
    e.kind = SmfEventKind::NoteOff;
    return e;
}

GrabWindow bars(uint32_t firstBar, uint32_t count, uint16_t mask = 0xFFFFu) {
    GrabWindow w{};
    w.division = kPpqn;
    w.startTick = (firstBar - 1u) * kBar;
    w.endTick = w.startTick + count * kBar;
    w.channelMask = mask;
    return w;
}
}  // namespace

int main() {
    // A quarter note at PPQN 480 is 96 runtime ticks / 1536 subticks; velocity kept.
    {
        Buffer melody{};
        MelodyGrabBuilder grab;
        assert(grab.begin(bars(5, 4), melody) == GrabStatus::Ok);
        const uint32_t a = 4u * kBar;  // bar 5
        grab.feed(on(a, 60, 17));
        grab.feed(off(a + kPpqn, 60));
        grab.feed(on(a + kPpqn, 64, 90));
        grab.feed(off(a + kPpqn + kPpqn / 2u, 64));
        assert(grab.finish() == GrabStatus::Ok);
        assert(melody.lengthTicks == 4u * PhraseRuntime::kTicksPerBar);
        assert(melody.count == 2u);
        assert(melody.events[0].startTick == 0u && melody.events[0].durationSubticks == 1536u);
        assert(melody.events[0].velocity == 17u && melody.events[0].note == 60u);
        assert(melody.events[1].startTick == 96u && melody.events[1].durationSubticks == 768u);
        assert(grab.report().notes == 2u && grab.report().bars == 4u);
    }

    // Repeated Note On of one pitch pairs Note Offs FIFO; the result is sorted by start.
    {
        Buffer melody{};
        MelodyGrabBuilder grab;
        assert(grab.begin(bars(1, 1), melody) == GrabStatus::Ok);
        grab.feed(on(0, 60));
        grab.feed(on(kPpqn, 60));
        grab.feed(off(kPpqn * 2u, 60));   // closes the first (FIFO)
        grab.feed(off(kPpqn * 3u, 60));   // closes the second
        assert(grab.finish() == GrabStatus::Ok);
        assert(melody.count == 2u);
        assert(melody.events[0].startTick == 0u && melody.events[0].durationSubticks == 2u * 1536u);
        assert(melody.events[1].startTick == 96u && melody.events[1].durationSubticks == 2u * 1536u);
    }

    // Window edges: a note started before A is not taken; one held past B is cut at B.
    {
        Buffer melody{};
        MelodyGrabBuilder grab;
        assert(grab.begin(bars(2, 1), melody) == GrabStatus::Ok);
        grab.feed(on(kBar - kPpqn, 50));      // before the window
        grab.feed(off(kBar + kPpqn, 50));     // its Note Off inside: ignored, counted
        grab.feed(on(2u * kBar - kPpqn, 70)); // last beat of the window
        grab.feed(off(2u * kBar + kPpqn, 70));
        assert(grab.finish() == GrabStatus::Ok);
        assert(melody.count == 1u && melody.events[0].note == 70u);
        assert(melody.events[0].startTick == 288u && melody.events[0].durationSubticks == 1536u);
        assert(grab.report().startedBefore == 1u && grab.report().cutAtEnd == 1u);
    }

    // A note with no Note Off before the window ends is closed at the end.
    {
        Buffer melody{};
        MelodyGrabBuilder grab;
        assert(grab.begin(bars(1, 1), melody) == GrabStatus::Ok);
        grab.feed(on(kPpqn * 2u, 62));
        assert(grab.finish() == GrabStatus::Ok);
        assert(melody.events[0].durationSubticks == 2u * 1536u);
        assert(grab.report().cutAtEnd == 1u);
    }

    // Channel mask: a HUB layer is a track plus channels; other channels are ignored.
    {
        Buffer melody{};
        MelodyGrabBuilder grab;
        assert(grab.begin(bars(1, 1, 1u << 2), melody) == GrabStatus::Ok);
        grab.feed(on(0, 60, 100, 0));
        grab.feed(off(kPpqn, 60, 0));
        grab.feed(on(0, 48, 100, 2));
        grab.feed(off(kPpqn, 48, 2));
        assert(grab.finish() == GrabStatus::Ok);
        assert(melody.count == 1u && melody.events[0].note == 48u);
    }

    // Chords stay chords (the Melody plays them through chord voices).
    {
        Buffer melody{};
        MelodyGrabBuilder grab;
        assert(grab.begin(bars(1, 1), melody) == GrabStatus::Ok);
        for (uint8_t n : {60, 63, 67}) grab.feed(on(0, n));
        for (uint8_t n : {60, 63, 67}) grab.feed(off(kBar, n));
        assert(grab.finish() == GrabStatus::Ok);
        assert(melody.count == 3u);
        assert(RuntimePhraseEdit::hasOverlappingNotes(melody));
    }

    // Length: 3 bars -> a 4-bar Melody, 5 -> 8, 9 -> refused; empty -> refused.
    {
        Buffer melody{};
        MelodyGrabBuilder grab;
        assert(grab.begin(bars(1, 3), melody) == GrabStatus::Ok);
        assert(melody.lengthTicks == 4u * PhraseRuntime::kTicksPerBar);
        assert(grab.begin(bars(1, 5), melody) == GrabStatus::Ok);
        assert(melody.lengthTicks == 8u * PhraseRuntime::kTicksPerBar);
        assert(grab.begin(bars(1, 9), melody) == GrabStatus::TooLong);
        assert(grab.begin(bars(1, 1), melody) == GrabStatus::Ok);
        assert(grab.finish() == GrabStatus::Empty);
        GrabWindow bad = bars(1, 1);
        bad.division = 0;
        assert(grab.begin(bad, melody) == GrabStatus::Invalid);
    }

    // More notes than a Melody holds: refused, never silently truncated.
    {
        Buffer melody{};
        MelodyGrabBuilder grab;
        assert(grab.begin(bars(1, 8), melody) == GrabStatus::Ok);
        const uint32_t step = kPpqn / 8u;  // 32nds over 8 bars = 256 notes
        for (uint32_t i = 0; i < 256u; ++i) {
            grab.feed(on(i * step, 60));
            grab.feed(off(i * step + step / 2u, 60));
        }
        assert(grab.finish() == GrabStatus::TooManyNotes);
    }

    std::puts("midi import projection (GRAB): PASS");
    return 0;
}
