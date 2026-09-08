#pragma once

#include <cstdint>
#include <cstddef>

namespace GroovePuterMidi {

using UsbHostMidiCallback = void (*)(const uint8_t packet[4]);

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
};

} // namespace GroovePuterMidi
