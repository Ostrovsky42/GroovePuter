#pragma once

#include <cstdint>
#include <cstddef>

namespace GroovePuterMidi {

using UsbHostMidiCallback = void (*)(const uint8_t packet[4]);

// Same-boot internal-heap snapshots taken around the Host bring-up (diagnostic; bytes).
struct UsbHostMemDiag {
    uint32_t freeBefore = 0, largestBefore = 0;          // before usb_host_install
    uint32_t freeInstalled = 0, largestInstalled = 0;    // after usb_host_install
    uint32_t freeClient = 0, largestClient = 0;          // after usb_host_client_register
    uint32_t freeDevice = 0, largestDevice = 0;          // after the first device was claimed
    uint32_t freePacket = 0, largestPacket = 0;          // at the first MIDI packet
    uint32_t minEverFree = 0;
    bool installed = false, client = false, device = false, packet = false;
};

class CardputerUsbHostMidi {
public:
    static bool begin(UsbHostMidiCallback callback);
    static void service();
    static bool isConnected();
    static uint16_t vid();
    static uint16_t pid();
    static uint32_t packetCount();
    static uint32_t noteOnCount();
    static uint32_t noteOffCount();
    static uint8_t lastNote();
    static uint8_t lastVelocity();
    static const char* status();
    static void stop();
    static const UsbHostMemDiag& memDiag();
};

} // namespace GroovePuterMidi
