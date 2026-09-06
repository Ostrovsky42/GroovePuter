#include "cardputer_sd.h"

#if defined(ARDUINO)

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>

#include "cardputer_adv_hardware.h"
#include "cardputer_runtime_diagnostics.h"

namespace GroovePuterPlatform {
namespace {

CardputerSdReadyHook g_sdReadyHook = nullptr;
bool g_sdReadyNotified = false;

void notifySdReadyOnce() {
    if (g_sdReadyNotified || g_sdReadyHook == nullptr) return;
    g_sdReadyNotified = true;
    g_sdReadyHook();
}

}  // namespace

void setCardputerSdReadyHook(CardputerSdReadyHook hook) {
    g_sdReadyHook = hook;
}

bool cardputerSdMounted() {
    return SD.cardType() != CARD_NONE;
}

bool ensureCardputerSdMounted() {
    if (cardputerSdMounted()) {
        CardputerRuntimeDiagnostics::checkpoint(
            CardputerRuntimeDiagnostics::Task::Loop,
            CardputerRuntimeDiagnostics::Phase::BeforeSdReadyHook);
        notifySdReadyOnce();
        CardputerRuntimeDiagnostics::checkpoint(
            CardputerRuntimeDiagnostics::Task::Loop,
            CardputerRuntimeDiagnostics::Phase::AfterSdReadyHook);
        return true;
    }

    const size_t freeBefore =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largestBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    Serial.printf("[SD] mount begin freeInt=%u largest=%u\n",
                  static_cast<unsigned>(freeBefore),
                  static_cast<unsigned>(largestBefore));

    CardputerRuntimeDiagnostics::checkpoint(
        CardputerRuntimeDiagnostics::Task::Loop,
        CardputerRuntimeDiagnostics::Phase::BeforeSpi);
    const bool spiReady = SPI.begin(GroovePuterHardware::kSdClockPin,
              GroovePuterHardware::kSdMisoPin,
              GroovePuterHardware::kSdMosiPin,
              GroovePuterHardware::kSdChipSelectPin);
    CardputerRuntimeDiagnostics::checkpoint(
        CardputerRuntimeDiagnostics::Task::Loop,
        CardputerRuntimeDiagnostics::Phase::AfterSpi);
    if (!spiReady) {
        // Diagnostic only: SDFS::begin() also calls spi.begin() with no
        // arguments, but SPIClass::begin() returns immediately once the bus
        // is already started (`if (_spi) return true;`), so it cannot
        // re-clobber these pins with ESP32 defaults. This early return only
        // distinguishes "our explicit custom-pin init failed" from "SD.begin()
        // itself failed" in the log, one stage earlier than before.
        Serial.printf("[SD] unavailable stage=spi sck=%d miso=%d mosi=%d cs=%d\n",
                      GroovePuterHardware::kSdClockPin,
                      GroovePuterHardware::kSdMisoPin,
                      GroovePuterHardware::kSdMosiPin,
                      GroovePuterHardware::kSdChipSelectPin);
        return false;
    }
    CardputerRuntimeDiagnostics::checkpoint(
        CardputerRuntimeDiagnostics::Task::Loop,
        CardputerRuntimeDiagnostics::Phase::BeforeSd);
    const bool began = SD.begin(GroovePuterHardware::kSdChipSelectPin,
                                SPI,
                                GroovePuterHardware::kSdFrequencyHz);
    CardputerRuntimeDiagnostics::checkpoint(
        CardputerRuntimeDiagnostics::Task::Loop,
        CardputerRuntimeDiagnostics::Phase::AfterSd);
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

    // Arduino clears the card handle on every failed begin, including FAT/VFS
    // failures. CARD_NONE here cannot distinguish those from an absent card.
    if (!mounted) {
        Serial.printf("[SD] unavailable stage=mount began=%d; "
                      "CARD_NONE does not identify the failure cause\n",
                      static_cast<int>(began));
    }
    CardputerRuntimeDiagnostics::sampleFromControlTask();
    if (mounted) {
        CardputerRuntimeDiagnostics::checkpoint(
            CardputerRuntimeDiagnostics::Task::Loop,
            CardputerRuntimeDiagnostics::Phase::BeforeSdReadyHook);
        notifySdReadyOnce();
        CardputerRuntimeDiagnostics::checkpoint(
            CardputerRuntimeDiagnostics::Task::Loop,
            CardputerRuntimeDiagnostics::Phase::AfterSdReadyHook);
    }
    return mounted;
}

}  // namespace GroovePuterPlatform

#endif
