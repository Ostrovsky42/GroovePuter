# GroovePuter 0.9.5-A — Hardened WAV loader

## Purpose

0.9.5-A hardens only the sampler storage/loading foundation. It does not add kit loading, relink UX, streaming, slicing, recording, waveform UI, voices, or sample-pool capacity.

Frozen base:

```text
d3db4e48ebc08862bdaf9f62532414f009839192  (0.9.4 FINAL)
```

The contract is:

```text
inspect (no PCM allocation) -> caller admission -> decode (one output allocation)
```

Accepted WAV subset is intentionally narrow: RIFF/WAVE, integer PCM format 1, 16-bit, mono or stereo. Stereo is decoded to mono in bounded 512-byte source chunks. PCM `byteRate` must exactly equal `sampleRate * blockAlign`; duplicate required `fmt `/`data` chunks are rejected; the parser validates the complete declared RIFF boundary even after both required chunks have been found.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- microSD card
- USB-C data cable
- normal Cardputer ADV no-PSRAM profile

No Yamaha SEQTRAK or external module is required for this test.

## Wiring

No external wiring is required. Use the built-in microSD slot and USB-C connection.

PORT.A / I2C is not used. GPIO2/GPIO1 and external 3.3 V/5 V rails are unchanged and must not be connected for this test.

## Build / Flash

Host regression corpus:

```bash
bash tests/run_sampler_wav_loader_tests.sh
```

The runner generates 32 deterministic WAV fixtures under:

```text
build/host-tests/sampler-wav-loader/corpus/
```

Then run the normal release gates:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash the exact candidate SHA:

```bash
git rev-parse HEAD
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

For hardware smoke, copy selected generated fixtures from the host corpus to `/samples/` on the microSD. Use `valid_mono.wav`, `valid_stereo.wav`, `odd_junk.wav`, and `odd_list.wav` as positive cases. Keep malformed fixtures only for explicit negative tests.

## Expected behavior

### Host

`tests/run_sampler_wav_loader_tests.sh` prints:

```text
sampler WAV loader 0.9.5-A: PASS
```

The corpus verifies:

- valid PCM16 mono;
- valid PCM16 stereo -> mono averaging;
- stereo payload larger than 512 bytes crosses multiple bounded scratch iterations correctly;
- odd-sized `JUNK`/`LIST` RIFF chunks with mandatory padding;
- extended PCM `fmt ` chunk;
- `data` before `fmt `;
- valid unknown chunks after required chunks;
- physical bytes after the declared RIFF boundary remain ignored;
- invalid RIFF and truncated RIFF/file bounds;
- overflow-shaped unknown chunk length;
- missing odd-chunk pad;
- too-short/missing `fmt ` and missing `data`;
- float/PCM8/PCM24 rejection;
- 0-channel and >2-channel rejection;
- zero sample rate, bad block alignment and invalid byte rate rejection;
- unaligned/empty data rejection;
- duplicate `fmt ` and duplicate `data` rejection;
- malformed trailing chunk rejection after valid required chunks;
- decoded-byte admission rejection before PCM allocation;
- inspect/decode metadata change detection.

### Cardputer ADV screen

- boot reaches the normal GroovePuter UI;
- `DRUMS -> SAMPLES` remains reachable;
- valid mono and stereo samples preload and preview normally;
- stereo samples sound as mono and do not require a second full-size transient allocation;
- sampler controls, 8-voice behavior, pitch/reverse/loop/choke and the 32 KiB pool policy remain unchanged.

### Serial

Valid files must not print loader errors. Unsupported or malformed files must fail cleanly without reset/WDT. Compatibility wrapper failures print a bounded reason such as `unsupported-encoding`, `unsupported-channels`, `invalid-format`, `truncated`, or `too-large`.

## Troubleshooting

### Valid stereo fails

Confirm it is RIFF/WAVE integer PCM format 1, exactly 16-bit and exactly 2 channels. WAVE_FORMAT_EXTENSIBLE (`0xFFFE`) is outside 0.9.5-A.

### File produced by an editor is rejected

Inspect its actual encoding. 8/24-bit PCM, IEEE float, extensible WAV and multichannel audio are deliberately unsupported in 0.9.5-A. Also verify that `byteRate == sampleRate * blockAlign` and that the file does not contain duplicate required `fmt `/`data` chunks. Convert to canonical PCM16 mono/stereo rather than broadening the loader during acceptance.

### `truncated` on a file that appears playable elsewhere

Check the RIFF declared size, chunk lengths, required even-byte padding and trailing chunks. 0.9.5-A validates the full declared RIFF and fails closed when any declared chunk boundary exceeds the RIFF or physical file.

### `too-large`

This is admission, not decoder failure. The decoded mono PCM byte count exceeds the caller's current budget. Do not increase the 32 KiB sampler pool in 0.9.5-A.

### Hardware reset or WDT

Reject the candidate. Capture Serial output and the exact fixture name. Malformed input must return failure without heap corruption or reset.

## Acceptance checklist

### Exact-SHA software

- [ ] base is exactly `d3db4e48ebc08862bdaf9f62532414f009839192`;
- [ ] focused 32-fixture WAV regression corpus is green;
- [ ] full Core/host suite is green;
- [ ] SDL build is green;
- [ ] Cardputer ADV normal compile is green;
- [ ] fixed DRAM budget is green;
- [ ] Cardputer ADV SEQTRAK MIDI-only compile is green;
- [ ] no B/C/D/E kit/relink/memory-policy implementation is present.

### Loader contract

- [ ] inspect performs no PCM allocation/data-payload decode;
- [ ] RIFF traversal honors even-byte chunk padding;
- [ ] RIFF/chunk arithmetic is bounded against declared and physical file sizes;
- [ ] complete declared RIFF traversal rejects malformed trailing chunks;
- [ ] valid unknown chunks after required chunks remain accepted;
- [ ] physical bytes after the declared RIFF boundary remain ignored;
- [ ] duplicate required `fmt `/`data` chunks fail closed;
- [ ] only PCM16 mono/stereo is accepted;
- [ ] `blockAlign` and `byteRate` exactly match PCM16 channel/rate metadata;
- [ ] decoded mono byte count is known before allocation;
- [ ] mono uses one exact output allocation;
- [ ] stereo uses one exact mono output allocation plus bounded 512-byte stack scratch;
- [ ] multi-chunk stereo conversion is regression-tested across the scratch boundary;
- [ ] no full-stereo + full-mono transient pair exists;
- [ ] file metadata changes between inspect and decode fail closed;
- [ ] existing `loadWavFile*` compatibility entry points remain available.

### Cardputer ADV hardware

- [ ] exact candidate SHA flashed;
- [ ] normal boot/UI unchanged;
- [ ] `valid_mono.wav` loads/previews;
- [ ] `valid_stereo.wav` loads/previews as mono;
- [ ] `odd_junk.wav` loads/previews;
- [ ] `odd_list.wav` loads/previews;
- [ ] one malformed/truncated fixture fails without reset/WDT;
- [ ] sampler pool remains 32 KiB;
- [ ] sampler 8-voice smoke passes;
- [ ] pitch/reverse/loop/choke smoke passes;
- [ ] heap integrity remains OK;
- [ ] underruns show no systematic regression.

## Freeze boundary

After software and hardware acceptance, freeze 0.9.5-A before starting 0.9.5-B. B may consume the public inspection result and error contract, but must not reopen WAV parser scope.
