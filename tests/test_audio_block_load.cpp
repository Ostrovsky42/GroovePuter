#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "src/audio/audio_block_load.h"

int main() {
    AudioBlockLoad load;
    constexpr uint32_t budget = 23219;
    for (uint32_t us : {10000u, 24000u, 25000u, 30000u, 9000u, 24000u, 8000u}) {
        load.record(us, budget);
    }
    const auto first = load.take();
    assert(first.blocks == 7 && first.renderUs == 130000);
    assert(first.avgUs() == 18571 && first.maxUs == 30000);
    assert(first.overBudget == 4 && first.longestRun == 3);

    load.record(24000, budget);
    load.record(5000, budget);
    const auto second = load.take();
    assert(second.blocks == 2 && second.overBudget == 1);
    assert(second.longestRun == 1 && second.maxUs == 24000);
    const auto empty = load.take();
    assert(empty.blocks == 0 && empty.avgUs() == 0 && empty.maxUs == 0);
    std::puts("audio block load: PASS");
}
