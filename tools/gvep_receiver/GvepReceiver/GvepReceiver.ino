#include <Arduino.h>
#include <WiFi.h>

#include <cstdint>
#include <cstring>

#include "esp_idf_version.h"
#include "esp_now.h"
#include "esp_wifi.h"

namespace {

constexpr uint8_t kEspNowChannel = 6;
constexpr size_t kPacketSize = 24;
constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kMessageEvent = 1;

uint16_t readLe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8u);
}

uint32_t readLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8u) |
         (static_cast<uint32_t>(data[2]) << 16u) |
         (static_cast<uint32_t>(data[3]) << 24u);
}

const char* eventName(uint8_t eventType) {
  switch (eventType) {
    case 0x01: return "KICK";
    case 0x20: return "PLAY";
    case 0x21: return "STOP";
    default: return "UNKNOWN";
  }
}

void handlePacket(const uint8_t* data, int len) {
  if (data == nullptr || len != static_cast<int>(kPacketSize)) {
    return;
  }
  if (data[0] != 'G' || data[1] != 'V' || data[2] != 'E' || data[3] != '1') {
    return;
  }
  if (data[4] != kProtocolVersion || data[5] != kMessageEvent) {
    return;
  }

  const uint8_t eventType = data[6];
  const uint8_t flags = data[7];
  const uint32_t sequence = readLe32(&data[8]);
  const uint32_t musicalTick = readLe32(&data[12]);
  const uint32_t timestampUs = readLe32(&data[16]);
  const uint16_t bar = readLe16(&data[20]);
  const uint8_t step = data[22];
  const uint8_t value = data[23];

  Serial.printf(
      "[GVEP] seq=%u event=%s(0x%02X) value=%u bar=%u step=%u tick=%u ts=%u flags=0x%02X\n",
      static_cast<unsigned>(sequence),
      eventName(eventType),
      static_cast<unsigned>(eventType),
      static_cast<unsigned>(value),
      static_cast<unsigned>(bar),
      static_cast<unsigned>(step),
      static_cast<unsigned>(musicalTick),
      static_cast<unsigned>(timestampUs),
      static_cast<unsigned>(flags));
}

#if ESP_IDF_VERSION_MAJOR >= 5
void onEspNowReceive(const esp_now_recv_info_t*, const uint8_t* data, int len) {
  handlePacket(data, len);
}
#else
void onEspNowReceive(const uint8_t*, const uint8_t* data, int len) {
  handlePacket(data, len);
}
#endif

bool initializeReceiver() {
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println("[GVEP] WiFi STA init failed");
    return false;
  }

  const esp_err_t channelResult =
      esp_wifi_set_channel(kEspNowChannel, WIFI_SECOND_CHAN_NONE);
  if (channelResult != ESP_OK) {
    Serial.printf("[GVEP] channel init failed: %d\n", static_cast<int>(channelResult));
    return false;
  }

  const esp_err_t nowResult = esp_now_init();
  if (nowResult != ESP_OK) {
    Serial.printf("[GVEP] esp_now_init failed: %d\n", static_cast<int>(nowResult));
    return false;
  }

  const esp_err_t callbackResult = esp_now_register_recv_cb(onEspNowReceive);
  if (callbackResult != ESP_OK) {
    Serial.printf("[GVEP] recv callback init failed: %d\n", static_cast<int>(callbackResult));
    esp_now_deinit();
    return false;
  }

  uint8_t mac[6]{};
  if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
    Serial.printf(
        "[GVEP] READY channel=%u rx=%02X:%02X:%02X:%02X:%02X:%02X\n",
        static_cast<unsigned>(kEspNowChannel),
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    Serial.printf("[GVEP] READY channel=%u\n",
                  static_cast<unsigned>(kEspNowChannel));
  }
  return true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nGVEP R0 reference receiver");
  if (!initializeReceiver()) {
    Serial.println("[GVEP] receiver disabled after initialization failure");
  }
}

void loop() {
  delay(1000);
}
