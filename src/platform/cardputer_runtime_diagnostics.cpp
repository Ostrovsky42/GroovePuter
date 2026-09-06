#include "cardputer_runtime_diagnostics.h"

#if defined(GROOVEPUTER_RUNTIME_DIAGNOSTICS) && defined(ARDUINO)

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace CardputerRuntimeDiagnostics {
namespace {

static_assert(sizeof(Record) <= 256,
              "runtime diagnostic RTC record exceeded its budget");

RTC_NOINIT_ATTR Record g_record;
TaskHandle_t g_taskHandles[static_cast<uint8_t>(Task::Count)]{};

uint8_t currentCore() {
    const BaseType_t core = xPortGetCoreID();
    return core >= 0 && core < 2 ? static_cast<uint8_t>(core) : 0;
}

const char* taskName(Task task) {
    switch (task) {
        case Task::Loop: return "loop";
        case Task::Audio: return "audio";
        case Task::Smf: return "smf";
        case Task::Midi: return "midi";
        case Task::Other: return "other";
        case Task::Count: break;
    }
    return "invalid";
}

const char* phaseName(uint8_t phase) {
    switch (static_cast<Phase>(phase)) {
        case Phase::None: return "none";
        case Phase::Enter: return "enter";
        case Phase::Keyboard: return "keyboard";
        case Phase::Control: return "control";
        case Phase::Ui: return "ui";
        case Phase::AudioRender: return "audio-render";
        case Phase::AudioWrite: return "audio-write";
        case Phase::SmfService: return "smf-service";
        case Phase::MidiService: return "midi-service";
        case Phase::BeforeSpi: return "before-spi";
        case Phase::AfterSpi: return "after-spi";
        case Phase::BeforeSd: return "before-sd";
        case Phase::AfterSd: return "after-sd";
        case Phase::BeforeSdReadyHook: return "before-sd-hook";
        case Phase::AfterSdReadyHook: return "after-sd-hook";
        case Phase::Idle: return "idle";
    }
    return "invalid";
}

Task currentTaskTag() {
    // SMF and MIDI share CPU0. A last-checkpoint-per-core tag would therefore
    // misidentify an allocation in the other task. Match the actual FreeRTOS
    // task handle instead; unknown contexts are deliberately reported as
    // Other rather than attributed to a stale checkpoint.
    const TaskHandle_t current = xTaskGetCurrentTaskHandle();
    for (uint8_t index = 0; index < static_cast<uint8_t>(Task::Count); ++index) {
        if (g_taskHandles[index] == current) return static_cast<Task>(index);
    }
    return Task::Other;
}

void printRecord(const char* prefix, const Record& record) {
    Serial.printf("[%s] boot=%u checkpoints=%u memory=%u/%u integrity=%u/%uus\n",
                  prefix,
                  static_cast<unsigned>(record.bootSequence),
                  static_cast<unsigned>(record.checkpointSequence),
                  static_cast<unsigned>(record.snapshot.freeInternal8),
                  static_cast<unsigned>(record.snapshot.largestInternal8),
                  static_cast<unsigned>(record.snapshot.integrityOk),
                  static_cast<unsigned>(record.snapshot.integrityDurationUs));
    for (uint8_t index = 0; index < static_cast<uint8_t>(Task::Count); ++index) {
        const Checkpoint& checkpoint = record.checkpoints[index];
        Serial.printf("[%s-TASK] name=%s seq=%u phase=%s core=%u stackFree=%u\n",
                      prefix, taskName(static_cast<Task>(index)),
                      static_cast<unsigned>(checkpoint.sequence),
                      phaseName(checkpoint.phase),
                      static_cast<unsigned>(checkpoint.core),
                      static_cast<unsigned>(record.snapshot.stackFreeBytes[index]));
    }
    for (const AllocationFailure& failure : record.failures) {
        if (__atomic_load_n(&failure.state, __ATOMIC_ACQUIRE) != 2) continue;
        Serial.printf("[%s-ALLOC] bytes=%u caps=0x%08x task=%s core=%u fn=0x%08x\n",
                      prefix, static_cast<unsigned>(failure.requestedBytes),
                      static_cast<unsigned>(failure.capabilities),
                      taskName(static_cast<Task>(failure.task)),
                      static_cast<unsigned>(failure.core),
                      static_cast<unsigned>(failure.functionAddress));
    }
}

void IRAM_ATTR onAllocationFailed(size_t requestedBytes, uint32_t capabilities,
                                  const char* functionName) {
    const uint8_t core = currentCore();
    AllocationFailure& destination = g_record.failures[core];
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&destination.state, &expected, 1, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        return;
    }
    destination.requestedBytes = static_cast<uint32_t>(requestedBytes);
    destination.capabilities = capabilities;
    destination.functionAddress = reinterpret_cast<uintptr_t>(functionName);
    destination.task = static_cast<uint8_t>(currentTaskTag());
    destination.core = core;
    __atomic_store_n(&destination.state, 2, __ATOMIC_RELEASE);
}

}  // namespace

void begin(uint32_t buildSignature, bool retainedReset) {
    const bool previousIsUs = recordMatches(g_record, buildSignature, retainedReset);
    const uint32_t nextBoot = previousIsUs ? g_record.bootSequence + 1 : 1;
    if (previousIsUs) printRecord("RDIAG-PREV", g_record);
    initializeRecord(g_record, buildSignature, nextBoot);
    for (TaskHandle_t& handle : g_taskHandles) handle = nullptr;
    const esp_err_t result = heap_caps_register_failed_alloc_callback(
        &onAllocationFailed);
    Serial.printf("[RDIAG] boot=%u allocHook=%d\n",
                  static_cast<unsigned>(nextBoot), static_cast<int>(result));
}

void registerCurrentTask(Task task) {
    g_taskHandles[taskIndex(task)] = xTaskGetCurrentTaskHandle();
    checkpoint(task, Phase::Enter);
}

void checkpoint(Task task, Phase phase) {
    const uint8_t core = currentCore();
    const uint32_t sequence = __atomic_add_fetch(&g_record.checkpointSequence,
                                                   1, __ATOMIC_RELAXED);
    Checkpoint& destination = g_record.checkpoints[taskIndex(task)];
    destination.sequence = sequence;
    destination.phase = static_cast<uint8_t>(phase);
    destination.core = core;
}

void sampleFromControlTask() {
    MemorySnapshot& snapshot = g_record.snapshot;
    snapshot.sequence += 1;
    snapshot.freeInternal8 = heap_caps_get_free_size(
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snapshot.largestInternal8 = heap_caps_get_largest_free_block(
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const uint32_t startedAt = micros();
    snapshot.integrityOk = heap_caps_check_integrity_all(false) ? 1 : 0;
    snapshot.integrityDurationUs = micros() - startedAt;
    for (uint8_t index = 0; index < static_cast<uint8_t>(Task::Count); ++index) {
        const TaskHandle_t handle = g_taskHandles[index];
        snapshot.stackFreeBytes[index] = handle == nullptr ? 0 :
            static_cast<uint32_t>(uxTaskGetStackHighWaterMark(handle)) *
                sizeof(StackType_t);
    }
}

void reportFromControlTask() {
    printRecord("RDIAG", g_record);
}

}  // namespace CardputerRuntimeDiagnostics

#endif  // GROOVEPUTER_RUNTIME_DIAGNOSTICS && ARDUINO
