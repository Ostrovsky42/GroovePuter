from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
message_path = ROOT / "src/input/midi_input_message.h"
queue_path = ROOT / "src/input/midi_input_queue.h"
message = message_path.read_text(encoding="utf-8")
queue = queue_path.read_text(encoding="utf-8")
combined = message + "\n" + queue

for forbidden in (
    "Adafruit_TinyUSB",
    "tusb.h",
    "Arduino.h",
    "Preferences.h",
    "device_profile",
    "output_ownership",
    "scene_manager",
    "InternalSynthOutput",
    "MiniAcid",
    "UsbMidiOutput",
    "SEQTRAK",
):
    assert forbidden not in combined, f"R2 core leaked dependency: {forbidden}"

for dynamic in (
    "std::vector",
    "std::deque",
    "std::list",
    "std::map",
    "std::unordered_map",
    "std::function",
    "new ",
    "malloc(",
):
    assert dynamic not in combined, f"R2 core must remain fixed/bounded: {dynamic}"

assert "using MidiInputTransportId = uint8_t;" in message
assert "using MidiInputSessionId = uint32_t;" in message
assert "struct NormalizedMidiInputMessage" in message
assert "MusicalEventTarget" not in message
assert "logicalDrum" not in message
assert "target{" not in message

assert "static constexpr std::size_t kStorageSize = 32;" in queue
assert "static constexpr std::size_t kCapacity = kStorageSize - 1;" in queue
assert "NormalizedMidiInputMessage messages_[kStorageSize]{};" in queue
assert "MidiRealtimeWord head_;" in queue
assert "MidiRealtimeWord tail_;" in queue
assert "MidiRealtimeWord dropped_;" in queue
assert "MidiRealtimeWord maxObservedDepth_;" in queue
assert "sizeof(MidiInputQueue) == 400" in queue

print("MIDI Input 0.9.10 R2 source contracts SUCCESS")
