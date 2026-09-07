#include "tools/hardware/nanokey2_h0/packet_diagnostics.h"
#include <cassert>

int main() {
    PacketDiagnostics d;
    const uint8_t padded[] = {9, 0x90, 60, 100, 0, 0, 0, 0};
    d.observe(padded, sizeof padded);
    assert(d.noteOns == 1 && d.empty == 1 && d.packets == 2);
    assert(d.haveNote && d.lastNote[2] == 60 && d.lastNote[3] == 100);
    const uint8_t zero[] = {0,0,0,0};
    d.observe(zero, sizeof zero);
    assert(d.lastNonzero[3] == 100 && d.lastNote[3] == 100);
    const uint8_t off[] = {9,0x90,60,0, 8,0x80,61,64};
    d.observe(off, sizeof off);
    assert(d.noteOffs == 2 && d.noteOns == 1);
    const uint8_t malformed[] = {9,0x80,60,64, 9,0x90,128,64, 1};
    d.observe(malformed, sizeof malformed);
    assert(d.invalid == 3 && d.noteOffs == 2 && d.noteOns == 1);
    assert(d.lastNote[1] == 0x80 && d.lastNote[2] == 61);
    PacketDiagnostics onlyEmpty;
    onlyEmpty.observe(zero, sizeof zero);
    assert(!onlyEmpty.haveNote && !onlyEmpty.haveNonzero);
}
