#include "cardputer_usb_midi_transport.h"

#include "src/midi/usb_midi_output.h"

#if ARDUINO_USB_MODE
#error "USB MIDI requires Cardputer USBMode=default (USB-OTG/TinyUSB)"
#endif

namespace {
constexpr uint8_t kCinNoteOff = 0x08;
constexpr uint8_t kCinNoteOn = 0x09;
constexpr uint8_t kStatusNoteOff = 0x80;
constexpr uint8_t kStatusNoteOn = 0x90;

// Construction of USBMIDI registers its interface descriptor before Arduino
// starts the TinyUSB composite. No USB methods or application state are touched
// during global initialization.
CardputerUsbMidiTransport g_transport;
UsbMidiOutput g_output(
    g_transport,
    UsbMidiRouteConfig{
        7,     // live Synth A -> MIDI channel 8
        8,     // live Synth B -> MIDI channel 9
        9,     // live Drums   -> MIDI channel 10
        7,     // PatternPlayer Synth A -> MIDI channel 8
        8,     // PatternPlayer Synth B -> MIDI channel 9
        true,
        true,
    });
bool g_registered = false;
}

uint8_t CardputerUsbMidiTransport::clamp7Bit(uint8_t value) {
    return value > 127 ? 127 : value;
}

uint8_t CardputerUsbMidiTransport::clampChannel(uint8_t channel) {
    return channel > 15 ? 15 : channel;
}

bool CardputerUsbMidiTransport::begin() {
    // The USBMIDI member constructor has already registered the MIDI interface.
    // With CDCOnBoot enabled, Arduino app_main() starts the complete TinyUSB
    // composite before setup(). USBMIDI::begin() is a no-op in the pinned core,
    // so registration remains safe during static GroovePuter sink bootstrap.
    midi_.begin();
    begun_ = true;
    return true;
}

bool CardputerUsbMidiTransport::mounted() const {
    return begun_ && static_cast<bool>(USB);
}

bool CardputerUsbMidiTransport::writeChannelPacket(uint8_t codeIndex,
                                                    uint8_t statusBase,
                                                    uint8_t zeroBasedChannel,
                                                    uint8_t note,
                                                    uint8_t velocity) {
    if (!mounted()) return false;

    midiEventPacket_t packet{
        codeIndex,
        static_cast<uint8_t>(statusBase | clampChannel(zeroBasedChannel)),
        clamp7Bit(note),
        clamp7Bit(velocity),
    };
    return midi_.writePacket(&packet);
}

bool CardputerUsbMidiTransport::sendNoteOn(uint8_t zeroBasedChannel,
                                           uint8_t note,
                                           uint8_t velocity) {
    return writeChannelPacket(kCinNoteOn,
                              kStatusNoteOn,
                              zeroBasedChannel,
                              note,
                              velocity);
}

bool CardputerUsbMidiTransport::sendNoteOff(uint8_t zeroBasedChannel,
                                            uint8_t note,
                                            uint8_t velocity) {
    return writeChannelPacket(kCinNoteOff,
                              kStatusNoteOff,
                              zeroBasedChannel,
                              note,
                              velocity);
}

void CardputerUsbMidiTransport::flush() {
    // USBMIDI::writePacket() queues a complete four-byte USB-MIDI event packet.
    // The pinned TinyUSB API exposes no additional flush operation.
}

void registerCardputerUsbMidiSink(MusicalEventRouter& router) {
    if (g_registered) return;
    if (!g_output.begin()) return;
    g_registered = router.addSink(g_output);
}
