#pragma once
#ifndef GROOVEPUTER_CALL_RESPONSE_MOTIF_H
#define GROOVEPUTER_CALL_RESPONSE_MOTIF_H

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "call_response_phrase_capture.h"

// CALL/RESPONSE phase 0: a compact, musician-readable description of one
// captured two-bar CALL. Absolute pitches are reduced to scale steps relative
// to the first note, so a transposed phrase has the same contour.
namespace GroovePuterCallResponse {

struct MotifDescriptor {
    static constexpr uint8_t kMaxContour = 8;

    // Bit i = an onset on sixteenth i of the two-bar window. 32 bits, not 16:
    // two bars of sixteenths do not fit a uint16 without losing a bar or the
    // sixteenth grid.
    uint32_t onsetMask{0};
    int8_t contour[kMaxContour]{};  // scale steps of note i relative to note 0
    uint8_t contourCount{0};        // min(noteCount, kMaxContour)
    uint8_t lastDegree{0};          // 0-based scale degree of the last note (0 = tonic)
    uint8_t noteCount{0};
    uint8_t lowNote{0};
    uint8_t highNote{0};
    uint8_t averageVelocity{0};
    uint8_t outOfScaleCount{0};     // chromatic input notes (snapped down for the contour)
};

// Scale-step index of a MIDI note: octave * scaleSize + index of the scale tone
// at or below the note. Chromatic notes are snapped down to the scale tone below.
inline int32_t absoluteScaleStep(uint8_t note, uint8_t rootPitchClass,
                                 const int8_t* intervals, uint8_t intervalCount,
                                 bool* inScale = nullptr) {
    const int32_t semis = static_cast<int32_t>(note) - static_cast<int32_t>(rootPitchClass % 12u);
    const int32_t octave = semis >= 0 ? semis / 12 : -((-semis + 11) / 12);
    const int32_t pitchClass = semis - octave * 12;
    uint8_t index = 0;
    bool exact = false;
    for (uint8_t i = 0; i < intervalCount; ++i) {
        if (intervals[i] <= pitchClass) {
            index = i;
            exact = intervals[i] == pitchClass;
        }
    }
    if (inScale) *inScale = exact;
    return octave * static_cast<int32_t>(intervalCount) + index;
}

inline bool describe(const PhraseCapture::Phrase& phrase,
                     uint8_t rootPitchClass,
                     const int8_t* intervals,
                     uint8_t intervalCount,
                     MotifDescriptor& out) {
    out = MotifDescriptor{};
    if (phrase.count == 0 || intervals == nullptr || intervalCount == 0) return false;
    out.noteCount = phrase.count;
    out.lowNote = 127;
    uint32_t velocitySum = 0;
    int32_t firstStep = 0;
    for (uint8_t i = 0; i < phrase.count; ++i) {
        const CapturedNote& note = phrase.notes[i];
        if (note.onset < PhraseCapture::kWindowSteps) out.onsetMask |= (1u << note.onset);
        if (note.note < out.lowNote) out.lowNote = note.note;
        if (note.note > out.highNote) out.highNote = note.note;
        velocitySum += note.velocity;
        bool inScale = true;
        const int32_t step = absoluteScaleStep(note.note, rootPitchClass,
                                               intervals, intervalCount, &inScale);
        if (!inScale) ++out.outOfScaleCount;
        if (i == 0) firstStep = step;
        if (i < MotifDescriptor::kMaxContour) {
            int32_t relative = step - firstStep;
            if (relative > 127) relative = 127;
            if (relative < -128) relative = -128;
            out.contour[i] = static_cast<int8_t>(relative);
            out.contourCount = static_cast<uint8_t>(i + 1);
        }
        if (i + 1 == phrase.count) {
            const int32_t size = static_cast<int32_t>(intervalCount);
            out.lastDegree = static_cast<uint8_t>(((step % size) + size) % size);
        }
    }
    out.averageVelocity = static_cast<uint8_t>((velocitySum + phrase.count / 2u) / phrase.count);
    return true;
}

// Match the average input velocity; never emit a silent NoteOn.
constexpr uint8_t kAnswerVelocityPercent = 100;
// Reserved for phase 1 (answer may rest); phase 0 never rests.
constexpr uint8_t kAnswerRestProbabilityPercent = 0;

inline uint8_t answerVelocityFor(uint8_t callAverageVelocity) {
    const uint32_t scaled = (static_cast<uint32_t>(callAverageVelocity) *
                                 kAnswerVelocityPercent + 50u) / 100u;
    if (scaled < 1u) return 1u;
    if (scaled > 127u) return 127u;
    return static_cast<uint8_t>(scaled);
}

// "[CR] bar=N mask=0b<bar1>_<bar2> contour=[0,2,-1] last=5 n=4 reg=60-67 vel=96"
// bar is 1-based. Returns the snprintf length (truncated output when > capacity).
inline int formatMotifLog(char* out, std::size_t capacity, uint32_t bar1Based,
                          const MotifDescriptor& d, const char* clockName,
                          uint8_t dropped) {
    if (!out || capacity == 0) return 0;
    char mask[2 + 32 + 1 + 1]{};
    std::size_t m = 0;
    mask[m++] = '0';
    mask[m++] = 'b';
    for (int bit = 0; bit < 32; ++bit) {
        if (bit == 16) mask[m++] = '_';
        mask[m++] = (d.onsetMask & (1u << bit)) ? '1' : '0';
    }
    mask[m] = '\0';
    char contour[MotifDescriptor::kMaxContour * 5 + 3]{};
    std::size_t c = 0;
    contour[c++] = '[';
    for (uint8_t i = 0; i < d.contourCount; ++i) {
        const int written = std::snprintf(contour + c, sizeof(contour) - c,
                                          i == 0 ? "%d" : ",%d", static_cast<int>(d.contour[i]));
        if (written <= 0) break;
        c += static_cast<std::size_t>(written);
        if (c >= sizeof(contour) - 2) { c = sizeof(contour) - 2; break; }
    }
    contour[c++] = ']';
    contour[c] = '\0';
    return std::snprintf(out, capacity,
                         "[CR] bar=%lu mask=%s contour=%s last=%u n=%u reg=%u-%u vel=%u oos=%u clk=%s drop=%u",
                         static_cast<unsigned long>(bar1Based), mask, contour,
                         static_cast<unsigned>(d.lastDegree + 1u),
                         static_cast<unsigned>(d.noteCount),
                         static_cast<unsigned>(d.lowNote),
                         static_cast<unsigned>(d.highNote),
                         static_cast<unsigned>(d.averageVelocity),
                         static_cast<unsigned>(d.outOfScaleCount),
                         clockName ? clockName : "?",
                         static_cast<unsigned>(dropped));
}

}  // namespace GroovePuterCallResponse

#endif  // GROOVEPUTER_CALL_RESPONSE_MOTIF_H
