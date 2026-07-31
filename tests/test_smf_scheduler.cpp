#include <cassert>

#include "src/midi/smf_scheduler.h"

using namespace GroovePuterMidi;

int main() {
    SmfDocument doc;
    doc.division = 480;
    doc.events.push_back(SmfEvent{0, 0, SmfEventKind::Tempo, 0, 0, 0, 500000});
    doc.events.push_back(SmfEvent{960, 1, SmfEventKind::Tempo, 0, 0, 0, 1000000});

    SmfTimingMap timing;
    assert(timing.build(doc));

    SmfScheduledPosition pos{};
    assert(scheduleSmfTick(timing, 0, 100, 480, 22050, 512, pos));
    // 500 ms == 11025 samples == 21 blocks + 273 frames.
    assert(pos.blockSequence == 121);
    assert(pos.frameOffset == 273);

    assert(scheduleSmfTick(timing, 0, 100, 960, 22050, 512, pos));
    assert(pos.blockSequence == 143);
    assert(pos.frameOffset == 34);

    // After the tempo change, another quarter is 1 second rather than 0.5 s.
    assert(scheduleSmfTick(timing, 960, 200, 1440, 22050, 512, pos));
    assert(pos.blockSequence == 243);
    assert(pos.frameOffset == 34);

    // Triplet position must remain a non-grid sample offset rather than flattening.
    assert(scheduleSmfTick(timing, 0, 10, 160, 22050, 512, pos));
    const uint64_t tripletFrames =
        (timing.tickToMicros(160) * 22050ull + 500000ull) / 1000000ull;
    assert(pos.blockSequence == 10 + tripletFrames / 512);
    assert(pos.frameOffset == tripletFrames % 512);

    assert(!scheduleSmfTick(timing, 480, 0, 479, 22050, 512, pos));
    assert(!scheduleSmfTick(timing, 0, 0, 480, 0, 512, pos));
    assert(!scheduleSmfTick(timing, 0, 0, 480, 22050, 0, pos));

    // Inverse map supports pause/seek position recovery from the accepted audio anchor.
    assert(timing.microsToTick(500000) == 480);
    assert(timing.microsToTick(1000000) == 960);
    assert(timing.microsPerQuarterAtTick(959) == 500000);
    assert(timing.microsPerQuarterAtTick(960) == 1000000);

    return 0;
}
