#include <cstddef>
#include <cstdint>
#include <type_traits>

// Compile-time ABI assertions on ESP32-S3 Xtensa toolchain

namespace GroovePuterMidi {

enum class InputSource : uint8_t { Qwerty = 0, Usb = 1, Uart = 2 };
enum class InputKind : uint8_t { NoteOn = 0, NoteOff = 1, Sustain = 2, AllNotesOff = 3, AllSoundOff = 4 };
enum class ResetReason : uint8_t { Detach, InputDisabled, FilterChanged, Overflow, SourcePanic, TargetChanged };

struct InputSession {
    InputSource source{InputSource::Qwerty};
    uint8_t pad[3]{0, 0, 0};
    uint32_t generation{0};
};
static_assert(sizeof(InputSession) == 8, "InputSession size mismatch");
static_assert(alignof(InputSession) == 4, "InputSession alignment mismatch");

struct InputKey {
    InputSource source{InputSource::Qwerty};
    uint8_t channel{0};
    uint8_t key{0};
    uint8_t pad{0};
    uint32_t generation{0};
};
static_assert(sizeof(InputKey) == 8, "InputKey size mismatch");
static_assert(alignof(InputKey) == 4, "InputKey alignment mismatch");

struct MidiInputEvent {
    InputKey id{};
    uint32_t atMicros{0};
    InputKind kind{InputKind::NoteOff};
    uint8_t velocity{0};
    uint8_t pad[2]{0, 0};
};
static_assert(sizeof(MidiInputEvent) == 16, "MidiInputEvent size mismatch");
static_assert(alignof(MidiInputEvent) == 4, "MidiInputEvent alignment mismatch");

// Ingress SPSC Queue (64 capacity + 1 empty slot)
struct MidiInputQueueLayout {
    static constexpr std::size_t kCapacity = 64;
    static constexpr std::size_t kStorageSize = kCapacity + 1; // 65
    MidiInputEvent events[kStorageSize]; // 65 * 16 = 1040 bytes
    uint32_t head;                       // 4 bytes
    uint32_t tail;                       // 4 bytes
    uint32_t recoveryEpoch;             // 4 bytes
    uint32_t droppedNoteOn;              // 4 bytes
    uint32_t droppedCritical;            // 4 bytes
    uint32_t consumedRecoveryEpoch;      // 4 bytes
};
static_assert(sizeof(MidiInputQueueLayout) == (65 * 16 + 24), "MidiInputQueueLayout size mismatch");
static_assert(sizeof(MidiInputQueueLayout) == 1064, "MidiInputQueueLayout exact size mismatch");

// Sparse Ownership Table (64 cells per endpoint)
struct OwnershipCell {
    uint8_t channel;
    uint8_t note;
    uint8_t wire;
    uint8_t smf;
};
static_assert(sizeof(OwnershipCell) == 4, "OwnershipCell size mismatch");

template <std::size_t Capacity>
struct OwnershipTableLayout {
    OwnershipCell cells[Capacity];
    uint32_t size;
};
static_assert(sizeof(OwnershipTableLayout<64>) == (64 * 4 + 4), "OwnershipTableLayout size mismatch");
static_assert(sizeof(OwnershipTableLayout<64>) == 260, "OwnershipTableLayout exact size mismatch");

// Independent Output FIFO per endpoint (64 events, no 8-slot eviction cache)
enum class MidiEndpointEventType : uint8_t { NoteOn = 0, NoteOff = 1 };
struct OutputFifoEvent {
    uint64_t sequence;                  // 8 bytes (8-byte aligned)
    uint32_t atMicros;                  // 4 bytes
    uint8_t channel;                    // 1 byte
    uint8_t note;                       // 1 byte
    uint8_t velocity;                   // 1 byte
    MidiEndpointEventType type;         // 1 byte
};
static_assert(sizeof(OutputFifoEvent) == 16, "OutputFifoEvent size mismatch");
static_assert(alignof(OutputFifoEvent) == 8, "OutputFifoEvent alignment mismatch");

template <std::size_t Capacity>
struct EndpointOutputFifoLayout {
    static constexpr std::size_t kStorage = Capacity + 1; // 65
    OutputFifoEvent events[kStorage];   // 65 * 16 = 1040 bytes
    uint32_t head;                      // 4 bytes
    uint32_t tail;                      // 4 bytes
    uint32_t generation;                // 4 bytes
    uint32_t droppedNoteOn;             // 4 bytes
    uint32_t droppedCritical;           // 4 bytes
    uint32_t pad;                       // 4 bytes padding to preserve 8-byte alignment
};
static_assert(sizeof(EndpointOutputFifoLayout<64>) == (65 * 16 + 24), "EndpointOutputFifoLayout size mismatch");
static_assert(sizeof(EndpointOutputFifoLayout<64>) == 1064, "EndpointOutputFifoLayout exact size mismatch");

// External Held Notes Pool (64 held notes)
struct ExternalHeldNote {
    InputKey key;       // 8 bytes
    uint8_t velocity;   // 1 byte
    bool sustained;     // 1 byte
    bool latched;       // 1 byte
    uint8_t pad;        // 1 byte
};
static_assert(sizeof(ExternalHeldNote) == 12, "ExternalHeldNote size mismatch");

template <std::size_t Capacity>
struct ExternalHeldPoolLayout {
    ExternalHeldNote held[Capacity];    // 64 * 12 = 768 bytes
    uint32_t count;                     // 4 bytes
    uint32_t activeEpoch;               // 4 bytes
};
static_assert(sizeof(ExternalHeldPoolLayout<64>) == (64 * 12 + 8), "ExternalHeldPoolLayout size mismatch");
static_assert(sizeof(ExternalHeldPoolLayout<64>) == 776, "ExternalHeldPoolLayout exact size mismatch");

// Telemetry Snapshot Ring Buffer (32 snapshots in RAM)
struct TelemetrySnapshot {
    uint32_t timestampMs;       // 4 bytes
    uint32_t free8;             // 4 bytes
    uint32_t min8;              // 4 bytes
    uint32_t largest8;          // 4 bytes
    uint32_t freeDma;           // 4 bytes
    uint32_t largestDma;        // 4 bytes
    uint16_t notesRx;           // 2 bytes
    uint16_t notesTx;           // 2 bytes
    uint8_t phase;              // 1 byte
    uint8_t usbRole;            // 1 byte
    uint16_t underruns;         // 2 bytes
};
static_assert(sizeof(TelemetrySnapshot) == 32, "TelemetrySnapshot size mismatch");

template <std::size_t Capacity>
struct TelemetryRingLayout {
    TelemetrySnapshot snapshots[Capacity]; // 32 * 32 = 1024 bytes
    uint32_t head;                         // 4 bytes
    uint32_t count;                        // 4 bytes
};
static_assert(sizeof(TelemetryRingLayout<32>) == (32 * 32 + 8), "TelemetryRingLayout size mismatch");
static_assert(sizeof(TelemetryRingLayout<32>) == 1032, "TelemetryRingLayout exact size mismatch");

} // namespace GroovePuterMidi

int main() {
    return 0;
}
