#include "cardputer_sd.h"

#if defined(ARDUINO)

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>

#include "cardputer_adv_hardware.h"

namespace GroovePuterPlatform {

bool cardputerSdMounted() {
    return SD.cardType() != CARD_NONE;
}

bool ensureCardputerSdMounted() {
    if (cardputerSdMounted()) return true;

    const size_t freeBefore =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largestBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    Serial.printf("[SD] mount begin freeInt=%u largest=%u\n",
                  static_cast<unsigned>(freeBefore),
                  static_cast<unsigned>(largestBefore));

    SPI.begin(GroovePuterHardware::kSdClockPin,
              GroovePuterHardware::kSdMisoPin,
              GroovePuterHardware::kSdMosiPin,
              GroovePuterHardware::kSdChipSelectPin);
    const bool began = SD.begin(GroovePuterHardware::kSdChipSelectPin,
                                SPI,
                                GroovePuterHardware::kSdFrequencyHz);
    const bool mounted = began && cardputerSdMounted();

    const size_t freeAfter =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largestAfter =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    Serial.printf("[SD] mount result=%d type=%d freeInt=%u largest=%u\n",
                  static_cast<int>(mounted),
                  static_cast<int>(SD.cardType()),
                  static_cast<unsigned>(freeAfter),
                  static_cast<unsigned>(largestAfter));
    return mounted;
}

}  // namespace GroovePuterPlatform

#endif
