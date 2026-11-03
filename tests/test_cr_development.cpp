#include <cassert>
#include <initializer_list>
#include "src/input/call_response_development.h"
#include "src/input/call_response_performance_plan.h"
using namespace GroovePuterCallResponse;
int main() {
    PhraseCapture::Phrase call{};
    call.windowSteps = 16;
    call.count = 4;
    for (uint8_t i = 0; i < 4; ++i) call.notes[i] = CapturedNote{static_cast<uint8_t>(i * 2), 2, static_cast<uint8_t>(60 + i), 100};
    PhraseCapture::Phrase out{};
    assert(!transformCall(call, ReplyStrategy::Echo, 1, out));
    assert(out.count == 4 && out.notes[2].onset == 4);
    assert(transformCall(call, ReplyStrategy::ShortReply, 1, out));
    assert(out.count == 2 && out.notes[0].onset == 0 && out.notes[1].onset == 2);
    assert(transformCall(call, ReplyStrategy::Develop, 1, out));
    assert(out.count == 4 && out.notes[0].onset == 0 && out.notes[1].onset == 2 && out.notes[2].onset == 5 && out.notes[3].onset == 6);
    assert(out.notes[2].length == 1);
    call.count = 1;
    assert(!transformCall(call, ReplyStrategy::Develop, 1, out));
    assert(out.count == 1);
    call.count = 3;
    call.notes[0] = CapturedNote{0, 2, 60, 100};
    call.notes[1] = CapturedNote{12, 2, 62, 100};
    call.notes[2] = CapturedNote{14, 1, 64, 100};
    assert(!transformCall(call, ReplyStrategy::Develop, 1, out));
    assert(out.notes[2].onset == 14);
    call.count = 4;
    call.notes[3] = CapturedNote{15, 1, 65, 100};
    MotifDescriptor motif{};
    motif.contourCount = 4;
    motif.contour[1] = 1;
    const uint32_t signature = callSignature(call, motif);
    call.windowStartStep = 64;
    for (uint8_t i = 0; i < call.count; ++i) call.notes[i].note += 12;
    assert(callSignature(call, motif) == signature);
    ReplyMemory memory{};
    assert(chooseReplyStrategy(call, motif, memory) == ReplyStrategy::Develop);
    rememberReply(memory, signature, ReplyStrategy::Develop);
    assert(chooseReplyStrategy(call, motif, memory) == ReplyStrategy::ShortReply);
    rememberReply(memory, signature, ReplyStrategy::ShortReply);
    assert(chooseReplyStrategy(call, motif, memory) == ReplyStrategy::Echo);
    call.count = 4;
    call.windowSteps = 16;
    for (uint8_t i = 0; i < 4; ++i) call.notes[i] = CapturedNote{static_cast<uint8_t>(i * 3), 2, static_cast<uint8_t>(60 + i), static_cast<uint8_t>(80 + i)};
    ResponsePlan plan{};
    ReplyStrategy actual{};
    ResponseHarmonyReport report{};
    assert(planPerformanceReply(call, motif, ResponseKey{}, ResponseHarmony{}, ResponseStyle{}, memory, true, false, 0, 0, plan, actual, report));
    assert(plan.count == call.count);
    for (uint8_t i = 0; i < plan.count; ++i) {
        assert(plan.notes[i].note == call.notes[i].note);
        assert(plan.notes[i].step == call.notes[i].onset);
        assert(plan.notes[i].velocity == call.notes[i].velocity);
    }
    ResponseStyle late{};
    late.genre = 0;
    late.ending = GenreIdiom::Ending::Late;
    assert(planPerformanceReply(call, motif, ResponseKey{}, ResponseHarmony{}, late, memory, true, false, 1, 0, plan, actual, report));
    assert(plan.notes[3].step == call.notes[3].onset);
    ReplyMemory shortMemory{};
    rememberReply(shortMemory, callSignature(call, motif), ReplyStrategy::Develop);
    assert(planPerformanceReply(call, motif, ResponseKey{}, ResponseHarmony{}, late, shortMemory, true, false, 3, 0, plan, actual, report));
    // Phase 7: LOOP never plays SHORT REPLY (it left half the loop silent);
    // the turn after DEVELOP is ECHO with every note (EVOLVE 3 may move the
    // tail by one step).
    assert(actual == ReplyStrategy::Echo && plan.count == call.count);
    for (uint8_t i = 0; i < plan.count; ++i)
        assert(plan.notes[i].step == call.notes[i].onset || (i >= 3 && plan.notes[i].step == call.notes[i].onset + 1));
    call.count = 8;
    for (uint8_t i = 0; i < 8; ++i) call.notes[i] = CapturedNote{static_cast<uint8_t>(i * 2), 1, 60, 100};
    assert(planPerformanceReply(call, motif, ResponseKey{}, ResponseHarmony{}, ResponseStyle{}, memory, false, true, 1, 1, plan, actual, report));
    assert(plan.count == 4 && actual == ReplyStrategy::ShortReply);
    call.windowSteps = 32;
    for (uint8_t i = 0; i < 8; ++i) call.notes[i] = CapturedNote{static_cast<uint8_t>(i * 3), 1, 60, 100};
    ReplyMemory evolving{};
    const uint32_t evolvingSignature = callSignature(call, motif);
    // Phase 7: LOOP alternates DEVELOP <-> ECHO (SHORT REPLY -> ECHO).
    const ReplyStrategy expected[] = {ReplyStrategy::Develop, ReplyStrategy::Echo, ReplyStrategy::Develop};
    for (ReplyStrategy next : expected) {
        assert(planPerformanceReply(call, motif, ResponseKey{}, ResponseHarmony{}, ResponseStyle{}, evolving, true, false, 3, 0, plan, actual, report));
        assert(actual == next);
        rememberReply(evolving, evolvingSignature, actual);
    }
    for (uint8_t bars : {uint8_t{1}, uint8_t{2}}) {
        call.windowSteps = bars * 16;
        for (uint8_t n = 1; n <= 16; ++n) {
            call.count = n;
            for (uint8_t i = 0; i < n; ++i) call.notes[i] = CapturedNote{static_cast<uint8_t>(i * bars), 1, 60, 100};
            for (uint8_t level = 0; level < 4; ++level) for (uint8_t delay = 0; delay < 2; ++delay) {
                assert(planPerformanceReply(call, motif, ResponseKey{}, ResponseHarmony{}, ResponseStyle{}, evolving, true, false, level, delay, plan, actual, report));
                assert(plan.count > 0 && plan.count <= 16);
                for (uint8_t i = 0; i < plan.count; ++i) {
                    assert(plan.notes[i].length >= 1);
                    assert(plan.notes[i].step + plan.notes[i].length <= call.windowSteps - delay);
                    if (i > 0) assert(plan.notes[i - 1].step < plan.notes[i].step);
                }
            }
        }
    }
    return 0;
}
