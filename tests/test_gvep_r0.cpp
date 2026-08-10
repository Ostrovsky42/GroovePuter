#include <cassert>
#include <cstdint>

#include "src/visual/gvep_r0.h"

int main() {
    using namespace GroovePuterVisual;

    static_assert(kGvepV1PacketSize == 24, "GVEP v1 packet size changed");
    static_assert(static_cast<uint8_t>(GvepEventType::Kick) == 0x01,
                  "GVEP KICK ID changed");
    static_assert(static_cast<uint8_t>(GvepEventType::Play) == 0x20,
                  "GVEP PLAY ID changed");
    static_assert(static_cast<uint8_t>(GvepEventType::Stop) == 0x21,
                  "GVEP STOP ID changed");

    GvepEvent event{};
    event.type = GvepEventType::Kick;
    event.value = 127;
    event.sequence = 0x12345678u;
    event.musicalTick = 0x01020304u;
    event.bar = 0x1122u;
    event.step = 15;

    uint8_t packet[kGvepV1PacketSize]{};
    serializeGvepV1Event(event, 0xA1B2C3D4u, packet);

    assert(packet[0] == 'G');
    assert(packet[1] == 'V');
    assert(packet[2] == 'E');
    assert(packet[3] == '1');
    assert(packet[4] == 1);
    assert(packet[5] == 1);
    assert(packet[6] == 0x01);
    assert(packet[7] == 0x00);

    assert(packet[8] == 0x78);
    assert(packet[9] == 0x56);
    assert(packet[10] == 0x34);
    assert(packet[11] == 0x12);

    assert(packet[12] == 0x04);
    assert(packet[13] == 0x03);
    assert(packet[14] == 0x02);
    assert(packet[15] == 0x01);

    assert(packet[16] == 0xD4);
    assert(packet[17] == 0xC3);
    assert(packet[18] == 0xB2);
    assert(packet[19] == 0xA1);

    assert(packet[20] == 0x22);
    assert(packet[21] == 0x11);
    assert(packet[22] == 15);
    assert(packet[23] == 127);

    return 0;
}
