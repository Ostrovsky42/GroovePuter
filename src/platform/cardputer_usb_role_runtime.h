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

    // False in a CDC-on-boot build: the core starts TinyUSB Device before setup(), so the saved role
    // cannot take effect there and the UI must not pretend otherwise.
    static bool selectableInThisBuild();

    // True when the saved role differs from the running one (a restart is needed).
    static bool restartPending();

    // Number of restarts requested through requestRebootWithRole() in this run (tests, diagnostics).
    static uint32_t restartRequests();

    // Set pending role in NVS. Returns true on successful write.
    static bool setPendingRole(UsbBootRole role);

    // Prepare system, persist new role, and request reboot.
    static bool requestRebootWithRole(UsbBootRole newRole);

private:
    static UsbBootRole activeRole_;
    static UsbBootRole pendingRole_;
    static bool initialized_;
    static uint32_t restartRequests_;
};
