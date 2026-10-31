#pragma once

#include <cstddef>
#include <cstdint>

// HUB MIDI row strip: one cell per bar of the file (bars are grouped when the
// strip is narrower than the file). Pure: no drawing, no globals.
namespace HubMidiBarStrip {

constexpr uint8_t kMaxBars = 32;  // occupancy kept per layer by the inspector
constexpr uint8_t kCellEmpty = 0;
constexpr uint8_t kCellNotes = 1;

// Bars the strip can speak for: the file's bars, capped at the stored 32.
inline uint8_t knownBars(uint32_t fileBars) {
    return fileBars > kMaxBars ? kMaxBars : static_cast<uint8_t>(fileBars);
}

// Cells for `fileBars` bars within `maxCells`. Bit n of `occupancy` = bar n
// (0-based) has note starts. A cell groups bars [first, last); the final
// cell takes the remainder, so a short last group is still one cell.
inline uint8_t cellCount(uint32_t fileBars, uint8_t maxCells) {
    const uint8_t bars = knownBars(fileBars);
    return bars < maxCells ? bars : maxCells;
}

inline uint8_t cellFirstBar(uint8_t cell, uint8_t cells, uint8_t bars) {
    return cells == 0 ? 0 : static_cast<uint8_t>(
        (static_cast<uint32_t>(cell) * bars) / cells);
}

inline uint8_t fill(uint32_t occupancy,
                    uint32_t fileBars,
                    uint8_t maxCells,
                    uint8_t* states,
                    std::size_t capacity) {
    const uint8_t bars = knownBars(fileBars);
    uint8_t cells = cellCount(fileBars, maxCells);
    if (cells > capacity) cells = static_cast<uint8_t>(capacity);
    for (uint8_t cell = 0; cell < cells; ++cell) {
        const uint8_t first = cellFirstBar(cell, cells, bars);
        const uint8_t end = (cell + 1u == cells)
            ? bars
            : cellFirstBar(static_cast<uint8_t>(cell + 1u), cells, bars);
        uint8_t state = kCellEmpty;
        for (uint8_t bar = first; bar < end; ++bar) {
            if ((occupancy >> bar) & 1u) { state = kCellNotes; break; }
        }
        states[cell] = state;
    }
    return cells;
}

}  // namespace HubMidiBarStrip
