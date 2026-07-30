#include "src/midi/usb_midi_output.h"
#include "src/platform/cardputer_usb_midi_transport.h"

namespace {
// Keep the native MIDI interface in static storage. USBMIDI registers its
// TinyUSB interface from the constructor, before Arduino app_main() starts the
// CDC-on-boot composite device.
CardputerUsbMidiTransport g_cardputerUsbMidiTransport;
UsbMidiOutput g_usbMidiOutput(
    g_cardputerUsbMidiTransport,
    UsbMidiRouteConfig{
        7,     // zero-based channel 7 == MIDI channel 8 / SEQTRAK SYNTH 1
        true,
    });

struct UsbMidiSinkRegistration {
    UsbMidiSinkRegistration() {
        // GroovePuter.ino is concatenated first by the Arduino sketch builder,
        // so the shared router has already been constructed at this point.
        // Registration is memory-only; USB packet writes remain inert until a
        // host has mounted the TinyUSB composite.
        g_usbMidiOutput.begin();
        g_musicalEventRouter.addSink(g_usbMidiOutput);
    }
};

UsbMidiSinkRegistration g_usbMidiSinkRegistration;
}  // namespace
