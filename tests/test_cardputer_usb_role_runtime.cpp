#include "src/platform/cardputer_usb_role_runtime.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    CardputerUsbRoleRuntime::init();

    // Default role is Device
    assert(CardputerUsbRoleRuntime::activeRole() == UsbBootRole::Device);
    assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Device);
    assert(std::strcmp(usbBootRoleToString(UsbBootRole::Device), "Device") == 0);
    assert(std::strcmp(usbBootRoleToString(UsbBootRole::Host), "Host") == 0);
    assert(std::strcmp(usbBootRoleToString(UsbBootRole::Off), "Off") == 0);

    // Setting pending role updates pending without changing active
    assert(CardputerUsbRoleRuntime::setPendingRole(UsbBootRole::Host));
    assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Host);
    assert(CardputerUsbRoleRuntime::activeRole() == UsbBootRole::Device);

    assert(CardputerUsbRoleRuntime::setPendingRole(UsbBootRole::Off));
    assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Off);
    assert(CardputerUsbRoleRuntime::activeRole() == UsbBootRole::Device);

    std::cout << "test_cardputer_usb_role_runtime: PASS\n";
    return 0;
}
