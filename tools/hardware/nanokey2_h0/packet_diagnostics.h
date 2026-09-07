#pragma once
#include <cstddef>
#include <cstdint>

// Probe-only counters, not the production MIDI parser.
struct PacketDiagnostics {
    uint32_t transfers{0}, bytes{0}, packets{0}, empty{0}, invalid{0};
    uint32_t noteOns{0}, noteOffs{0};
    uint8_t lastNonzero[4]{}, lastNote[4]{};
    bool haveNonzero{false}, haveNote{false};

    void observe(const uint8_t* data, size_t size) {
        ++transfers;
        bytes += static_cast<uint32_t>(size);
        if (size % 4 != 0) ++invalid;
        for (size_t i = 0; i + 4 <= size; i += 4) {
            const uint8_t* p = data + i;
            ++packets;
            if ((p[0] | p[1] | p[2] | p[3]) == 0) { ++empty; continue; }
            for (unsigned j = 0; j < 4; ++j) lastNonzero[j] = p[j];
            haveNonzero = true;
            const uint8_t cin = p[0] & 15;
            const uint8_t status = p[1] & 0xf0;
            if ((cin == 8 || cin == 9) && status == (cin << 4) &&
                p[2] < 128 && p[3] < 128) {
                for (unsigned j = 0; j < 4; ++j) lastNote[j] = p[j];
                haveNote = true;
                if (cin == 8 || p[3] == 0) ++noteOffs;
                else ++noteOns;
            } else if (cin < 2 || ((cin == 8 || cin == 9))) {
                ++invalid;
            }
        }
    }
};
