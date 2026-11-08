#pragma once
#ifndef GROOVEPUTER_SRC_AUDIO_MIDI_IMPORT_PROJECTION_H
#define GROOVEPUTER_SRC_AUDIO_MIDI_IMPORT_PROJECTION_H

#include <cstdint>

#include "src/midi/smf_document.h"
#include "src/phrase/runtime_phrase_edit.h"
#include "src/phrase/runtime_synth_events.h"

// SMF notes -> Melody (RuntimeSynthEventBuffer) for one window of one source.
//
// First consumer: GRAB on the MIDI player (a loop A-B of one HUB layer into
// the Working Melody). The full workspace import plan
// (docs/superpowers/plans/2026-10-03-midi-workspace-import.md, Task 2) is
// meant to reuse this instead of a second projection.
//
// Policy, all counted in the report rather than hidden:
// - time: file ticks -> 96 runtime ticks per quarter, durations in subticks
//   (16 per tick), at least one subtick; no grid quantization;
// - a note started before the window is not taken (its Note Off is ignored);
// - a note still sounding at the window end is cut there;
// - repeated Note On of the same channel/pitch pair with Note Offs FIFO;
// - more than kMaxSynthEvents notes: refused, never truncated;
// - Melody length = window rounded up to 1, 2, 4 or 8 bars of 4/4.
namespace MidiImport {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

constexpr uint32_t kRuntimeTicksPerQuarter = PhraseRuntime::kTicksPerBar / 4u;
constexpr uint8_t kMaxGrabBars = 8u;
constexpr uint8_t kMaxOpenNotes = 32u;

enum class GrabStatus : uint8_t {
    Ok = 0,
    Empty,          // no note of the source starts inside the window
    TooLong,        // the window is longer than kMaxGrabBars
    TooManyNotes,   // more than a Melody holds
    TooPolyphonic,  // more than kMaxOpenNotes notes held at once
    Invalid,        // bad window / division, or the result does not validate
};

inline const char* grabStatusText(GrabStatus status) {
    switch (status) {
        case GrabStatus::Ok: return "OK";
        case GrabStatus::Empty: return "NO NOTES IN RANGE";
        case GrabStatus::TooLong: return "MAX 8 BARS";
        case GrabStatus::TooManyNotes: return "TOO MANY NOTES";
        case GrabStatus::TooPolyphonic: return "TOO MANY HELD NOTES";
        case GrabStatus::Invalid:
        default: return "GRAB FAILED";
    }
}

struct GrabWindow {
    uint16_t division{0};       // file PPQN
    uint32_t startTick{0};      // file ticks, inclusive
    uint32_t endTick{0};        // file ticks, exclusive
    uint16_t channelMask{0xFFFFu};
};

struct GrabReport {
    uint16_t notes{0};
    uint16_t cutAtEnd{0};          // still sounding at the window end
    uint16_t startedBefore{0};     // Note Off whose Note On preceded the window
    uint8_t bars{0};
};

inline uint8_t melodyBarsForWindow(const GrabWindow& window) {
    if (window.division == 0u || window.endTick <= window.startTick) return 0u;
    const uint64_t span = window.endTick - window.startTick;
    const uint64_t barTicks = static_cast<uint64_t>(window.division) * 4u;
    const uint64_t bars = (span + barTicks - 1u) / barTicks;
    for (uint8_t allowed : {1u, 2u, 4u, 8u}) {
        if (bars <= allowed) return allowed;
    }
    return 0u;
}

class MelodyGrabBuilder {
public:
    GrabStatus begin(const GrabWindow& window, Buffer& out) {
        window_ = window;
        out_ = &out;
        report_ = GrabReport{};
        openCount_ = 0u;
        failed_ = GrabStatus::Ok;
        out = Buffer{};
        if (window.division == 0u || window.endTick <= window.startTick) {
            return failed_ = GrabStatus::Invalid;
        }
        const uint8_t bars = melodyBarsForWindow(window);
        if (bars == 0u) return failed_ = GrabStatus::TooLong;
        report_.bars = bars;
        out.lengthTicks = static_cast<uint16_t>(PhraseRuntime::kTicksPerBar * bars);
        return GrabStatus::Ok;
    }

    // Events of one source, in file order. Anything else is ignored.
    void feed(const GroovePuterMidi::SmfEvent& event) {
        if (failed_ != GrabStatus::Ok || !out_) return;
        if (event.channel > 15u ||
            (window_.channelMask & (1u << event.channel)) == 0u) {
            return;
        }
        if (event.kind == GroovePuterMidi::SmfEventKind::NoteOn) {
            if (event.tick < window_.startTick || event.tick >= window_.endTick) return;
            if (openCount_ >= kMaxOpenNotes) {
                failed_ = GrabStatus::TooPolyphonic;
                return;
            }
            open_[openCount_++] = Open{event.tick, event.channel, event.data1,
                                       event.data2 == 0u ? uint8_t{1} : event.data2};
            return;
        }
        if (event.kind != GroovePuterMidi::SmfEventKind::NoteOff) return;
        for (uint8_t i = 0u; i < openCount_; ++i) {  // FIFO: earliest first
            if (open_[i].channel != event.channel || open_[i].note != event.data1) continue;
            const Open note = open_[i];
            for (uint8_t j = i; j + 1u < openCount_; ++j) open_[j] = open_[j + 1u];
            --openCount_;
            const bool cut = event.tick > window_.endTick;
            if (cut) ++report_.cutAtEnd;
            emit(note, cut ? window_.endTick : event.tick);
            return;
        }
        if (event.tick >= window_.startTick && event.tick <= window_.endTick) {
            ++report_.startedBefore;
        }
    }

    // Closes notes still held at the window end and validates the Melody.
    GrabStatus finish() {
        if (failed_ != GrabStatus::Ok) return failed_;
        while (openCount_ > 0u) {
            ++report_.cutAtEnd;
            emit(open_[0], window_.endTick);
            for (uint8_t j = 0u; j + 1u < openCount_; ++j) open_[j] = open_[j + 1u];
            --openCount_;
            if (failed_ != GrabStatus::Ok) return failed_;
        }
        if (out_->count == 0u) return failed_ = GrabStatus::Empty;
        sortByStart();
        if (!RuntimePhraseEdit::validate(*out_)) return failed_ = GrabStatus::Invalid;
        report_.notes = out_->count;
        return GrabStatus::Ok;
    }

    const GrabReport& report() const { return report_; }

private:
    struct Open {
        uint32_t tick;
        uint8_t channel;
        uint8_t note;
        uint8_t velocity;
    };

    uint32_t toRuntimeTicks(uint32_t fileTicks) const {
        return static_cast<uint32_t>(
            (static_cast<uint64_t>(fileTicks) * kRuntimeTicksPerQuarter) / window_.division);
    }

    void emit(const Open& note, uint32_t offTick) {
        if (out_->count >= PhraseRuntime::kMaxSynthEvents) {
            failed_ = GrabStatus::TooManyNotes;
            return;
        }
        const uint32_t start = toRuntimeTicks(note.tick - window_.startTick);
        if (start >= out_->lengthTicks) return;
        const uint32_t span = offTick > note.tick ? offTick - note.tick : 0u;
        uint64_t duration =
            (static_cast<uint64_t>(span) * kRuntimeTicksPerQuarter *
             PhraseRuntime::kSubticksPerTick) / window_.division;
        if (duration == 0u) duration = 1u;
        const uint64_t room =
            (static_cast<uint64_t>(out_->lengthTicks) - start) * PhraseRuntime::kSubticksPerTick;
        if (duration > room) duration = room;
        if (duration > UINT16_MAX) duration = UINT16_MAX;

        PhraseRuntime::RuntimeSynthEvent event{};
        event.startTick = static_cast<uint16_t>(start);
        event.durationSubticks = static_cast<uint16_t>(duration);
        event.note = note.note;
        event.velocity = note.velocity > 127u ? uint8_t{127} : note.velocity;
        event.probability = 100;
        out_->events[out_->count++] = event;
    }

    void sortByStart() {
        for (uint16_t i = 1u; i < out_->count; ++i) {  // stable insertion sort
            const PhraseRuntime::RuntimeSynthEvent value = out_->events[i];
            uint16_t j = i;
            while (j > 0u && out_->events[j - 1u].startTick > value.startTick) {
                out_->events[j] = out_->events[j - 1u];
                --j;
            }
            out_->events[j] = value;
        }
    }

    GrabWindow window_{};
    Buffer* out_{nullptr};
    GrabReport report_{};
    Open open_[kMaxOpenNotes]{};
    uint8_t openCount_{0};
    GrabStatus failed_{GrabStatus::Ok};
};

}  // namespace MidiImport

#endif  // GROOVEPUTER_SRC_AUDIO_MIDI_IMPORT_PROJECTION_H
