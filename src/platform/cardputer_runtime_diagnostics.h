#pragma once

#include <cstddef>
#include <cstdint>

// Runtime-panic telemetry deliberately uses only fixed storage. The record is
// valid across a panic reset, but a power-on reset and another ELF must never
// be allowed to reinterpret stale fields as evidence.
namespace CardputerRuntimeDiagnostics {

enum class Task : uint8_t {
    Loop = 0,
    Audio,
    Smf,
    Midi,
    Other,
    Count,
};

enum class Phase : uint8_t {
    None = 0,
    Enter,
    Keyboard,
    Control,
    Ui,
    AudioRender,
    AudioWrite,
    SmfService,
    MidiService,
    BeforeSpi,
    AfterSpi,
    BeforeSd,
    AfterSd,
    BeforeSdReadyHook,
    AfterSdReadyHook,
    Idle,
};

struct Checkpoint {
    uint32_t sequence;
    uint8_t phase;
    uint8_t core;
    uint16_t reserved;
};

struct AllocationFailure {
    // 0=empty, 1=being written, 2=complete. Publishing state last makes a
    // partially interrupted callback record visibly invalid after reset.
    uint32_t state;
    uint32_t requestedBytes;
    uint32_t capabilities;
    uintptr_t functionAddress;
    uint8_t task;
    uint8_t core;
    uint16_t reserved;
};

struct MemorySnapshot {
    uint32_t sequence;
    uint32_t freeInternal8;
    uint32_t largestInternal8;
    uint32_t integrityDurationUs;
    uint8_t integrityOk;
    uint8_t reserved[3];
    uint32_t stackFreeBytes[static_cast<uint8_t>(Task::Count)];
};

struct Record {
    uint32_t magic;
    uint32_t version;
    uint32_t buildSignature;
    uint32_t bootSequence;
    uint32_t checkpointSequence;
    Checkpoint checkpoints[static_cast<uint8_t>(Task::Count)];
    AllocationFailure failures[2];
    MemorySnapshot snapshot;
    uint32_t complement;
};

constexpr uint32_t kRecordMagic = 0x47505244u;  // GPRD
constexpr uint32_t kRecordVersion = 1;

constexpr uint8_t taskIndex(Task task) {
    const uint8_t index = static_cast<uint8_t>(task);
    return index < static_cast<uint8_t>(Task::Count) ? index :
        static_cast<uint8_t>(Task::Other);
}

constexpr bool recordIsComplete(const Record& record) {
    return record.magic == kRecordMagic &&
        record.version == kRecordVersion &&
        record.complement == ~record.buildSignature;
}

constexpr bool recordMatches(const Record& record, uint32_t buildSignature,
                             bool retainedReset) {
    return retainedReset && recordIsComplete(record) &&
        record.buildSignature == buildSignature;
}

inline void initializeRecord(Record& record, uint32_t buildSignature,
                             uint32_t bootSequence) {
    record.magic = 0;
    record.version = kRecordVersion;
    record.buildSignature = buildSignature;
    record.bootSequence = bootSequence;
    record.checkpointSequence = 0;
    for (uint8_t index = 0; index < static_cast<uint8_t>(Task::Count); ++index) {
        record.checkpoints[index] = Checkpoint{0, static_cast<uint8_t>(Phase::None),
                                                0xFF, 0};
        record.snapshot.stackFreeBytes[index] = 0;
    }
    record.failures[0] = AllocationFailure{0, 0, 0, 0,
                                            static_cast<uint8_t>(Task::Other), 0xFF, 0};
    record.failures[1] = AllocationFailure{0, 0, 0, 0,
                                            static_cast<uint8_t>(Task::Other), 0xFF, 0};
    record.snapshot = MemorySnapshot{};
    record.complement = ~buildSignature;
    record.magic = kRecordMagic;
}

inline void recordCheckpoint(Record& record, Task task, Phase phase,
                             uint8_t core) {
    const uint32_t nextSequence = record.checkpointSequence + 1;
    record.checkpointSequence = nextSequence;
    Checkpoint& destination = record.checkpoints[taskIndex(task)];
    destination.sequence = nextSequence;
    destination.phase = static_cast<uint8_t>(phase);
    destination.core = core;
}

inline bool recordFirstAllocationFailure(Record& record, uint8_t core,
                                         Task task, size_t requestedBytes,
                                         uint32_t capabilities,
                                         uintptr_t functionAddress) {
    const uint8_t index = core < 2 ? core : 0;
    AllocationFailure& destination = record.failures[index];
    if (destination.state != 0) return false;
    destination.state = 1;
    destination.requestedBytes = static_cast<uint32_t>(requestedBytes);
    destination.capabilities = capabilities;
    destination.functionAddress = functionAddress;
    destination.task = static_cast<uint8_t>(task);
    destination.core = core;
    destination.state = 2;
    return true;
}

// Product builds retain no runtime panic telemetry and add no calls to the
// timing-sensitive paths. The diagnostic build supplies the definitions below.
#if defined(GROOVEPUTER_RUNTIME_DIAGNOSTICS)
void begin(uint32_t buildSignature, bool retainedReset);
void registerCurrentTask(Task task);
void checkpoint(Task task, Phase phase);
void sampleFromControlTask();
void reportFromControlTask();
#else
inline void begin(uint32_t, bool) {}
inline void registerCurrentTask(Task) {}
inline void checkpoint(Task, Phase) {}
inline void sampleFromControlTask() {}
inline void reportFromControlTask() {}
#endif

}  // namespace CardputerRuntimeDiagnostics
