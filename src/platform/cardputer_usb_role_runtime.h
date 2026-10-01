#pragma once

#include <cstdint>

// Cardputer ADV single-binary USB boot role.
// The ESP32-S3 has a single USB-OTG controller (pins 19/20).
// Host and Device roles are mutually exclusive and selected at boot.
enum class UsbBootRole : uint8_t {
    Off = 0,
    Device = 1,
    Host = 2
};

const char* usbBootRoleToString(UsbBootRole role);

class CardputerUsbRoleRuntime {
public:
    // Initialize and read persisted role from NVS at boot before USB start.
    static void init();

    // Active role for current boot session (immutable after init).
    static UsbBootRole activeRole();

    // Pending role to be applied on next boot.
    static UsbBootRole pendingRole();

    // Set pending role in NVS. Returns true on successful write.
    static bool setPendingRole(UsbBootRole role);

    // Prepare system, persist new role, and request reboot.
    static bool requestRebootWithRole(UsbBootRole newRole);

private:
    static UsbBootRole activeRole_;
    static UsbBootRole pendingRole_;
    static bool initialized_;
};
