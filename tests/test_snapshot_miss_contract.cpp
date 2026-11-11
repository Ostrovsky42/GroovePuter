// Part C of docs/midi/2026-11-10-snapshot-miss-and-smf-retrigger-design.md: a
// missed ProjectTransportTimeline::trySnapshot() ("unavailable": the seqlock
// writer was mid-update for all 4 attempts) must not look like "no transport"
// ("invalid"), so the PERFORM harmony context is not cleared by one missed read.
//
//   T1 the timeline has three states: unavailable / invalid / valid.
//   T2 the wrapper snapshot() marks a miss (readMissed) and leaves the honest
//      "no transport" snapshot unmarked. Behavioural.
//   T3 STRUCTURAL ONLY (reads GroovePuter.ino as text): the function is static
//      and cannot be linked into a host test. It checks that
//      updatePerformanceHarmonyContext() returns on HarmonyCapture::Unavailable
//      before g_performanceHarmonyBridge.update(). The behavioural half (the
//      engine returning Unavailable vs Invalid) is covered by
//      test_performance_boundary_ui.cpp (Invalid) and by T1/T2 for the timeline.
//
// run from the repo root: g++ -std=c++17 -Wall -Wextra -Werror -I.
//   tests/test_snapshot_miss_contract.cpp -o <out> && <out>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

// Test-only: force the seqlock into the "writer is mid-update" state.
#define private public
#include "src/midi/project_transport_timeline.h"
#undef private

using namespace GroovePuterMidi;

namespace {

int gFailures = 0;
void report(const char* name, bool ok, const char* detail) {
    std::printf("%s %s  (%s)\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) ++gFailures;
}

void t1ThreeStates() {
    ProjectTransportTimeline timeline;
    ProjectTransportBlockSnapshot out{};
    const bool invalidRead = timeline.trySnapshot(out);
    const bool invalidOk = invalidRead && !out.valid;  // never published

    timeline.publishBlock(1, 512, 0.0f, 120.0f, 22050.0f, true);
    ProjectTransportBlockSnapshot valid{};
    const bool validOk = timeline.trySnapshot(valid) && valid.valid && valid.playing;

    timeline.sequence_.fetch_add(1);  // odd: the writer is inside publishBlock
    ProjectTransportBlockSnapshot miss{};
    const bool unavailableOk = !timeline.trySnapshot(miss);
    timeline.sequence_.fetch_add(1);
    report("T1 timeline has unavailable / invalid / valid", invalidOk && validOk && unavailableOk,
           "trySnapshot false = unavailable; true+!valid = invalid; true+valid = valid");
}

void t2WrapperCollapsesStates() {
    ProjectTransportTimeline invalidTimeline;  // never published: invalid
    ProjectTransportTimeline missTimeline;
    missTimeline.publishBlock(1, 512, 0.0f, 120.0f, 22050.0f, true);
    missTimeline.sequence_.fetch_add(1);  // unavailable
    const ProjectTransportBlockSnapshot a = invalidTimeline.snapshot();
    const ProjectTransportBlockSnapshot b = missTimeline.snapshot();
    const bool distinguishable = b.readMissed && !a.readMissed;
    ProjectTransportTimeline okTimeline;
    okTimeline.publishBlock(1, 512, 0.0f, 120.0f, 22050.0f, true);
    const ProjectTransportBlockSnapshot c = okTimeline.snapshot();
    const bool okUnmarked = !c.readMissed && c.valid && c.playing;
    report("T2 snapshot() lets a caller tell a miss from 'no transport'", distinguishable && okUnmarked,
           "readMissed set only on a miss; valid read and never-published read are unmarked");
}

void t3HarmonyContextSourceStructure() {
    std::ifstream in("GroovePuter.ino");
    if (!in) {
        report("T3 GroovePuter.ino: unavailable path in updatePerformanceHarmonyContext", false,
               "GroovePuter.ino not found: run from the repo root");
        return;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string src = buffer.str();
    const size_t begin = src.find("static void updatePerformanceHarmonyContext()");
    const size_t end = begin == std::string::npos ? begin : src.find("\nstatic ", begin + 10);
    const std::string body = begin == std::string::npos ? std::string() : src.substr(begin, end - begin);
    const size_t unavailable = body.find("== HarmonyCapture::Unavailable) return;");
    const size_t patternUnavailable = body.find("patternCapture == HarmonyCapture::Unavailable) return;");
    const size_t bridge = body.find("g_performanceHarmonyBridge.update(g_performanceKeyboard, snapshot)");
    const bool ok = unavailable != std::string::npos && patternUnavailable != std::string::npos &&
                    bridge != std::string::npos && unavailable < bridge && patternUnavailable < bridge;
    report("T3 (structural) harmony context keeps the old state on an unavailable capture", ok,
           "Song and Pattern Unavailable both return before bridge.update; source text check only");
}

}  // namespace

int main() {
    t1ThreeStates();
    t2WrapperCollapsesStates();
    t3HarmonyContextSourceStructure();
    std::printf("%d failing case(s)\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
