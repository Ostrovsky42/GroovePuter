// SMF player loop region: modes, A/B marks and where playback jumps.
#include <cassert>
#include <cstdio>

#include "src/midi/smf_loop.h"

using namespace GroovePuterMidi;

namespace {
// 4/4 at 480 PPQN: bar n starts at (n - 1) * 1920.
uint32_t tickForBar(uint32_t bar) { return (bar - 1u) * 1920u; }
}  // namespace

int main() {
    constexpr uint32_t kTotalBars = 32u;
    constexpr uint32_t kEndTick = 32u * 1920u;
    constexpr uint32_t kMusicStart = 0u;

    // Off: no boundary, the file ends as before.
    SmfLoopRegion region{};
    assert(smfLoopBoundaryTick(region, kEndTick, tickForBar) == kSmfNoLoopBoundary);

    // L: OFF -> SONG loops at the end of the file back to the music start.
    cycleSmfLoopMode(region, 5u, kTotalBars);
    assert(region.mode == SmfLoopMode::Song);
    assert(smfLoopBoundaryTick(region, kEndTick, tickForBar) == kEndTick);
    assert(smfLoopRestartTick(region, kMusicStart, tickForBar) == kMusicStart);

    // SONG -> SECTION without marks: four bars from the current bar.
    cycleSmfLoopMode(region, 5u, kTotalBars);
    assert(region.mode == SmfLoopMode::Section);
    assert(region.startBar == 5u && region.endBar == 8u);
    assert(smfLoopBoundaryTick(region, kEndTick, tickForBar) == tickForBar(9u));
    assert(smfLoopRestartTick(region, kMusicStart, tickForBar) == tickForBar(5u));

    // SECTION -> OFF keeps the marks for the next time.
    cycleSmfLoopMode(region, 5u, kTotalBars);
    assert(region.mode == SmfLoopMode::Off && region.sectionMarked());

    // ] arms the section; B is inclusive, so the jump is at the end of bar B.
    region = SmfLoopRegion{};
    markSmfLoopStart(region, 9u, kTotalBars);
    assert(region.mode == SmfLoopMode::Off);            // [ alone does not start looping
    markSmfLoopEnd(region, 12u, kTotalBars);
    assert(region.mode == SmfLoopMode::Section);
    assert(region.startBar == 9u && region.endBar == 12u);
    assert(smfLoopBoundaryTick(region, kEndTick, tickForBar) == tickForBar(13u));

    // Marks in either order give A <= B; one-bar sections work.
    region = SmfLoopRegion{};
    markSmfLoopEnd(region, 7u, kTotalBars);
    assert(region.startBar == 7u && region.endBar == 7u);
    markSmfLoopStart(region, 10u, kTotalBars);
    assert(region.startBar == 7u && region.endBar == 10u);

    // B on the last bar: the boundary is capped at the end of the file.
    region = SmfLoopRegion{};
    markSmfLoopStart(region, 30u, kTotalBars);
    markSmfLoopEnd(region, 40u, kTotalBars);              // clamped to the last bar
    assert(region.endBar == kTotalBars);
    assert(smfLoopBoundaryTick(region, kEndTick, tickForBar) == kEndTick);

    // A restart never lands before the first note of the file.
    region = SmfLoopRegion{};
    markSmfLoopStart(region, 1u, kTotalBars);
    markSmfLoopEnd(region, 2u, kTotalBars);
    assert(smfLoopRestartTick(region, 960u, tickForBar) == 960u);

    std::puts("smf loop region: PASS");
    return 0;
}
