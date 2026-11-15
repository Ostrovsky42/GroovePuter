#pragma once
// Macro -> CC link (CC LINK on PERFORM > MACRO). Pure rules plus the two values the
// MIDI side reads: the link mode (set by the page) and the published macro state.
// Off by default: with no link the macros send nothing.
#include <atomic>
#include <cstdint>

#include "performance_macro_engine.h"
#include "src/midi/cutoff_sweep.h"

namespace GroovePuterPerformance {
namespace MacroCc {

enum class Link : uint8_t { Off = 0, A, B, All };

inline Link nextLink(Link link) {
    return link == Link::Off ? Link::A
         : link == Link::A ? Link::B
         : link == Link::B ? Link::All : Link::Off;
}

inline const char* linkName(Link link) {
    switch (link) {
        case Link::A: return "A";
        case Link::B: return "B";
        case Link::All: return "ALL";
        default: return "OFF";
    }
}

// Mode and macro state shared with the MIDI side. The page writes the mode; the
// engine publishes the packed macro state whenever it changes.
inline volatile uint8_t gLinkMode = 0;
inline std::atomic<uint32_t> gPublishedMacroState{0};

// Controller indices in GroovePuterMidi::kControllers.
inline constexpr uint8_t kIndexCutoff = 0;
inline constexpr uint8_t kIndexReverb = 1;
inline constexpr uint8_t kIndexDelay = 2;
inline constexpr uint8_t kIndexReso = 3;
inline constexpr uint8_t kIndexExpr = 8;

// Percent of the link that reaches a voice: A = SYNTH A (voice 0), B = SYNTH B.
inline int voiceShare(Link link, uint8_t voice) {
    if (link == Link::Off) return 0;
    if (link == Link::All) return 100;
    if (link == Link::A) return voice == 0 ? 100 : 0;
    return voice == 1 ? 100 : 0;
}

// Offsets (signed, in controller units) a macro state adds on one voice. The share
// is applied last so a half share rounds the same way everywhere. SPACE is the one
// macro that favours SYNTH B: on ALL, SYNTH A gets half of it.
inline void offsetsFor(const Macros::State& state, Link link, uint8_t voice,
                       int out[GroovePuterMidi::kControllerCount]) {
    for (uint8_t i = 0; i < GroovePuterMidi::kControllerCount; ++i) out[i] = 0;
    const int share = voiceShare(link, voice);
    if (share == 0) return;
    const int tension = state.tension;
    const int space = state.space;
    const int energy = state.energy;
    // TENSION: cutoff both ways; resonance only on the positive side, and smaller.
    out[kIndexCutoff] += tension * 24 / 100 * share / 100;
    if (tension > 0) out[kIndexReso] += tension * 12 / 100 * share / 100;
    // SPACE: reverb and delay sends, SYNTH B at full share, SYNTH A at half on ALL.
    int spaceShare = share;
    if (link == Link::All && voice == 0) spaceShare = 50;
    out[kIndexReverb] += space * 40 / 100 * spaceShare / 100;
    out[kIndexDelay] += space * 30 / 100 * spaceShare / 100;
    // ENERGY: only the negative side, as EXPRESSION 127 down to about 84.
    if (energy < 0) out[kIndexExpr] += energy * 43 / 100 * share / 100;
}

}  // namespace MacroCc
}  // namespace GroovePuterPerformance
