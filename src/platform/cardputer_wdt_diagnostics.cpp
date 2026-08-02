#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))

#include "cardputer_wdt_diagnostics.h"

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr uint32_t kTaskWdtRecordMagic = 0x47505744u;  // "GPWD"
constexpr std::size_t kTaskNameBytes = 16;

RTC_NOINIT_ATTR uint32_t g_taskWdtRecordMagic;
RTC_NOINIT_ATTR uint32_t g_taskWdtCore;
RTC_NOINIT_ATTR char g_taskWdtName[kTaskNameBytes];

}  // namespace

extern "C" void IRAM_ATTR esp_task_wdt_isr_user_handler(void) {
    const char* taskName = pcTaskGetName(xTaskGetCurrentTaskHandle());
    for (std::size_t i = 0; i < kTaskNameBytes; ++i) {
        const char c = taskName != nullptr ? taskName[i] : '\0';
        g_taskWdtName[i] = c;
        if (c == '\0') {
            for (++i; i < kTaskNameBytes; ++i) g_taskWdtName[i] = '\0';
            break;
        }
    }
    g_taskWdtName[kTaskNameBytes - 1] = '\0';
    g_taskWdtCore = static_cast<uint32_t>(xPortGetCoreID());
    asm volatile("memw" ::: "memory");
    // Publish the magic last so a partially written ISR record is ignored.
    g_taskWdtRecordMagic = kTaskWdtRecordMagic;
}

void printAndClearCardputerWdtDiagnostic() {
    if (g_taskWdtRecordMagic != kTaskWdtRecordMagic) return;

    char taskName[kTaskNameBytes]{};
    std::memcpy(taskName, g_taskWdtName, sizeof(taskName));
    taskName[kTaskNameBytes - 1] = '\0';
    Serial.printf("[WDT-DIAG] task=%s core=%u\n",
                  taskName[0] != '\0' ? taskName : "unknown",
                  static_cast<unsigned>(g_taskWdtCore));
    g_taskWdtRecordMagic = 0;
}

#else

#include "cardputer_wdt_diagnostics.h"

void printAndClearCardputerWdtDiagnostic() {}

#endif
