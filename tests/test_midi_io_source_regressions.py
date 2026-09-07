#!/usr/bin/env python3
"""Keep the MIDI input queue single-producer/single-consumer safe."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
QUEUE = (ROOT / "src/midi/midi_input_queue.h").read_text()

assert '#include "midi_realtime_word.h"' in QUEUE
assert "MidiRealtimeWord head_" in QUEUE
assert "MidiRealtimeWord tail_" in QUEUE
assert "loadAcquire" in QUEUE
assert "storeRelease" in QUEUE
