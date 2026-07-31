#include <cassert>
#include <cstdint>

#include "src/midi/smf_timing.h"

using namespace GroovePuterMidi;

int main() {
    SmfDocument doc;
    doc.format = 1;
    doc.division = 480;
    doc.endTick = 3840;

    // 4/4 at tick 0, 120 BPM until tick 960, then 60 BPM.
    doc.events.push_back(SmfEvent{0, 0, SmfEventKind::Tempo, 0, 0, 0, 500000});
    doc.events.push_back(SmfEvent{0, 1, SmfEventKind::TimeSignature, 0, 4, 2, 0});
    doc.events.push_back(SmfEvent{960, 2, SmfEventKind::Tempo, 0, 0, 0, 1000000});
    // Change to 3/4 exactly at the start of bar 2.
    doc.events.push_back(SmfEvent{1920, 3, SmfEventKind::TimeSignature, 0, 3, 2, 0});

    SmfTimingMap map;
    assert(map.build(doc));
    assert(map.valid());
    assert(map.division() == 480);

    assert(map.tempoPoints().size() == 2);
    assert(map.tempoPoints()[0].tick == 0);
    assert(map.tempoPoints()[0].microsPerQuarter == 500000);
    assert(map.tempoPoints()[1].tick == 960);
    assert(map.tempoPoints()[1].microsAtTick == 1000000);
    assert(map.tempoPoints()[1].microsPerQuarter == 1000000);

    assert(map.tickToMicros(0) == 0);
    assert(map.tickToMicros(480) == 500000);
    assert(map.tickToMicros(960) == 1000000);
    assert(map.tickToMicros(1440) == 2000000);
    assert(map.tickToMicros(1920) == 3000000);

    SmfBarBeat pos = map.barBeatForTick(0);
    assert(pos.bar == 1 && pos.beat == 1 && pos.tickInBeat == 0);

    pos = map.barBeatForTick(480);
    assert(pos.bar == 1 && pos.beat == 2 && pos.tickInBeat == 0);

    pos = map.barBeatForTick(1919);
    assert(pos.bar == 1 && pos.beat == 4 && pos.tickInBeat == 479);

    pos = map.barBeatForTick(1920);
    assert(pos.bar == 2 && pos.beat == 1 && pos.tickInBeat == 0);

    pos = map.barBeatForTick(2400);
    assert(pos.bar == 2 && pos.beat == 2 && pos.tickInBeat == 0);

    // 3/4 bar is 1440 ticks, so bar 3 begins at 3360.
    pos = map.barBeatForTick(3360);
    assert(pos.bar == 3 && pos.beat == 1 && pos.tickInBeat == 0);

    assert(map.tickForBar(1) == 0);
    assert(map.tickForBar(2) == 1920);
    assert(map.tickForBar(3) == 3360);

    // Default maps are 120 BPM and 4/4 when the file has no metadata.
    SmfDocument defaults;
    defaults.division = 96;
    SmfTimingMap defaultMap;
    assert(defaultMap.build(defaults));
    assert(defaultMap.tickToMicros(96) == 500000);
    assert(defaultMap.tickForBar(2) == 384);

    SmfDocument invalid;
    invalid.division = 0;
    SmfTimingMap invalidMap;
    assert(!invalidMap.build(invalid));
    assert(!invalidMap.valid());

    return 0;
}
