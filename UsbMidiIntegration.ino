// Arduino concatenates GroovePuter.ino before this secondary sketch tab, so
// MusicalEventRouter and g_musicalEventRouter are already declared here.
// Keep TinyUSB headers out of the main UI translation unit: TinyUSB's HID mouse
// constants collide with GroovePuter's desktop UI mouse enum names.
void registerCardputerUsbMidiSink(MusicalEventRouter& router);

namespace {
struct UsbMidiSinkRegistration {
    UsbMidiSinkRegistration() {
        // This runs before Arduino app_main() starts the CDC-on-boot TinyUSB
        // composite. The platform function constructs USBMIDI early enough to
        // register its interface, then adds UsbMidiOutput as an independent sink.
        registerCardputerUsbMidiSink(g_musicalEventRouter);
    }
};

UsbMidiSinkRegistration g_usbMidiSinkRegistration;
}  // namespace
