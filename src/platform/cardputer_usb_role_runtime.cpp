#include "cardputer_usb_role_runtime.h"

#if defined(ARDUINO) || defined(ESP_PLATFORM)
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>
#endif

namespace {
constexpr const char* kNvsNamespace = "usb_role";
constexpr const char* kNvsKeyRole = "boot_role";
}  // namespace

#if defined(GROOVEPUTER_DEFAULT_USB_ROLE_HOST)
UsbBootRole CardputerUsbRoleRuntime::activeRole_{UsbBootRole::Host};
UsbBootRole CardputerUsbRoleRuntime::pendingRole_{UsbBootRole::Host};
#else
UsbBootRole CardputerUsbRoleRuntime::activeRole_{UsbBootRole::Device};
UsbBootRole CardputerUsbRoleRuntime::pendingRole_{UsbBootRole::Device};
#endif
bool CardputerUsbRoleRuntime::initialized_{false};

const char* usbBootRoleToString(UsbBootRole role) {
    switch (role) {
        case UsbBootRole::Off: return "Off";
        case UsbBootRole::Device: return "Device";
        case UsbBootRole::Host: return "Host";
        default: return "Unknown";
    }
}

void CardputerUsbRoleRuntime::init() {
    if (initialized_) return;
    initialized_ = true;

#if defined(ARDUINO) || defined(ESP_PLATFORM)
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint8_t val = static_cast<uint8_t>(activeRole_);
        if (nvs_get_u8(handle, kNvsKeyRole, &val) == ESP_OK) {
            if (val <= static_cast<uint8_t>(UsbBootRole::Host)) {
                activeRole_ = static_cast<UsbBootRole>(val);
            }
        }
        nvs_close(handle);
    }
#endif
    pendingRole_ = activeRole_;
}

UsbBootRole CardputerUsbRoleRuntime::activeRole() {
    if (!initialized_) init();
    return activeRole_;
}

UsbBootRole CardputerUsbRoleRuntime::pendingRole() {
    if (!initialized_) init();
    return pendingRole_;
}

bool CardputerUsbRoleRuntime::setPendingRole(UsbBootRole role) {
    if (!initialized_) init();
    pendingRole_ = role;

#if defined(ARDUINO) || defined(ESP_PLATFORM)
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;
    err = nvs_set_u8(handle, kNvsKeyRole, static_cast<uint8_t>(role));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return (err == ESP_OK);
#else
    return true;
#endif
}

bool CardputerUsbRoleRuntime::requestRebootWithRole(UsbBootRole newRole) {
    if (!setPendingRole(newRole)) return false;
#if defined(ARDUINO) || defined(ESP_PLATFORM)
    esp_restart();
#endif
    return true;
}
