# 0.9.10 R4 — Logical drum MIDI input

R4 extends the incoming-controller target with `DRUMS` while preserving the
existing logical drum runtime and sampler-layer ownership.

The fixed v1 incoming map is:

| MIDI note | Logical lane |
|---|---:|
| 36 Kick | 0 |
| 38 Snare | 1 |
| 42 Closed Hat | 2 |
| 46 Open Hat | 3 |
| 43 Mid Tom | 4 |
| 47 High Tom | 5 |
| 37 Rim | 6 |
| 39 Clap | 7 |

Unmapped notes are ignored. The mapping is input policy and does not read the
outbound `DeviceProfile`.

`MidiInputRouter` retains physical input channel separately from the resolved
logical drum lane so NoteOff remains stable after configuration changes.

`InternalSynthOutput` accepts `MidiInput` for the existing logical DRUMS path.
PerformanceKeyboard/Arpeggiator events continue to honor OutputOwnership;
incoming controller Drums remain independent of that outbound policy. Existing
local drum synthesis and optional sampler layer are reused; no Sampler input
target or new sampler owner is introduced.
