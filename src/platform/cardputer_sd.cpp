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

#if defined(GROOVEPUTER_SD_SLOT_CENSUS)
// Diagnostic only. Measures what one FatFs file slot actually costs, by
// mounting the same card at max_files 5..1 and reporting each delta. Runs
// once, before anything else has fragmented the heap, and leaves the card
// unmounted so the normal path below performs the real mount. No production
// call site and no semantic change: SD.begin()'s later default-argument call
// is unchanged.
namespace {
uint32_t censusFree() {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
uint32_t censusLargest() {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
}  // namespace

void runCardputerSdSlotCensus() {
    static bool done = false;
    if (done) return;
    done = true;
    for (int slots = 5; slots >= 1; --slots) {
        SD.end();
        const uint32_t f0 = censusFree();
        const uint32_t l0 = censusLargest();
        const bool ok = SD.begin(GroovePuterHardware::kSdChipSelectPin,
                                 SPI,
                                 GroovePuterHardware::kSdFrequencyHz,
                                 "/sd",
                                 (uint8_t)slots,
                                 false);
        const uint32_t f1 = censusFree();
        const uint32_t l1 = censusLargest();
        Serial.printf("[SDCENSUS] max_files=%d ok=%d type=%d "
                      "free=%u->%u delta=%ld largest=%u->%u largestDelta=%ld\n",
                      slots, (int)ok, (int)SD.cardType(),
                      (unsigned)f0, (unsigned)f1, (long)f1 - (long)f0,
                      (unsigned)l0, (unsigned)l1, (long)l1 - (long)l0);
        delay(20);
    }
    SD.end();
    Serial.printf("[SDCENSUS] done free=%u largest=%u\n",
                  (unsigned)censusFree(), (unsigned)censusLargest());
}
#endif

bool ensureCardputerSdMounted() {
#if defined(GROOVEPUTER_SD_SLOT_CENSUS)
    runCardputerSdSlotCensus();
#endif
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
