#include <Arduino.h>

#include <usb/usb_host.h>

namespace {

volatile bool g_hostInstalled = false;

void hostEvents(void*) {
    while (true) {
        uint32_t flags = 0;
        const esp_err_t result = usb_host_lib_handle_events(
            pdMS_TO_TICKS(100), &flags);
        if (result != ESP_OK) {
            Serial.printf("[NANOKEY2-H0] host-events=%s\n", esp_err_to_name(result));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("[NANOKEY2-H0] boot");

    const usb_host_config_t config = {
        .skip_phy_setup = false,
        .root_port_unpowered = false,
        .intr_flags = 0,
    };
    const esp_err_t result = usb_host_install(&config);
    Serial.printf("[NANOKEY2-H0] usb_host_install=%s\n", esp_err_to_name(result));
    if (result != ESP_OK) return;

    g_hostInstalled = true;
    const BaseType_t task = xTaskCreate(
        hostEvents, "NanoKeyHost", 4096, nullptr, 20, nullptr);
    Serial.printf("[NANOKEY2-H0] host-task=%d\n", static_cast<int>(task));
}

void loop() {
    delay(1000);
}
