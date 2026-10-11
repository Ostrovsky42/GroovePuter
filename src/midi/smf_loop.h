#pragma once
#ifndef GROOVEPUTER_SMF_LOOP_H
#define GROOVEPUTER_SMF_LOOP_H

#include <cstdint>

// SMF player loop: the whole song, or a bar section A..B (B inclusive).
//
// The player loops by restarting through its ordinary start path when the
// audio clock reaches the boundary, so the existing panic/NoteOff cleanup and
// generation filtering own every note across the jump. That costs a short gap
// (the start lead) at each wrap; a seamless wrap would have to rewind the
// stream ahead of time inside the scheduler and is deliberately not done here.
namespace GroovePuterMidi {

enum class SmfLoopMode : uint8_t {
    Off = 0,
    Song,
    Section,
};

constexpr uint32_t kSmfNoLoopBoundary = 0xFFFFFFFFu;
// A section loop started without marks covers this many bars from the playhead.
constexpr uint32_t kSmfDefaultSectionBars = 4u;

struct SmfLoopRegion {
    SmfLoopMode mode{SmfLoopMode::Off};
    uint32_t startBar{0};   // one-based; 0 = not marked
    uint32_t endBar{0};     // one-based, inclusive; 0 = not marked

    bool sectionMarked() const { return startBar != 0u && endBar != 0u; }
};

inline uint32_t clampSmfBar(uint32_t bar, uint32_t totalBars) {
    if (totalBars == 0u) totalBars = 1u;
    if (bar < 1u) return 1u;
    return bar > totalBars ? totalBars : bar;
}

// Keeps A <= B whichever mark was set last.
inline void orderSmfLoopMarks(SmfLoopRegion& region) {
    if (region.sectionMarked() && region.endBar < region.startBar) {
        const uint32_t swap = region.startBar;
        region.startBar = region.endBar;
        region.endBar = swap;
    }
}

inline void markSmfLoopStart(SmfLoopRegion& region,
                             uint32_t currentBar,
                             uint32_t totalBars) {
    region.startBar = clampSmfBar(currentBar, totalBars);
    if (region.endBar == 0u) {
        region.endBar = clampSmfBar(region.startBar + kSmfDefaultSectionBars - 1u, totalBars);
    }
    orderSmfLoopMarks(region);
}

// Setting B arms the section loop: that is what marking an end means.
inline void markSmfLoopEnd(SmfLoopRegion& region,
                           uint32_t currentBar,
                           uint32_t totalBars) {
    region.endBar = clampSmfBar(currentBar, totalBars);
    if (region.startBar == 0u) region.startBar = region.endBar;
    orderSmfLoopMarks(region);
    region.mode = SmfLoopMode::Section;
}

// OFF -> SONG -> SECTION -> OFF. Entering SECTION without marks loops
// kSmfDefaultSectionBars bars from the current bar.
inline void cycleSmfLoopMode(SmfLoopRegion& region,
                             uint32_t currentBar,
                             uint32_t totalBars) {
    switch (region.mode) {
        case SmfLoopMode::Off:
            region.mode = SmfLoopMode::Song;
            return;
        case SmfLoopMode::Song:
            if (!region.sectionMarked()) {
                region.startBar = clampSmfBar(currentBar, totalBars);
                region.endBar = clampSmfBar(
                    region.startBar + kSmfDefaultSectionBars - 1u, totalBars);
            }
            region.mode = SmfLoopMode::Section;
            return;
        case SmfLoopMode::Section:
        default:
            region.mode = SmfLoopMode::Off;
            return;
    }
}

// Tick at which playback must jump back: the end of bar B (capped at the end
// of the file) for a section, the end of the file for the song.
// tickForBar maps a one-based bar to its first tick.
template <typename TickForBar>
uint32_t smfLoopBoundaryTick(const SmfLoopRegion& region,
                             uint32_t endTick,
                             TickForBar tickForBar) {
    switch (region.mode) {
        case SmfLoopMode::Song:
            return endTick;
        case SmfLoopMode::Section: {
            if (!region.sectionMarked()) return kSmfNoLoopBoundary;
            const uint32_t afterB = tickForBar(region.endBar + 1u);
            return afterB < endTick ? afterB : endTick;
        }
        case SmfLoopMode::Off:
        default:
            return kSmfNoLoopBoundary;
    }
}

template <typename TickForBar>
uint32_t smfLoopRestartTick(const SmfLoopRegion& region,
                            uint32_t musicStartTick,
                            TickForBar tickForBar) {
    if (region.mode == SmfLoopMode::Section && region.sectionMarked()) {
        const uint32_t start = tickForBar(region.startBar);
        return start > musicStartTick ? start : musicStartTick;
    }
    return musicStartTick;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_LOOP_H
