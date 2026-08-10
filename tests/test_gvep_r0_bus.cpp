#include <cassert>
#include <cstdint>

#include "src/visual/gvep_r0.h"

int main() {
    using namespace GroovePuterVisual;

    GvepR0EventBus bus;

    for (uint32_t i = 0; i < GvepR0EventBus::kCapacity; ++i) {
        const bool accepted = bus.tryPublish(
            GvepEventType::Kick,
            static_cast<uint8_t>(i & 0x7Fu),
            i,
            static_cast<uint16_t>(i / 4u),
            static_cast<uint8_t>(i & 0x0Fu));
        assert(accepted);
    }

    assert(bus.publishedCount() == GvepR0EventBus::kCapacity);
    assert(bus.droppedCount() == 0);
    assert(bus.highWaterMark() == GvepR0EventBus::kCapacity);

    const bool overflowAccepted = bus.tryPublish(
        GvepEventType::Kick,
        127,
        999,
        99,
        15);
    assert(!overflowAccepted);
    assert(bus.publishedCount() == GvepR0EventBus::kCapacity);
    assert(bus.droppedCount() == 1);
    assert(bus.highWaterMark() == GvepR0EventBus::kCapacity);

    GvepEvent event{};
    for (uint32_t i = 0; i < GvepR0EventBus::kCapacity; ++i) {
        assert(bus.tryPop(event));
        assert(event.sequence == i);
    }
    assert(!bus.tryPop(event));
    assert(bus.poppedCount() == GvepR0EventBus::kCapacity);

    // The failed publish consumed sequence kCapacity. The next accepted event
    // must therefore expose a one-sequence gap to a remote receiver.
    assert(bus.tryPublish(GvepEventType::Play, 0, 1000, 100, 0));
    assert(bus.tryPop(event));
    assert(event.type == GvepEventType::Play);
    assert(event.sequence == GvepR0EventBus::kCapacity + 1u);

    return 0;
}
