# Groove Visual Event Protocol — Research Design Brief

**Status:** research only; no production implementation is approved by this document  
**Base:** `dev_0.9_test` @ `0a2fe15696eb7c8f0bdcc1a986aed3ca342dd948`  
**Research branch:** `agent/20260810-01-visual-event-protocol-research`  
**Working name:** Groove Visual Event Protocol (`GVEP`)  
**Initial transport candidate:** ESP-NOW on Cardputer ADV / ESP32-S3  
**Purpose:** define a small, optional, non-blocking event API that lets GroovePuter drive arbitrary user-built visualizations without coupling the sequencer, DSP, UI, or protocol to one specific display project.

This document intentionally separates the **application protocol** from the **radio transport**.

`MCP eye's` is a useful first receiver and acceptance target, but it is **not** the protocol owner and must not appear in the core protocol contract.

---

# Executive decision

The feature is worth researching, but it must enter GroovePuter only behind an explicit runtime setting and only after hardware evidence proves that enabling Wi-Fi/ESP-NOW does not violate audio or internal-memory gates.

The desired architecture is:

```text
Sequencer / transport / musical engines
                |
                | semantic, non-blocking events
                v
        Groove Visual Event Bus
                |
                | stable application API
                v
        Transport adapter boundary
           |             |
           |             +---- future USB / Serial / UDP / other
           v
      ESP-NOW adapter
           |
           v
   arbitrary visual receiver(s)
```

Primary contract:

> **GroovePuter publishes musical intent, not graphics.**
>
> GroovePuter must never transmit framebuffers, LVGL commands, pixel coordinates, animation names tied to one receiver, or receiver-specific state.

A receiver decides what `KICK`, `SNARE`, `BAR`, `PLAY`, `STOP`, `FILL`, or other supported semantic events look like.

---

# User-visible feature boundary

The feature must be separately enabled in Settings.

Initial UI contract:

```text
SETTINGS
  VISUAL OUT
    OFF       <- default
    ESP-NOW
```

Future transport choices may extend this list without changing the event API.

## OFF means actually off

When `VISUAL OUT = OFF`:

- Wi-Fi must not be initialized solely for GVEP;
- ESP-NOW must not be initialized;
- no GVEP worker task may run;
- no dynamic event queue may be allocated;
- no radio callbacks may be registered;
- musical behavior and timing must be identical to a build with GVEP unused;
- there must be no requirement for a visualization receiver to be present.

The feature must therefore be safe to leave compiled into firmware while remaining operationally absent until enabled.

## ESP-NOW enabled

When `VISUAL OUT = ESP-NOW`:

- GroovePuter initializes the minimum Wi-Fi/ESP-NOW resources required by the selected transport;
- events are delivered best-effort;
- visual delivery is subordinate to audio correctness;
- a missed visual event is acceptable;
- an audio underrun, crackle, scheduler stall, or transport-induced UI freeze is not acceptable.

---

# Non-goals

This research does **not** approve:

- remote control of GroovePuter through ESP-NOW;
- sending pattern contents over GVEP;
- sending audio samples or FFT frames;
- transmitting a framebuffer;
- transmitting every 96 PPQN clock pulse by default;
- a bidirectional distributed state system;
- a receiver-specific animation protocol;
- replacing MIDI clock or the existing MIDI transport;
- changing Scene persistence format during the research phase;
- weakening existing fixed-DRAM, runtime, MIDI, synth, or release gates;
- adding a networking framework rewrite.

GVEP is an **output event bus**, not another musical control plane.

---

# Ownership model

## Producers

Existing GroovePuter subsystems may report events only when the event has already become musically real.

Examples:

- drum sequencer emits `KICK` only when a kick is actually triggered;
- transport emits `PLAY` only when playback enters the playing state;
- bar logic emits `BAR` at the accepted bar boundary;
- future generation code may emit `FILL` only when a fill is materialized/entered, not merely considered.

A producer must not know whether ESP-NOW is enabled.

## VisualEventBus

`VisualEventBus` owns:

- the protocol-neutral event structure;
- non-blocking enqueue semantics;
- sequence numbering;
- drop/coalesce counters;
- fan-out to zero or one enabled transport in the first implementation.

It does **not** own:

- Wi-Fi setup;
- peer discovery;
- graphics;
- LVGL;
- receiver-specific animation logic;
- musical state.

## Transport adapter

`EspNowVisualTransport` owns:

- Wi-Fi/ESP-NOW lifecycle when the setting is enabled;
- peer/broadcast policy;
- serialization of protocol packets;
- send callbacks;
- transport counters and errors;
- shutdown and resource release when the setting is disabled.

It does not call back into DSP or mutate sequencer state.

## Receiver

A receiver owns the complete visual mapping.

Examples:

```text
KICK velocity=120 -> pupil pulse
KICK velocity=120 -> LED flash
KICK velocity=120 -> particle burst
KICK velocity=120 -> mechanical meter impulse
```

All four are equally valid GVEP consumers.

---

# Internal producer API candidate

The first implementation should expose one small protocol-neutral operation conceptually equivalent to:

```text
publish(event)
```

The event model must contain only data that is already known at the musical trigger point:

```text
VisualEvent
  type
  value
  flags
  sequence
  musicalTick
  bar
  step
  monotonicTimestamp
```

Required behavior:

```text
publish(event):
  never block
  never wait for radio
  never allocate
  never retry synchronously
  never log synchronously from an audio-critical path

if queue full:
  increment dropped counter
  drop/coalesce visual event
  continue music immediately
```

This is a hard invariant.

---

# Event vocabulary — candidate v1

The protocol should start small. A large vocabulary before hardware testing creates false API stability.

## Numeric allocation

`0x00` is invalid/reserved and must never be emitted.

R0 freezes only the three IDs required for the hardware spike:

| ID | Event | Meaning | `value` |
|---:|---|---|---|
| `0x01` | `KICK` | kick drum actually triggered | MIDI-style trigger velocity `0..127` |
| `0x20` | `PLAY` | transport entered playing state | `0` |
| `0x21` | `STOP` | transport stopped | `0` |

Candidate R1 allocations are intentionally documented but remain provisional until R1 begins:

| ID | Event | Meaning | `value` |
|---:|---|---|---|
| `0x02` | `SNARE` | snare actually triggered | velocity `0..127` |
| `0x03` | `CLAP` | clap actually triggered | velocity `0..127` |
| `0x04` | `HAT_CLOSED` | closed-hat trigger | velocity `0..127` |
| `0x05` | `HAT_OPEN` | open-hat trigger | velocity `0..127` |
| `0x06` | `PERC` | generic percussion trigger | velocity `0..127` |
| `0x10` | `BAR` | accepted bar boundary | `0` |
| `0x11` | `FILL` | fill section/phrase entered | `0` |
| `0x12` | `BREAK` | break/empty section entered | `0` |
| `0x30` | `ACCENT` | explicit accent semantic event where already available | `0..127` intensity |

Allocation policy:

- `0x01..0x0F`: trigger/percussion events;
- `0x10..0x1F`: structural/phrase events;
- `0x20..0x2F`: transport events;
- `0x30..0x3F`: expressive semantic events;
- `0x40..0x7F`: reserved for future core protocol allocation;
- `0x80..0xFF`: experimental/private receiver ecosystem use; never emitted by production GroovePuter unless later standardized.

Unknown event IDs must be ignored safely by receivers.

Events describe musical semantics, not display instructions.

Do not add names such as `BLINK`, `PUPIL_PULSE`, `EYE_SHAKE`, `LED_RED`, or `FIRE_ANIMATION` to the GroovePuter protocol.

---

# Wire protocol candidate

The hardware spike should use a **small fixed binary packet**. JSON is explicitly out of scope for the real-time event path.

The candidate R0 event frame is exactly 24 bytes:

| Byte(s) | Field | Encoding |
|---:|---|---|
| `0..3` | magic | ASCII `GVE1` |
| `4` | protocol version | `uint8`, initially `1` |
| `5` | message type | `uint8`, `1 = EVENT` |
| `6` | event type | `uint8`, numeric registry above |
| `7` | flags | `uint8`; all bits `0` in R0 |
| `8..11` | sequence | little-endian `uint32` |
| `12..15` | musical tick | little-endian `uint32`, GroovePuter 96 PPQN domain |
| `16..19` | monotonic timestamp low 32 bits | little-endian `uint32`, microseconds |
| `20..21` | bar index | little-endian `uint16` |
| `22` | step | `0..15`, or `255` when not applicable |
| `23` | value | event-defined by the numeric registry; R0 trigger velocity is `0..127` |

R0 flag rule:

```text
sender:   flags = 0
receiver: ignore unknown flag bits
```

No semantic meaning is allocated to flag bits until a later protocol revision documents it.

Important ABI rule:

> The wire format is serialized explicitly byte-by-byte. A compiler-native C/C++ struct layout must not be used as the protocol definition.

This prevents padding, alignment, and compiler differences from becoming part of the public API.

## Sequence

`sequence` lets receivers:

- detect packet loss;
- ignore duplicates;
- measure loss rate without acknowledgements.

Sequence wrap is legal and receivers must compare it modulo `uint32` rather than treating wrap as a reset/error.

## Musical tick

The current GroovePuter timing domain is 96 PPQN. The receiver may use the tick for visual phase or diagnostics, but it must not assume that every tick is transmitted.

## Monotonic timestamp

The timestamp exists for latency measurement and optional receiver-side scheduling experiments. It wraps naturally as a 32-bit microsecond counter; receivers that compare timestamps must do wrap-safe unsigned arithmetic. R0 receivers may ignore it.

## Bar and step

`bar` and `step` are descriptive coordinates captured at the event source. They are not commands to move the GroovePuter sequencer. `step = 255` means that a 16-step position is not applicable to that event.

---

# Timing and real-time contract

The visual path must never become part of the audio deadline.

Required flow:

```text
musical trigger
    |
    +--> normal synth/drum work
    |
    +--> VisualEventBus.tryPublish()
              |
              v
          fixed queue
              |
              v
      low-priority visual TX worker
              |
              v
          ESP-NOW send
```

Forbidden flow:

```text
audio/sequencer -> wait -> Wi-Fi -> callback -> retry -> continue audio
```

## Event-rate policy

GVEP is for **semantic events**, not a high-rate telemetry stream.

Research defaults:

- no periodic 96 PPQN packet stream;
- no audio-rate data;
- fixed queue, proposed initial capacity: `16` events;
- proposed software rate ceiling: `64 events/s` before coalescing/dropping non-essential visual events;
- `KICK`, `PLAY`, `STOP`, and bar/state-transition events have higher retention priority than dense hat telemetry;
- no retransmission from an audio-critical producer.

These values are research candidates, not frozen production constants.

---

# ESP-NOW transport boundary

ESP-NOW is attractive because the GVEP payload is tiny and connectionless delivery maps well to ephemeral musical triggers.

The transport must nevertheless be treated as a Wi-Fi feature with real RAM and scheduler cost.

## Channel

ESP-NOW peers must operate on a compatible Wi-Fi channel. A peer configured with channel `0` uses the current interface channel.

The first GroovePuter spike should avoid dynamic channel hopping and should document the actual selected channel in serial diagnostics.

## Send callbacks

ESP-NOW send/receive callbacks execute in the Wi-Fi task context. They must do minimal work only.

For GroovePuter the callback may:

- update an atomic/counter-sized transport result;
- enqueue a tiny diagnostic result if necessary.

It must not:

- render UI;
- print long serial diagnostics;
- perform retry loops;
- touch synth state;
- acquire locks used by audio/sequencer code.

## Broadcast vs paired unicast

R0 may test broadcast because it makes arbitrary receiver bring-up simple.

A production decision must compare:

- broadcast: simple discovery, one-to-many visuals, no receiver registration requirement;
- unicast peers: explicit target ownership and optional encryption, but pairing/settings complexity.

The application protocol must remain identical in either case.

---

# Settings and persistence

The user-facing enable state is independent from the protocol version.

Candidate persisted setting:

```text
visualOutMode = OFF | ESPNOW
```

Rules:

1. Default is `OFF` for new and migrated configurations.
2. Legacy Scenes/projects must not implicitly enable Wi-Fi.
3. Turning the feature off must tear down transport-owned resources when safe.
4. A missing receiver must never block boot, playback, Save/Load, or shutdown.
5. Receiver MAC/channel details, if later required, belong to device/settings persistence, not musical Scene content.

Research may initially keep the setting compile-time or volatile if changing persistent settings would widen the spike. The final feature must satisfy the rules above before production merge.

---

# Resource contract

This is the main research gate.

ESP-NOW payload size is not the concern; Wi-Fi initialization, Wi-Fi buffers, tasks, and heap fragmentation are.

No absolute RAM budget is frozen before measurement. The spike must record evidence instead of guessing.

Required measurements on Cardputer ADV:

```text
A. VISUAL OUT OFF after normal boot
B. immediately before Wi-Fi init
C. after Wi-Fi init
D. after ESP-NOW init
E. after peer/broadcast setup
F. during dense event transmission
G. after VISUAL OUT is disabled/deinitialized
H. after 30-minute combined audio + visual soak
```

At every point record at least:

```text
free internal heap
minimum free internal heap
largest free internal block
free PSRAM if available
visual queue high-water mark
visual events published
visual events sent
visual events dropped/coalesced
ESP-NOW send success/fail counters
audio underrun/dropout counter if available
```

Hard rule:

> GVEP must not weaken any existing fixed-DRAM or release memory gate to make the feature pass.

If Wi-Fi initialization makes the existing safe memory envelope impossible, the feature remains experimental or requires a separate memory optimization effort. The gate must not be lowered to accommodate it.

---

# Audio correctness contract

The following are release blockers for the feature:

- audible crackle correlated with visual traffic;
- playback timing drift introduced by the transport;
- MIDI clock degradation;
- synth voice starvation caused by networking;
- UI stalls caused by ESP-NOW callbacks;
- deadlock or priority inversion involving the visual path;
- audio-critical code waiting for queue space or Wi-Fi completion.

Packet loss is **not** a release blocker by itself if music remains correct and the measured loss is acceptable for visual use.

Priority order is explicit:

```text
1. audio correctness
2. musical/transport timing
3. normal GroovePuter UI
4. visual event delivery
```

---

# Failure model

GVEP must fail soft.

| Failure | Required GroovePuter behavior |
|---|---|
| no receiver present | continue normally |
| receiver powered off | continue normally |
| packet loss | continue normally; count it |
| queue full | drop/coalesce visual event |
| ESP-NOW send error | count; continue |
| channel mismatch | expose diagnostic; continue |
| Wi-Fi init failure | disable visual transport for session; continue GroovePuter |
| receiver sends unexpected data | ignore in TX-only R0 |

No GVEP failure may trigger Panic/AllNotesOff unless the underlying musical system independently requires it.

---

# Receiver API contract

A third-party visualization only needs to implement the GVEP packet decoder and its own mapping.

Minimal receiver behavior:

```text
receive bytes
  -> require exactly 24 bytes for EVENT v1
  -> validate magic == GVE1
  -> validate protocolVersion == 1
  -> validate messageType == 1
  -> decode little-endian fields explicitly
  -> ignore unknown event IDs
  -> reject duplicate/stale sequence if desired
  -> map known event to local visual behavior
```

The receiver must not need:

- GroovePuter source code;
- Genre internals;
- Pattern storage format;
- Scene format;
- LVGL;
- M5Stack libraries;
- MCP eye's code.

That is the portability criterion for the protocol.

---

# Example receivers

## MCP eye's

Possible local mapping:

```text
KICK       -> pupil/eye squash impulse
SNARE      -> short lateral twitch
HAT_CLOSED -> small highlight pulse
BAR        -> blink or gaze reset
PLAY       -> wake/active expression
STOP       -> idle expression
FILL       -> temporary animation intensity increase
```

This mapping belongs entirely to MCP eye's.

## LED matrix

```text
KICK       -> center flash
SNARE      -> horizontal burst
HAT_CLOSED -> edge sparkle
BAR        -> palette phase change
```

## Addressable LEDs

```text
KICK  -> brightness impulse
SNARE -> secondary color impulse
BAR   -> pattern phase reset
```

## User-built kinetic or screen visualization

The same packet may drive a meter, servo-safe animation controller, desktop application, external ESP32 screen, or another visualization endpoint without any GroovePuter protocol change.

---

# Versioning rules

GVEP must be versioned from the first packet.

Candidate policy:

```text
magic: GVE1
protocolVersion: 1
```

Rules:

1. Receivers must validate packet length and version before decoding.
2. Unknown event IDs must be ignored, not treated as fatal.
3. Unknown flag bits must be ignored unless a later revision explicitly declares them mandatory.
4. Existing event IDs never change meaning within a major protocol version.
5. New event IDs may be added compatibly.
6. A breaking wire-layout change requires a new major magic/version.
7. Receiver-specific extensions use only the documented experimental/private range until standardized.

A later public protocol document should include a machine-readable event registry if the vocabulary grows materially.

---

# Security and trust boundary

R0 is TX-only from GroovePuter.

This sharply limits risk:

- received ESP-NOW application commands are not part of the feature;
- a visualization cannot alter notes, transport, Scene state, MIDI routing, or settings through GVEP;
- arbitrary receiver payloads do not enter the musical engine.

If bidirectional control is ever proposed, it requires a separate architecture review and must not be smuggled into the visual-output transport.

---

# Proposed implementation boundaries after research approval

No production files are added by this research branch. If the spike passes, the expected minimal implementation shape is approximately:

```text
src/visual/
  VisualEvent.*
  VisualEventBus.*
  VisualOutSettings.*

src/visual/transports/
  EspNowVisualTransport.*

examples/ or tests/hardware/
  GVEP receiver reference test
```

Exact paths must follow the repository layout found at implementation time. This design does not authorize a framework rewrite.

---

# Research stages

## R0 — memory and jitter spike

Scope:

- Cardputer ADV transmitter only;
- `KICK`, `PLAY`, `STOP` only;
- one tiny fixed queue;
- ESP-NOW TX only;
- simple receiver/reference logger;
- no pairing UI;
- no Scene format change;
- serial resource metrics;
- dense audio stress test.

Decision produced by R0:

```text
Is Wi-Fi + ESP-NOW affordable inside GroovePuter's actual internal-RAM and audio-jitter envelope?
```

If no, stop. Do not continue expanding the protocol.

## R1 — protocol validation

Only after R0 passes:

- implement explicit serializer/decoder;
- validate sequence/loss behavior;
- add `SNARE`, `CLAP`, `HAT_CLOSED`, `HAT_OPEN`, `BAR`;
- test at least two receiver implementations/mappings;
- test broadcast vs unicast policy;
- define settings UX.

## R2 — production candidate

Only after R1 passes:

- persistent `VISUAL OUT` setting defaulting to OFF;
- resource teardown/re-enable test;
- final protocol v1 registry;
- documentation for third-party receivers;
- CI/source contracts for non-blocking ownership;
- hardware acceptance on exact SHA.

---

# Hardware research test

## Purpose

Measure the real RAM, scheduling, audio, and packet-delivery cost of enabling ESP-NOW visual output on GroovePuter before any production feature is approved.

## Hardware

- M5Stack Cardputer ADV / ESP32-S3 running GroovePuter research firmware;
- one ESP32-family receiver capable of ESP-NOW receive;
- USB cable for Cardputer serial logging;
- optional second receiver for one-to-many broadcast testing;
- headphones/speaker and normal MIDI setup used for GroovePuter hardware acceptance.

## Wiring

No signal wiring is required for ESP-NOW.

Normal Cardputer ADV hardware rules remain unchanged. Do not repurpose PORT.A or other GroovePuter pins for this test.

## Build / flash

R0 implementation must provide a reproducible build target or compile-time test switch without altering the normal release target.

The exact commands must be written into the implementation PR once the spike exists. This research-only branch intentionally does not invent commands for code that has not been written.

## Expected behavior

With `VISUAL OUT = OFF`:

```text
GroovePuter behavior is unchanged.
Wi-Fi/ESP-NOW is not initialized for GVEP.
```

With `VISUAL OUT = ESP-NOW`:

```text
PLAY -> receiver logs/visualizes PLAY
kick -> receiver logs/visualizes KICK with value/velocity
STOP -> receiver logs/visualizes STOP
```

Audio remains clean while the receiver is present, absent, rebooting, or dropping packets.

## Troubleshooting

### Receiver sees nothing

Check:

- transmitter reports ESP-NOW initialized;
- receiver and transmitter use compatible Wi-Fi channels;
- packet magic/version is accepted;
- broadcast/unicast target matches test configuration.

Do not add retry loops to an audio-critical path to compensate.

### Audio crackles after enabling

Treat this as a failed R0 gate. Capture memory and task/resource metrics and stop feature expansion until the cause is understood.

### Largest internal block collapses

Treat this as a failed memory gate even if total free heap appears acceptable. Heap fragmentation is part of the acceptance decision.

### Visual events are occasionally missing

Inspect sequence gaps, queue high-water mark, drop counters, and send results. Occasional best-effort loss is preferable to disturbing audio.

---

# Acceptance checklist

## Research branch

- [x] research is isolated from production implementation;
- [x] base branch and SHA are recorded;
- [x] `VISUAL OUT` is specified as separately enabled and default OFF;
- [x] OFF semantics require no GVEP-owned Wi-Fi initialization;
- [x] MCP eye's is documented only as one receiver example;
- [x] protocol semantics are independent of graphics/UI implementation;
- [x] ESP-NOW is separated behind a transport adapter boundary;
- [x] TX-only/non-blocking ownership is explicit;
- [x] packet-loss behavior is explicitly subordinate to audio correctness;
- [x] public wire-format candidate is versioned and fixed-size;
- [x] R0 numeric event IDs and value semantics are explicit;
- [x] byte order, timestamp wrap, unknown IDs, and flags behavior are explicit;
- [x] third-party receiver requirements are documented;
- [x] R0 memory/jitter measurements are defined;
- [x] existing release/DRAM gates may not be weakened.

## R0 implementation acceptance

- [ ] exact test SHA recorded;
- [ ] `VISUAL OUT = OFF` does not initialize GVEP Wi-Fi/ESP-NOW resources;
- [ ] OFF baseline memory metrics recorded;
- [ ] Wi-Fi init delta recorded;
- [ ] ESP-NOW init delta recorded;
- [ ] largest internal free block recorded at all required checkpoints;
- [ ] 30-minute dense audio + visual soak passes without crackle/underrun;
- [ ] receiver-off/reboot test does not affect playback;
- [ ] queue overflow drops visual events without blocking music;
- [ ] sequence gaps/loss rate are measurable;
- [ ] normal MIDI/SEQTRAK transport smoke remains clean;
- [ ] disabling visual output releases transport resources as designed;
- [ ] existing fixed-DRAM/release gates remain unchanged and pass.

## R1 protocol acceptance

- [ ] at least two distinct visualization mappings consume the same packets;
- [ ] no receiver-specific graphics concepts enter core event IDs;
- [ ] unknown event IDs are safely ignored;
- [ ] protocol decoder rejects wrong magic/version/length;
- [ ] broadcast/unicast decision is documented from measurements, not preference;
- [ ] third-party receiver documentation is copy-pasteable.

---

# Decision criteria

Promote GVEP toward production only if all of the following are true:

1. ESP-NOW initialization fits the real Cardputer ADV internal-memory envelope without weakening existing gates.
2. Dense visual traffic does not produce audible or measurable timing regressions.
3. The event producer path remains constant-time, non-blocking, and allocation-free.
4. `VISUAL OUT = OFF` leaves networking operationally absent.
5. At least two visually different receivers can consume the same protocol without GroovePuter-specific graphics logic.
6. The public protocol remains substantially smaller and simpler than exposing internal sequencer/MIDI structures.

If these conditions fail, the correct outcome is to keep GVEP experimental or reject ESP-NOW as the transport while preserving the protocol-neutral event-bus idea.

---

# Vision

The target is not “make MCP eye's blink on kick.”

The target is a small, optimized visual event output standard for GroovePuter:

```text
GroovePuter music engine
        |
        | GVEP
        +--------------------+
        |                    |
        v                    v
     ESP-NOW              future transport
        |
   +----+---------+----------------+
   |              |                |
   v              v                v
MCP eye's      LED matrix      user display
                                   / lights /
                                   kinetic UI
```

A user should be able to build a visualization by implementing a tiny GVEP receiver, choose their own rendering language and hardware, and remain insulated from GroovePuter's internal pattern, Scene, genre, DSP, and UI architecture.

That is the feature boundary worth preserving.

---

# Primary technical references

- Espressif ESP-IDF ESP32-S3 ESP-NOW API: `https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html`
- Espressif ESP32-S3 Wi-Fi performance and buffer usage: `https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/wifi-driver/wifi-performance-and-power-save.html`
- Espressif ESP32-S3 RAM usage guidance: `https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32s3/api-guides/performance/ram-usage.html`
