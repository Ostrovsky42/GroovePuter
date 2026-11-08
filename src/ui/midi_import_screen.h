#pragma once

#include <cstdint>
#include <cstring>

// PROJECT > Import MIDI track list: pure state and rules, no drawing.
// A row is one used MIDI channel of the scanned file (the importer routes by
// channel), each with a target: Synth A, Synth B, Drums or Skip.
namespace MidiImportScreen {

enum class Target : uint8_t { A = 0, B, Drums, Skip };

constexpr uint8_t kMaxRows = 16;
constexpr uint8_t kMaxBars = 32;  // occupancy bits kept per channel

struct Row {
    uint8_t channel{1};      // 1..16
    int noteCount{0};
    uint8_t minNote{127};
    uint8_t maxNote{0};
    char name[7]{};          // 6 chars + NUL
    uint32_t barMask{0};     // bit n = bar n (0-based) has note starts
    Target target{Target::Skip};
};

struct State {
    Row rows[kMaxRows];
    uint8_t count{0};
    uint8_t cursor{0};       // selected row
    uint8_t scroll{0};       // first visible row
    uint8_t fileBars{0};     // known bars, capped at kMaxBars
    uint8_t barCursor{0};
    uint8_t rangeStart{0};   // first imported bar (0-based, inclusive)
    uint8_t rangeEnd{0};     // last imported bar (0-based, inclusive)
};

// A -> B -> DRUMS -> SKIP -> A.
inline Target nextTarget(Target t) {
    switch (t) {
        case Target::A: return Target::B;
        case Target::B: return Target::Drums;
        case Target::Drums: return Target::Skip;
        default: return Target::A;
    }
}

inline const char* targetLabel(Target t) {
    switch (t) {
        case Target::A: return "A";
        case Target::B: return "B";
        case Target::Drums: return "DRUMS";
        default: return "SKIP";
    }
}

inline void reset(State& s, int estimatedBars) {
    s = State{};
    int bars = estimatedBars < 1 ? 1 : estimatedBars;
    if (bars > kMaxBars) bars = kMaxBars;
    s.fileBars = static_cast<uint8_t>(bars);
    s.rangeStart = 0;
    s.rangeEnd = static_cast<uint8_t>(bars - 1);
}

inline bool addRow(State& s, uint8_t channel, int noteCount, uint8_t minNote,
                   uint8_t maxNote, const char* name, uint32_t barMask) {
    if (s.count >= kMaxRows || channel < 1 || channel > 16) return false;
    Row& r = s.rows[s.count++];
    r = Row{};
    r.channel = channel;
    r.noteCount = noteCount;
    r.minNote = minNote;
    r.maxNote = maxNote;
    if (name) {
        std::strncpy(r.name, name, sizeof(r.name) - 1);
        r.name[sizeof(r.name) - 1] = '\0';
    }
    r.barMask = barMask;
    return true;
}

// Channel 10 -> DRUMS; the lowest melodic track (by lowest note) -> A; the
// next lowest -> B; everything else SKIP.
inline void assignInitialTargets(State& s) {
    int a = -1;
    int b = -1;
    for (uint8_t i = 0; i < s.count; ++i) {
        Row& r = s.rows[i];
        r.target = Target::Skip;
        if (r.channel == 10) { r.target = Target::Drums; continue; }
        if (a < 0 || r.minNote < s.rows[a].minNote) {
            b = a;
            a = i;
        } else if (b < 0 || r.minNote < s.rows[b].minNote) {
            b = i;
        }
    }
    if (a >= 0) s.rows[a].target = Target::A;
    if (b >= 0) s.rows[b].target = Target::B;
}

inline void cycleTarget(State& s) {
    if (s.cursor < s.count) {
        s.rows[s.cursor].target = nextTarget(s.rows[s.cursor].target);
    }
}

// 'A' or 'B' when two tracks share that synth, otherwise 0.
inline char clashSynth(const State& s) {
    int a = 0;
    int b = 0;
    for (uint8_t i = 0; i < s.count; ++i) {
        if (s.rows[i].target == Target::A) ++a;
        if (s.rows[i].target == Target::B) ++b;
    }
    return a > 1 ? 'A' : (b > 1 ? 'B' : 0);
}

inline bool rowClashes(const State& s, uint8_t row) {
    if (row >= s.count) return false;
    const Target t = s.rows[row].target;
    if (t != Target::A && t != Target::B) return false;
    for (uint8_t i = 0; i < s.count; ++i) {
        if (i != row && s.rows[i].target == t) return true;
    }
    return false;
}

struct Masks {
    uint16_t a{0};
    uint16_t b{0};
    uint16_t d{0};
    bool any() const { return (a | b | d) != 0; }
};

inline Masks buildMasks(const State& s) {
    Masks m;
    for (uint8_t i = 0; i < s.count; ++i) {
        const uint16_t bit = static_cast<uint16_t>(1u << (s.rows[i].channel - 1));
        switch (s.rows[i].target) {
            case Target::A: m.a |= bit; break;
            case Target::B: m.b |= bit; break;
            case Target::Drums: m.d |= bit; break;
            default: break;
        }
    }
    return m;
}

inline void moveRow(State& s, int delta, uint8_t visible) {
    if (s.count == 0) return;
    int c = static_cast<int>(s.cursor) + delta;
    if (c < 0) c = 0;
    if (c >= s.count) c = s.count - 1;
    s.cursor = static_cast<uint8_t>(c);
    if (s.cursor < s.scroll) s.scroll = s.cursor;
    if (visible > 0 && s.cursor >= s.scroll + visible) {
        s.scroll = static_cast<uint8_t>(s.cursor - visible + 1);
    }
}

inline void moveBar(State& s, int delta) {
    int c = static_cast<int>(s.barCursor) + delta;
    if (c < 0) c = 0;
    if (c >= s.fileBars) c = s.fileBars - 1;
    s.barCursor = static_cast<uint8_t>(c);
}

inline void setRangeStart(State& s) {
    s.rangeStart = s.barCursor;
    if (s.rangeEnd < s.rangeStart) s.rangeEnd = s.rangeStart;
}

inline void setRangeEnd(State& s) {
    s.rangeEnd = s.barCursor;
    if (s.rangeStart > s.rangeEnd) s.rangeStart = s.rangeEnd;
}

// The importer counts source bars from the first routed note (floored to a
// bar). Convert the absolute range to importer settings: `from` is relative to
// the first bar where any routed track has notes. A range before that bar just
// loses its leading empty bars.
inline void rangeToSettings(const State& s, int& fromBar, int& lengthBars) {
    const Masks m = buildMasks(s);
    int first = -1;
    for (uint8_t i = 0; i < s.count && m.any(); ++i) {
        if (s.rows[i].target == Target::Skip || s.rows[i].barMask == 0) continue;
        int lowest = 0;
        while (!((s.rows[i].barMask >> lowest) & 1u)) ++lowest;
        if (first < 0 || lowest < first) first = lowest;
    }
    if (first < 0) first = 0;
    int from = static_cast<int>(s.rangeStart) - first;
    int length = static_cast<int>(s.rangeEnd) - static_cast<int>(s.rangeStart) + 1;
    if (from < 0) {
        length += from;
        from = 0;
    }
    if (length < 1) length = 1;
    fromBar = from;
    lengthBars = length;
}

}  // namespace MidiImportScreen
