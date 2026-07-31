#pragma once

#include <cstdint>

#include "src/midi/usb_midi_transport.h"

#if !defined(ARDUINO)
#error "CardputerUsbMidiTransport is available only in the Arduino firmware build"
#endif

#include "USB.h"
#include "USBMIDI.h"

// Native ESP32-S3 TinyUSB MIDI transport for Cardputer-Adv.
//
// The platform owns one global instance so USBMIDI can register its descriptor
// before Arduino's app_main() starts the TinyUSB CDC composite. begin() and
// router registration are deliberately deferred until setup().
class CardputerUsbMidiTransport final : public IUsbMidiTransport {
public:
    CardputerUsbMidiTransport() = default;

    bool begin() override;
    bool mounted() const override;

    bool sendNoteOn(uint8_t zeroBasedChannel,
                    uint8_t note,
                    uint8_t velocity) override;
    bool sendNoteOff(uint8_t zeroBasedChannel,
                     uint8_t note,
                     uint8_t velocity) override;
    bool sendTimingClock() override;
    bool sendStart() override;
    bool sendStop() override;
    void flush() override;

private:
    static uint8_t clamp7Bit(uint8_t value);
    static uint8_t clampChannel(uint8_t channel);
    bool writeChannelPacket(uint8_t codeIndex,
                            uint8_t statusBase,
                            uint8_t zeroBasedChannel,
                            uint8_t note,
                            uint8_t velocity);
    bool writeRealtimePacket(uint8_t status);

    USBMIDI midi_;
    bool begun_{false};
};
