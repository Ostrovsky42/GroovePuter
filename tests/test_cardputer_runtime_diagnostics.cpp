#include <cassert>
#include <type_traits>

#include "src/platform/cardputer_runtime_diagnostics.h"

using namespace CardputerRuntimeDiagnostics;

int main() {
    static_assert(std::is_trivial<Record>::value,
                  "RTC diagnostic record must have no constructor");
    static_assert(sizeof(Record) <= 256,
                  "runtime diagnostic RTC record exceeded its budget");

    Record record{};
    assert(!recordIsComplete(record));
    initializeRecord(record, 0x12345678u, 4);
    assert(recordIsComplete(record));
    assert(recordMatches(record, 0x12345678u, true));
    assert(!recordMatches(record, 0x12345678u, false));
    assert(!recordMatches(record, 0x87654321u, true));

    // A torn RTC publication and a power-on reset are both rejected before
    // task phases are interpreted as prior-boot evidence.
    record.magic = 0;
    assert(!recordMatches(record, 0x12345678u, true));
    initializeRecord(record, 0x12345678u, 4);

    recordCheckpoint(record, Task::Loop, Phase::Keyboard, 1);
    recordCheckpoint(record, Task::Audio, Phase::AudioRender, 0);
    assert(record.checkpoints[taskIndex(Task::Loop)].phase ==
           static_cast<uint8_t>(Phase::Keyboard));
    assert(record.checkpoints[taskIndex(Task::Audio)].phase ==
           static_cast<uint8_t>(Phase::AudioRender));

    assert(recordFirstAllocationFailure(record, 0, Task::Midi, 2048, 3,
                                        0x1234));
    assert(!recordFirstAllocationFailure(record, 0, Task::Midi, 4096, 7,
                                         0x4567));
    assert(record.failures[0].requestedBytes == 2048);
    assert(record.failures[0].state == 2);
    assert(recordFirstAllocationFailure(record, 1, Task::Audio, 512, 9,
                                        0x6789));
    assert(record.failures[1].task == static_cast<uint8_t>(Task::Audio));

    record.buildSignature ^= 1;
    assert(!recordIsComplete(record));
}
