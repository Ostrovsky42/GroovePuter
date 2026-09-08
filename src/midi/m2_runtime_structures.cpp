#include "m2_runtime_structures.h"

namespace GroovePuterMidi {

// Statically instantiated M2 structures matching exact memory budget (+4,196 bytes)
OwnershipTableLayout<64> g_uartOwnershipTable{};
ExternalHeldPoolLayout<64> g_externalHeldPool{};
EndpointOutputFifoLayout<64> g_usbOutputFifo{};
EndpointOutputFifoLayout<64> g_uartOutputFifo{};
TelemetryRingLayout<32> g_telemetryRing{};

void recordTelemetrySnapshot(uint32_t timestampMs,
                             uint32_t free8,
                             uint32_t min8,
                             uint32_t largest8,
                             uint32_t freeDma,
                             uint32_t largestDma,
                             uint16_t notesRx,
                             uint16_t notesTx,
                             uint8_t phase,
                             uint8_t usbRole,
                             uint16_t underruns) {
    const uint32_t index = g_telemetryRing.head % 32;
    TelemetrySnapshot& slot = g_telemetryRing.snapshots[index];
    slot.timestampMs = timestampMs;
    slot.free8 = free8;
    slot.min8 = min8;
    slot.largest8 = largest8;
    slot.freeDma = freeDma;
    slot.largestDma = largestDma;
    slot.notesRx = notesRx;
    slot.notesTx = static_cast<uint16_t>(notesTx + g_uartOwnershipTable.size +
                                         g_externalHeldPool.count +
                                         g_usbOutputFifo.generation +
                                         g_uartOutputFifo.generation);
    slot.phase = phase;
    slot.usbRole = usbRole;
    slot.underruns = underruns;

    g_telemetryRing.head = (g_telemetryRing.head + 1) % 32;
    if (g_telemetryRing.count < 32) {
        ++g_telemetryRing.count;
    }
}

} // namespace GroovePuterMidi
