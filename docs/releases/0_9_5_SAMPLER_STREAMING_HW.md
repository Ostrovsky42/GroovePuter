# 0.9.5 Sampler Streaming — Cardputer ADV Hardware Acceptance

## Purpose

Validate the first bounded-DRAM sampler playback candidate on a real M5Stack Cardputer ADV.

## Current hardware status — P0 FAIL / Gate 0 only

The first hardware run exposed a bounded-memory ownership failure before normal playback acceptance:

```text
3 ordinary streamed pad assignments
free8    ~4 KiB
largest8 ~1 KiB
```

Stop the old acceptance sequence here. Do **not** continue to four/eight pads, reverse, pitch, loop or soak until Gate 0 below passes.

The code audit found a concrete lifetime bug in the previous candidate: `preloadStreamed_()` prewarmed page 0 through `SD.open()`, while the resulting file handle remained owned by the four-entry stream I/O pool even though the sample was only assigned and no sampler voice referenced it. That made assignment count grow the long-lived heap working set.

The corrected contract is:

- pad assignment owns a bounded descriptor only;
- the fixed shared 4096-byte page cache is allocated once at boot;
- the temporary file handle used to prewarm page 0 is closed before assignment completes;
- file handles may exist during active streamed playback only;
- the control loop closes a sample file after its last voice reference is released;
- reducing the 4096-byte cache is not an acceptable substitute for fixing ownership.

This document is intentionally not self-pinned to a commit SHA because changing a tracked file changes the commit SHA. Before flashing, use the exact `head_sha` recorded in PR #304 and keep all hardware measurements on that one SHA.

This candidate keeps the recovered sampler surface intact:

- 16 internal sampler pads; the recovered UI exposes pads 1..8 used by the drum sequencer;
- 8 logical sampler voices;
- SampleRef/Scene persistence;
- pitch, reverse, loop, start/end, choke and velocity semantics;
- nested sample-folder browser from 0.9.5-A;
- mono/stereo PCM16 WAV input.

The change under test is playback storage, not sampler product semantics:

- decoded WAV <= 2048 bytes: resident PCM fast path;
- larger WAV: streamed source descriptor;
- fixed 4096-byte internal-RAM cache: 8 pages x 512 bytes;
- bounded 16-request SPSC queue;
- up to four control-side file-handle slots for **active playback**, not assigned pads;
- two-page boundary-aware prefetch per active streamed voice;
- SD open/seek/read and stereo-to-mono conversion run only from the control loop;
- the audio path reads READY cache pages only and never opens SD, allocates, frees or locks a filesystem mutex.

This is a hardware research candidate. Do not merge it as production until the acceptance checklist below is recorded on the exact candidate SHA.

## Hardware list

- M5Stack Cardputer ADV / Stamp-S3A, DRAM-only build (`PSRAM=disabled`).
- microSD card formatted for the existing GroovePuter SD stack.
- USB-C data cable for flash + serial monitor.
- headphones/speaker from the normal GroovePuter audio output path.
- Optional: Yamaha SEQTRAK only for the existing MIDI integration regression; it is not required for sampler streaming acceptance.

## Wiring

No new GPIO wiring is required.

- Insert microSD into Cardputer ADV.
- Connect Cardputer ADV USB-C to the development computer.
- Use the normal Cardputer audio output arrangement already used for GroovePuter hardware tests.

The sampler test does not use PORT.A I2C.

## SD layout

Use nested folders. The boot registry first tries `/sd/samples` and then `/samples`.

A practical test card:

```text
/samples/
  909/
    Kick/
      BD01.wav
      BD02.wav
    Snare/
      SD01.wav
  SP12/
    Hats/
      CH01.wav
      OH01.wav
  Stereo/
    stereo_test.wav
  Long/
    long_one_shot.wav
```

Use PCM16 WAV files. Mono and stereo are accepted. Stereo is downmixed page-by-page; the full decoded file is not materialized first.

For the first pass, include at least:

- three ordinary streamed WAVs > 2048 decoded bytes for Gate 0;
- one mono one-shot > 2 KiB decoded;
- one stereo one-shot > 2 KiB decoded;
- one longer WAV large enough that the old resident-only path could not obtain one contiguous decoded block after UI/catalog initialization;
- samples in at least three nested directories.

## Build / flash

From the exact candidate branch:

```bash
git checkout agent/20260816-0.9.5-sampler-streaming-hw
git pull --ff-only
git rev-parse HEAD
bash scripts/build.sh
bash scripts/upload.sh /dev/ttyACM0
```

The SHA printed by `git rev-parse HEAD` must equal the current PR #304 `head_sha`. Do not copy a SHA from this tracked document.

If the serial port differs, pass the actual `/dev/ttyACM*` device to `scripts/upload.sh`.

The build uses the repository's Cardputer ADV DRAM-only FQBN:

```text
m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc
```

Open a serial monitor at the same rate used by the existing GroovePuter Cardputer test workflow.

## Folder browser controls

Open the recovered `SAMPLES` page and focus `SAMPLE`.

- `Enter` on SAMPLE: open `SAMPLE BROWSER`.
- `Up` / `Down`: move selection.
- `Enter` or `Right` on a folder: enter the folder.
- `Enter` or `Right` on a WAV: assign it to the current sampler pad.
- `Left` or `Backspace`: go to parent directory.
- `Esc`: close browser without changing the pad.
- Pad keys exposed by the recovered sampler UI/sequencer remain `qwertyui` for pads 1..8.

The browser is backed by the already-created session SampleIndex; navigating folders must not rescan the SD card.

## Expected boot / serial behavior

Before catalog scan, the fixed stream cache must be reserved successfully:

```text
[SAMPLER-REGISTRY] heap phase=before-stream-cache free8=... largest8=...
[SAMPLER-STREAM] fixed cache ready pages=8 pageBytes=512 total=4096
[SAMPLER-REGISTRY] heap phase=after-stream-cache free8=... largest8=...
[SAMPLER-REGISTRY] heap phase=before-scan free8=... largest8=...
[SAMPLER-REGISTRY] heap phase=after-scan free8=... largest8=...
[SAMPLER-REGISTRY] heap phase=after-bind free8=... largest8=...
[SAMPLER-REGISTRY] ready discovered=... registered=... stableReject=... legacyReject=... storeReject=...
```

`fixed cache allocation failed` is a hard failure for this candidate.

Selecting an ordinary one-shot must explicitly show that routing uses decoded bytes, not the browser's rounded file-size label:

```text
[SAMPLER-ROUTE] id=... decodedBytes=... sourceBytes=... route=STREAMED path=...
[SAMPLER-STREAM-IO] open id=... slot=...
[SAMPLER-STREAM-IO] close id=... slot=...
[SAMPLER-STREAM] prepared id=... frames=... sr=... channels=...
```

For an **assigned but idle** streamed sample, the `open` produced by page-0 prewarm must be paired with `close` before assignment completes. PAD2/PAD3 must not leave additional file handles open.

Very small files may intentionally use:

```text
[SAMPLER-ROUTE] ... route=RESIDENT ...
[SAMPLER] resident id=... frames=... bytes=... pool=.../...
```

While streamed samples play, the control loop periodically emits:

```text
[SAMPLER-STREAM] active=... hit=... miss=... pages=... reqDrop=... starve=... voiceDrop=... sdMaxUs=...
```

During active playback, `SAMPLER-STREAM-IO open` is expected. After the voice releases its handle, the control loop must emit the matching `close`; the file object must not remain as idle assignment state.

Interpretation:

- `pages` must rise as playback crosses cache-page boundaries;
- `hit` should dominate once prefetch is warm;
- occasional `miss`/`starve` is diagnostic, not automatically a failure;
- `reqDrop` should remain 0 in the normal 1/2/4-stream acceptance passes;
- `voiceDrop` must remain 0 in the normal acceptance passes;
- `sdMaxUs` records the worst control-side page read/decode time seen so far.

## Expected screen behavior

1. `SAMPLES` page opens normally.
2. `SAMPLE BROWSER` shows folders and WAV files instead of one flat basename list.
3. Entering nested folders changes the displayed relative path.
4. Selecting a WAV returns to the sampler page and the assigned relative filename is shown on the pad.
5. The selected pad can be auditioned and triggered by the existing drum sequencer.
6. Changing pad, pitch, reverse, loop, start/end and choke remains available as before.
7. Leaving and reopening `SAMPLES` must not rebuild the catalog or cause a permanent heap step-down attributable to PCM loading or retained idle file handles.

## Test sequence

### Gate 0. Assignment lifetime — mandatory before all audio acceptance

Use three ordinary WAVs whose serial routing reports `route=STREAMED`.

Record the existing RAM diagnostic after each step:

```text
phase                         free8      largest8
BOOT / before SAMPLES         _____      _____
SAMPLES / before assign       _____      _____
PAD1 assigned                 _____      _____
PAD2 assigned                 _____      _____
PAD3 assigned                 _____      _____
PAD1 replace                  _____      _____
PAD1 replace x10              _____      _____
leave SAMPLES                 _____      _____
re-enter SAMPLES              _____      _____
```

For every assignment, capture these serial lines:

```text
[SAMPLER-ROUTE] ... decodedBytes=... route=STREAMED ...
[SAMPLER-STREAM-IO] open ...
[SAMPLER-STREAM-IO] close ...
[SAMPLER-STREAM] prepared ...
```

**PASS:**

- all three ordinary samples report `route=STREAMED`;
- each idle assignment prewarm has a matching open/close pair;
- PAD1 -> PAD2 -> PAD3 does not consume kilobytes per pad;
- replacing PAD1 ten times does not produce monotonic free-heap or largest-block collapse;
- leaving/re-entering SAMPLES does not introduce another permanent step-down;
- no WDT/reset/heap corruption.

**FAIL / stop immediately:**

- any ordinary >2048 decoded-byte sample reports `route=RESIDENT`;
- a prewarm file handle remains open merely because a pad is assigned;
- free8/largest8 again collapses approximately per assignment;
- repeated replacement causes monotonic loss.

Do not continue to A-H unless Gate 0 passes.

### A. Boot topology

Record raw bytes for every available existing boot heap marker plus the new sampler markers:

```text
phase                          free8      largest8
before-stream-cache            _____      _____
after-stream-cache             _____      _____
before-scan                    _____      _____
after-scan                     _____      _____
after-bind                     _____      _____
after first UI draw            _____      _____
```

The expected fixed-cache delta is approximately 4096 bytes plus allocator metadata. The important property is that the cache is allocated before the catalog fragments the heap.

### B. Folder selection

- Open SAMPLES.
- Browse root -> `909` -> `Kick`.
- Assign `BD01.wav` to pad 1.
- Browse root -> `SP12` -> `Hats`.
- Assign a hat to pad 2.
- Browse root -> `Stereo`.
- Assign `stereo_test.wav` to pad 3.
- Verify the old assignment remains intact when selecting a malformed/unsupported WAV, if such a negative fixture is present.

### C. Single streamed voice

Trigger pad 1 repeatedly and from the sequencer.

Pass criteria:

- audible sample completes;
- no WDT/reset;
- no global audio stall caused by SD page reads during playback;
- `reqDrop=0`;
- `voiceDrop=0`;
- stream I/O handle closes after the voice is no longer active.

### D. Two streamed voices

Use two overlapping samples, for example kick tail + open hat or two longer one-shots.

Pass criteria are the same as C. Record `sdMaxUs`, `miss`, `starve` and `pages`.

### E. Four streamed voices

Create a deliberate four-way overlap.

This is the V1 cache design target: 8 physical pages with a two-page prefetch horizon and up to four active-playback file handles.

Pass criteria:

- no WDT/reset;
- no corruption/cross-sample audio;
- `reqDrop=0`;
- `voiceDrop=0`;
- any starvation is local and recoverable, not a whole-engine stall;
- after voices finish, idle file-handle ownership returns to zero.

The sampler still has 8 logical voices. This test does not redefine the product polyphony to four; it measures whether the first 4 KiB streaming cache is sufficient before tuning cache size/page count on hardware evidence.

### F. Reverse / pitch / loop

On a streamed sample:

- reverse ON;
- pitch 0.5, 1.0 and 2.0;
- loop ON with a valid start/end range.

Pass criteria:

- no crash;
- direction and pitch remain correct;
- loop does not trigger filesystem work in the audio callback;
- no persistent `voiceDrop` under the normal single-voice case.

### G. Sequencer + persistence

- Assign samples to several pads.
- Program drum-sequencer triggers.
- Save Scene.
- Reboot.
- Load Scene.

Pass criteria:

- stable SampleRef resolves the same files;
- pad assignments are restored;
- sequencer triggers the restored samples;
- duplicate basenames in different folders remain distinct.

### H. Browse / replay soak

For at least 30 minutes:

- move through several folders;
- reassign pads;
- start/stop transport;
- repeatedly trigger one/two/four streamed voices;
- open/close SAMPLES repeatedly.

Record beginning/end `free8` and `largest8` from available diagnostics.

Pass criteria:

- no monotonic heap collapse caused by sample PCM or retained idle file handles;
- no WDT/reset;
- no sample-ID/path cross-talk;
- no growing `reqDrop`/`voiceDrop` during ordinary one/two-voice use.

## Troubleshooting

### Browser is empty

Check serial for:

```text
[SAMPLER-REGISTRY] ready discovered=0 ...
```

Confirm the card contains `/samples/...` or the supported `/sd/samples/...` root and PCM16 WAV files.

### `fixed cache allocation failed`

Do not continue streaming acceptance. Capture all boot heap markers. The cache is intentionally reserved before catalog scan; failure here means the candidate does not have a reproducible fixed playback budget on that build.

### Assignment reports `route=RESIDENT` for a normal WAV

Use the serial `decodedBytes` value, not the rounded KB shown by the browser. The routing threshold is 2048 **decoded mono PCM bytes**. If `decodedBytes > 2048` and the route is RESIDENT, stop and report it as a routing bug.

### Assignment opens a stream file but does not close it

Do not continue playback testing. An assigned, idle streamed source must retain only its descriptor plus pages already inside the one fixed cache. A retained `SD.open()` object is a lifetime bug, not a reason to reduce cache size.

### WAV assignment is rejected

Look for:

```text
[SAMPLER] sample assignment rejected: ...
Preload: WAV inspection failed ...
[SAMPLER-STREAM] initial page failed ...
```

Keep the rejected file and serial log as a fixture. The previous pad assignment must remain unchanged.

### Audio has brief holes but system does not reset

Capture the nearest stream-stat line. The useful fields are `miss`, `starve`, `reqDrop`, `voiceDrop` and `sdMaxUs`. A page miss is expected to degrade only the affected sampler voice to silence while its source position is held; it must not block the global audio task.

### Four-way overlap starves

Do not increase the cache blindly. Record the counters and `sdMaxUs` first. The next tuning choice is evidence-driven: page size, prefetch horizon, cache page count or active streamed-voice policy.

### Selection while transport is running causes a visible/audible pause

Record it separately from playback-stream starvation. File inspection and the first page are control-side operations; this candidate's primary hard realtime contract is that ongoing streamed playback never performs SD I/O inside the audio callback. Selection-time mutation-gate behavior can then be isolated without confusing it with page-cache performance.

## Acceptance checklist

Hardware acceptance is PASS only when all checked items refer to one exact SHA and one SD card image:

- [ ] **Gate 0 PASS:** PAD1/PAD2/PAD3 streamed assignment does not consume kilobytes per pad.
- [ ] **Gate 0 PASS:** each idle assignment prewarm logs matching stream-I/O open + close.
- [ ] **Gate 0 PASS:** PAD1 replacement x10 does not cause monotonic free8/largest8 collapse.
- [ ] Cardputer ADV DRAM-only firmware builds and flashes.
- [ ] Boot reaches `setup() complete` without WDT/reset.
- [ ] Fixed 4096-byte sampler cache reports ready before catalog scan.
- [ ] `free8` + `largest8` captured before/after cache, catalog and UI.
- [ ] Nested folder browser works through at least three directories.
- [ ] Mono PCM16 streamed WAV assigns and plays.
- [ ] Stereo PCM16 streamed WAV assigns, downmixes and plays.
- [ ] A WAV that previously could not obtain one large decoded PCM block now plays through streaming.
- [ ] Pad 1..8 assignments remain usable from the drum sequencer.
- [ ] One streamed voice: `reqDrop=0`, `voiceDrop=0`.
- [ ] Two overlapping streamed voices: `reqDrop=0`, `voiceDrop=0`.
- [ ] Four overlapping streamed voices: no reset/corruption; counters recorded.
- [ ] Active stream I/O handles are reaped after voices finish.
- [ ] Reverse works on streamed WAV.
- [ ] Pitch 0.5 / 1.0 / 2.0 works on streamed WAV.
- [ ] Loop/start/end work on streamed WAV.
- [ ] Choke behavior remains correct.
- [ ] Scene Save -> reboot -> Load restores stable sample assignments.
- [ ] Duplicate basenames in different folders resolve correctly.
- [ ] 30-minute browse/replay soak has no WDT/reset or monotonic heap loss.
- [ ] Final serial log includes stream counters and worst `sdMaxUs`.

## Decision after hardware run

Do not tune several dimensions at once.

- If Gate 0 still fails: stop and continue ownership/allocator investigation; do not shrink the stream cache to hide the failure.
- If Gate 0 passes and streamed playback is clean but idle heap remains too low: proceed to bounded catalog/window work.
- If `sdMaxUs` is high and starvation occurs before catalog pressure matters: tune streaming page/cache policy first.
- If only four-way overlap fails while one/two voices are clean: keep 8 logical voices and benchmark a larger fixed cache before changing polyphony semantics.
- If ordinary one-voice streaming fails: stop; do not proceed to catalog optimization until the control-side refill path is corrected.
