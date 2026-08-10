# GVEP R0 — Cardputer ADV hardware spike

## Purpose

Measure the actual internal-RAM and realtime-audio cost of a minimal Groove Visual Event Protocol transmitter on Cardputer ADV before any production `VISUAL OUT` setting is implemented.

R0 is intentionally narrow:

- ESP-NOW TX only;
- broadcast on Wi-Fi channel 6;
- fixed 24-byte GVEP v1 packets;
- `KICK`, `PLAY`, `STOP` only;
- fixed 16-event SPSC queue;
- no acknowledgement/retry protocol;
- no Scene/project persistence;
- no production Settings UI change.

The normal build keeps `GROOVEPUTER_GVEP_R0=0`. The dedicated R0 build sets it to `1`. In the R0 build the transmitter starts only after the normal GroovePuter UI object exists, then waits another 2 seconds before Wi-Fi initialization so the serial checkpoints measure the loaded application rather than early boot.

The final feature still requires the documented production contract:

```text
SETTINGS
  VISUAL OUT
    OFF       <- default
    ESP-NOW
```

R0 does not weaken that requirement; it avoids persistence/UI work until the radio cost is known.

## Hardware list

- M5Stack Cardputer ADV running GroovePuter;
- one ESP32/ESP32-S3 receiver for the reference logger;
- USB cable for Cardputer ADV serial/upload;
- USB cable for the receiver serial/upload;
- headphones or speaker used for normal GroovePuter listening tests;
- optional SEQTRAK for the existing MIDI/clock smoke test.

A second Cardputer/Cardputer ADV can be used as the reference receiver. MCP eye's can consume the same packet later; R0 does not depend on that project.

## Wiring

No signal wiring is required. ESP-NOW uses the ESP32-S3 2.4 GHz radio.

Cardputer ADV hardware assumptions remain unchanged:

- target: ESP32-S3 / Cardputer ADV;
- PSRAM disabled for the production-equivalent memory profile;
- PORT.A remains untouched;
- normal Cardputer ADV power/audio/I2C pin ownership remains unchanged.

Both ESP-NOW devices must use Wi-Fi channel `6` for this R0 test.

## Build / flash

### 1. Checkout the exact R0 branch

```bash
git fetch origin
git switch agent/20260810-02-gvep-r0-hardware-spike
git reset --hard origin/agent/20260810-02-gvep-r0-hardware-spike
```

Record the exact SHA:

```bash
git rev-parse HEAD
```

### 2. Install the pinned Arduino dependencies

```bash
bash scripts/install_arduino_deps.sh
```

The repository pins the M5Stack ESP32 core and M5 libraries used by the normal Cardputer ADV build.

### 3. Run the focused host protocol test

```bash
bash tests/run_gvep_r0_tests.sh
```

Expected:

```text
GVEP R0 host tests: PASS
```

### 4. Build the normal OFF baseline

```bash
rm -rf build/cardputer-adv-current
bash scripts/build.sh --warnings all
```

This build does not define `GROOVEPUTER_GVEP_R0`; no GVEP worker or Wi-Fi initialization is referenced by the realtime producer path.

### 5. Build the R0 ESP-NOW transmitter

```bash
rm -rf build/cardputer-adv-gvep-r0
bash scripts/build_gvep_r0.sh --warnings all
```

### 6. Flash the R0 transmitter

```bash
BUILD_PATH="$PWD/build/cardputer-adv-gvep-r0" \
  bash scripts/upload.sh /dev/ttyACM0
```

Change `/dev/ttyACM0` only if the Cardputer enumerates on another device.

### 7. Build the reference receiver

For a second Cardputer/Cardputer ADV using the same pinned M5Stack core:

```bash
RX_BUILD="$PWD/build/gvep-r0-receiver"
rm -rf "$RX_BUILD"
arduino-cli compile \
  --clean \
  --fqbn "m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc" \
  --output-dir "$RX_BUILD" \
  tools/gvep_receiver/GvepReceiver
```

Flash it, using its actual serial port:

```bash
arduino-cli upload \
  --fqbn "m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc" \
  -p /dev/ttyACM1 \
  --input-dir "$PWD/build/gvep-r0-receiver"
```

For another ESP32-S3 board, keep the receiver source unchanged and substitute only that board's valid FQBN/upload port.

## Expected behavior

### Cardputer ADV screen

The normal GroovePuter UI must look and behave exactly as before. R0 adds no page, overlay, visual indicator, or control binding.

### Cardputer ADV serial

After the normal UI is constructed, serial must show the ordered resource checkpoints:

```text
[GVEP-R0][before-wifi] freeInt=... minInt=... largestInt=... freePsram=...
[GVEP-R0][after-wifi] freeInt=... minInt=... largestInt=... freePsram=...
[GVEP-R0][after-espnow] freeInt=... minInt=... largestInt=... freePsram=...
[GVEP-R0][after-peer] freeInt=... minInt=... largestInt=... freePsram=...
[GVEP-R0] READY channel=6 tx=..:..:..:..:..:.. packet=24B
```

Every five seconds while enabled:

```text
[GVEP-R0][RUN] freeInt=... minInt=... largestInt=... pub=... pop=... drop=... high=... send=.../... cb=.../... initFail=...
```

Interpretation:

- `pub`: semantic events accepted by the fixed queue;
- `pop`: events removed by the TX worker;
- `drop`: events discarded because the queue was full;
- `high`: maximum observed queue depth, maximum `16`;
- `send=A/B`: `esp_now_send()` accepted/failed enqueue counts;
- `cb=A/B`: ESP-NOW send callback success/fail counts;
- `initFail`: initialization failures.

### Receiver serial

At boot:

```text
GVEP R0 reference receiver
[GVEP] READY channel=6 rx=..:..:..:..:..:..
```

Starting GroovePuter playback should produce a `PLAY` event. Pattern kick hits should produce `KICK`. Stopping playback should produce `STOP`.

Example:

```text
[GVEP] seq=12 event=KICK(0x01) value=100 bar=1 step=4 tick=480 ts=12345678 flags=0x00
```

R0 timestamps are the transmitter's 32-bit monotonic microsecond value at ESP-NOW serialization/send time. They are intended for interval/latency experiments and wrap naturally.

## Measurement procedure

### A. Baseline

Flash the normal build from step 4 and record the existing GroovePuter heap diagnostics after normal startup.

### B. R0 radio deltas

Flash the R0 build and copy these four lines verbatim:

```text
before-wifi
after-wifi
after-espnow
after-peer
```

Calculate:

```text
Wi-Fi heap delta       = before-wifi - after-wifi
ESP-NOW heap delta     = after-wifi - after-espnow
peer/setup heap delta  = after-espnow - after-peer
largest-block delta    = before-wifi largestInt - after-peer largestInt
```

Do not judge safety from total free heap alone. `largestInt` is a release-relevant metric for this project.

### C. Dense musical traffic

Use a pattern with frequent kick/retrigger activity while also running the heaviest normal Synth A/B + FX combination you use for release acceptance.

Listen for:

- crackle;
- I2S write failures;
- timing wobble;
- UI stalls;
- MIDI/SEQTRAK clock degradation.

`drop > 0` is preferable to any audio failure.

### D. 30-minute soak

Keep the dense pattern and visual receiver active for 30 minutes. Capture at least the first and last `[GVEP-R0][RUN]` lines and the normal audio underrun diagnostics.

### E. Receiver failure test

While GroovePuter is playing:

1. power the receiver off;
2. keep playing for at least one minute;
3. power the receiver back on;
4. verify GroovePuter never pauses or blocks.

Broadcast delivery is best-effort. R0 intentionally has no application ACK/retry loop.

## Troubleshooting

### Receiver prints READY but no events

Check that both devices report channel `6`. The R0 transmitter does not scan or hop channels.

Confirm the GroovePuter build was created with:

```bash
bash scripts/build_gvep_r0.sh --warnings all
```

A normal `scripts/build.sh` build intentionally has GVEP disabled.

### `esp_now_init` or peer setup fails

Copy all `[GVEP-R0]` initialization lines. Do not add retries inside the audio producer. R0 treats initialization failure as visual-output failure and leaves GroovePuter itself running.

### Audio crackles after `READY`

Treat R0 as failed. Record:

- exact firmware SHA;
- `before-wifi`, `after-wifi`, `after-espnow`, `after-peer` metrics;
- latest `[GVEP-R0][RUN]` line;
- audio underrun count;
- active synth/drum/FX configuration.

Do not lower the existing DRAM gate to make GVEP pass.

### `drop` increases

This is not automatically a failure. The producer is deliberately non-blocking. Check `high`, send callback results, and whether the visual output still feels acceptable.

### `cb` failures increase when receiver is absent

This can occur with best-effort ESP-NOW delivery. GroovePuter must remain unaffected. R0 does not retry from the musical producer path.

## Acceptance checklist

### Build / protocol

- [ ] exact R0 SHA recorded;
- [ ] `bash tests/run_gvep_r0_tests.sh` passes;
- [ ] normal Cardputer ADV build still succeeds;
- [ ] dedicated `scripts/build_gvep_r0.sh` build succeeds;
- [ ] receiver accepts only the documented 24-byte `GVE1`/v1 EVENT frame;
- [ ] `KICK=0x01`, `PLAY=0x20`, `STOP=0x21` remain unchanged.

### Screen / serial

- [ ] Cardputer screen/UI is unchanged;
- [ ] four ordered heap checkpoints appear;
- [ ] `READY channel=6` appears;
- [ ] receiver logs PLAY on playback start;
- [ ] receiver logs actual PatternPlayer kick hits;
- [ ] receiver logs STOP on playback stop;
- [ ] kick `value` follows the normalized trigger velocity.

### Realtime / memory

- [ ] Wi-Fi heap delta recorded;
- [ ] ESP-NOW heap delta recorded;
- [ ] `largestInt` delta recorded;
- [ ] existing fixed-DRAM gate is not changed or weakened;
- [ ] no I2S/audio underrun regression during dense traffic;
- [ ] no audible crackle correlated with GVEP traffic;
- [ ] no transport/MIDI timing regression;
- [ ] queue overflow drops visual events rather than blocking audio;
- [ ] receiver-off/reboot does not disturb GroovePuter;
- [ ] 30-minute soak passes.

## R0 decision

Only if every realtime/memory acceptance item passes should work continue to R1 protocol expansion and a real persistent `SETTINGS -> VISUAL OUT` implementation.

If R0 fails because Wi-Fi/ESP-NOW consumes too much internal RAM or creates audio jitter, keep the protocol-neutral GVEP event API but reject or redesign the ESP-NOW transport. Do not compensate by weakening GroovePuter's existing safety gates.
