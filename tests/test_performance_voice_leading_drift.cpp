// NEAREST voice leading must not drift in octave over long cycle-of-fourths progressions.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

#include "src/input/performance_keyboard.h"

namespace {
class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override { events.push_back(event); }
    std::vector<MusicalEvent> events;
};

constexpr int kBase = 48;

// Plays 24 major chords, root moving by `step` semitones (mod 12) from kBase.
bool runProgression(int step, int& minNote, int& maxNote, double& centroidSpan) {
    MusicalEventRouter router;
    RecordingSink sink;
    router.addSink(sink);
    PerformanceKeyboard kb(router);
    kb.setChordMode(PerformanceChordMode::Major);
    kb.setVoiceLeading(PerformanceVoiceLeading::Nearest);
    minNote = 127;
    maxNote = 0;
    double cMin = 1e9, cMax = -1e9;
    int pc = 0;
    for (int i = 0; i < 24; ++i) {
        sink.events.clear();
        const uint8_t root = static_cast<uint8_t>(kBase + pc);
        if (!kb.externalNoteOn(root, 100)) return false;
        double sum = 0;
        int n = 0;
        for (const MusicalEvent& e : sink.events) {
            if (e.type != MusicalEventType::NoteOn) continue;
            minNote = std::min<int>(minNote, e.note);
            maxNote = std::max<int>(maxNote, e.note);
            sum += e.note;
            ++n;
        }
        if (n == 0) return false;
        const double c = sum / n;
        cMin = std::min(cMin, c);
        cMax = std::max(cMax, c);
        kb.externalNoteOff(root);
        pc = ((pc + step) % 12 + 12) % 12;
    }
    centroidSpan = cMax - cMin;
    std::fprintf(stderr, "step %+d: notes [%d,%d] centroid span %.2f\n", step, minNote, maxNote, centroidSpan);
    return true;
}
}  // namespace

int main() {
    for (int step : {5, -5}) {
        int lo, hi;
        double span;
        assert(runProgression(step, lo, hi, span));
        assert(lo >= kBase - 12);
        assert(hi <= kBase + 19);
        assert(span <= 12.0);
    }
    std::fputs("voice leading drift: OK\n", stdout);
    return 0;
}
