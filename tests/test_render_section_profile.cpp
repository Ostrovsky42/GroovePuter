#include <cassert>
#include <cstdio>

#include "src/dsp/render_section_profile.h"

int main() {
    RenderProfile::SectionProfile profile;
    uint32_t first[RenderProfile::Count]{};
    uint32_t second[RenderProfile::Count]{};
    first[RenderProfile::Seq] = 2000;
    first[RenderProfile::SynthA] = 4000;
    second[RenderProfile::Seq] = 6000;
    second[RenderProfile::SynthA] = 2000;
    profile.publish(first);
    profile.publish(second);
    const auto window = profile.take(1000);
    assert(window.blocks == 2);
    assert(window.avgUs[RenderProfile::Seq] == 4);
    assert(window.maxUs[RenderProfile::Seq] == 6);
    assert(window.avgUs[RenderProfile::SynthA] == 3);
    assert(window.maxUs[RenderProfile::SynthA] == 4);
    const auto empty = profile.take(1000);
    assert(empty.blocks == 0 && empty.avgUs[RenderProfile::Seq] == 0);
    assert(empty.maxUs[RenderProfile::Seq] == 0);
    std::puts("render section profile: PASS");
}
