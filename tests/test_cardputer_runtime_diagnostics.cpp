#include "src/platform/cardputer_runtime_diagnostics.h"

#include <cassert>
#include <iostream>

using namespace CardputerRuntimeDiagnostics;

int main() {
    // Record size must fit within the RTC / noinit 256-byte budget.
    static_assert(sizeof(Record) <= 256, "Record exceeded 256 bytes");
    static_assert(kRecordVersion == 2, "kRecordVersion must be 2");

    Record record{};
    const uint32_t buildSig = 0x12345678u;
    initializeRecord(record, buildSig, 1);

    assert(record.magic == kRecordMagic);
    assert(record.version == 2);
    assert(record.buildSignature == buildSig);
    assert(record.bootSequence == 1);
    assert(record.checkpointSequence == 0);
    assert(recordIsComplete(record));
    assert(recordMatches(record, buildSig, true));
    assert(!recordMatches(record, buildSig, false));
    assert(!recordMatches(record, buildSig + 1, true));

    // Verify snapshot fields are cleared
    assert(record.snapshot.sequence == 0);
    assert(record.snapshot.freeInternal8 == 0);
    assert(record.snapshot.minFreeInternal8 == 0);
    assert(record.snapshot.largestInternal8 == 0);
    assert(record.snapshot.freeInternalDefault == 0);
    assert(record.snapshot.minFreeInternalDefault == 0);
    assert(record.snapshot.largestInternalDefault == 0);
    assert(record.snapshot.freeInternalDma == 0);
    assert(record.snapshot.minFreeInternalDma == 0);
    assert(record.snapshot.largestInternalDma == 0);

    for (uint8_t i = 0; i < static_cast<uint8_t>(Task::Count); ++i) {
        assert(record.snapshot.stackFreeBytes[i] == 0);
        assert(record.snapshot.stackHighWaterBytes[i] == 0);
    }

    // Verify all 8 new phases can be recorded
    const Phase newPhases[] = {
        Phase::AfterM5Init,
        Phase::AfterDisplayInit,
        Phase::AfterAudioTaskStart,
        Phase::AfterDspDelaysInit,
        Phase::AfterSdMount,
        Phase::AfterSmfBegin,
        Phase::AfterSceneLoad,
        Phase::AfterFirstUiFrame,
    };

    for (Phase p : newPhases) {
        recordCheckpoint(record, Task::Loop, p, 1);
        const Checkpoint& cp = record.checkpoints[taskIndex(Task::Loop)];
        assert(cp.phase == static_cast<uint8_t>(p));
        assert(cp.core == 1);
    }
    assert(record.checkpointSequence == 8);

    // Verify failure recording
    bool fail1 = recordFirstAllocationFailure(record, 0, Task::Audio, 1024, 0x10, 0x40001000);
    assert(fail1);
    assert(record.failures[0].requestedBytes == 1024);
    assert(record.failures[0].task == static_cast<uint8_t>(Task::Audio));

    // Second failure on same core is ignored
    bool fail2 = recordFirstAllocationFailure(record, 0, Task::Loop, 2048, 0x20, 0x40002000);
    assert(!fail2);
    assert(record.failures[0].requestedBytes == 1024);

    std::cout << "test_cardputer_runtime_diagnostics: PASS (sizeof(Record)=" << sizeof(Record) << ")\n";
    return 0;
}
