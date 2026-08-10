#include <cassert>
#include <cstdint>

#include "src/input/musical_event_queue.h"

namespace GroovePuterVisual {
GvepR0EventBus& gvepR0EventBus() {
    static GvepR0EventBus bus;
    return bus;
}
}  // namespace GroovePuterVisual

namespace {
float readPhase(void* context) {
    return *static_cast<float*>(context);
}

GroovePuterVisual::GvepEvent popVisual() {
    GroovePuterVisual::GvepEvent event{};
    const bool ok = GroovePuterVisual::gvepR0EventBus().tryPop(event);
    assert(ok);
    return event;
}
}  // namespace

int main() {
    using namespace GroovePuterVisual;

    MusicalEventQueue queue;
    float phase = 15.95f;
    queue.setPhaseReader(readPhase, &phase);

    queue.beginMidiRenderBlock(
        1,
        512,
        15.95f,
        120.0f,
        22050.0f,
        true,
        true,
        true);

    GvepEvent event = popVisual();
    assert(event.type == GvepEventType::Play);
    assert(event.bar == 0);
    assert(event.step == 0);
    assert(event.musicalTick == 0);

    phase = 0.0f;
    const bool kickQueued = queue.tryPush(MusicalEvent{
        MusicalEventType::NoteOn,
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::Drums,
        0,
        60,
        111,
    });
    assert(kickQueued);

    event = popVisual();
    assert(event.type == GvepEventType::Kick);
    assert(event.value == 111);
    assert(event.bar == 0);
    assert(event.step == 0);
    assert(event.musicalTick == 0);

    queue.endMidiRenderBlock();

    phase = 15.75f;
    queue.beginMidiRenderBlock(
        2,
        512,
        15.75f,
        120.0f,
        22050.0f,
        true,
        true,
        true);

    phase = 0.10f;
    const bool wrappedKickQueued = queue.tryPush(MusicalEvent{
        MusicalEventType::NoteOn,
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::Drums,
        0,
        60,
        90,
    });
    assert(wrappedKickQueued);

    event = popVisual();
    assert(event.type == GvepEventType::Kick);
    assert(event.bar == 1);
    assert(event.step == 0);
    assert(event.musicalTick >= kGvepTicksPerBar);
    assert(event.musicalTick < kGvepTicksPerBar + 24u);

    queue.endMidiRenderBlock();

    queue.beginMidiRenderBlock(
        3,
        512,
        0.20f,
        120.0f,
        22050.0f,
        true,
        true,
        true);
    queue.endMidiRenderBlock();

    queue.beginMidiRenderBlock(
        4,
        512,
        0.20f,
        120.0f,
        22050.0f,
        false,
        true,
        true);

    event = popVisual();
    assert(event.type == GvepEventType::Stop);
    assert(event.bar == 1);
    assert(event.musicalTick >= kGvepTicksPerBar);

    GvepEvent unexpected{};
    assert(!gvepR0EventBus().tryPop(unexpected));

    return 0;
}
