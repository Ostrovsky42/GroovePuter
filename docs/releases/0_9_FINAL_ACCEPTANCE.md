# GroovePuter 0.9 — Final Acceptance Record

## Purpose

Provide one copy-pasteable record for automated, Cardputer ADV, storage, synth, MIDI and runtime acceptance of draft PR #131 before merging into `dev_0.9` or tagging 0.9.

This document prepares the procedure; unchecked hardware items are not evidence of completion.

## Exact SHA

```text
release base: dev_0.9 @ b28c63801660c9d024e4aad57716d534744fa324
stabilization PR: #131
code + reconciled-manual parent: 7c73f7d9e0818eb4e7114b7ba7f3f847e7c57111
final candidate head: read from PR #131 after the last documentation commit
```

Because this file is itself part of the candidate commit, the immutable final head cannot self-reference its own SHA. Copy the exact PR head into the PR body and the test log immediately after the branch is frozen. All automated and hardware results must use that same head.

## Hardware

- M5Stack Cardputer ADV;
- USB-C data cable;
- FAT32 microSD with known-good project, Scene and MIDI fixtures;
- headphones;
- optional Yamaha SEQTRAK and the already validated USB data/host topology.

## Wiring

No external GPIO wiring is required.

Use the built-in display, keyboard, microSD slot and audio path. For SEQTRAK, keep power and USB topology unchanged throughout a comparison run.

## Build and flash

```bash
git fetch origin
git switch release/0.9-final-stabilization
git reset --hard origin/release/0.9-final-stabilization

git rev-parse HEAD

rm -rf build .pio .pioenvs .piolibdeps

bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Upload using the repository command for the detected serial device and monitor at `115200` baud.

## Automated results

Record workflow run links and conclusions for the exact final head.

| Gate | Result | Evidence |
|---|---|---|
| Release 0.9 focused TB303/DST contract | PENDING FINAL HEAD | PR #131 Actions |
| Core host regressions | PENDING FINAL HEAD | PR #131 Actions |
| Pattern paging/storage/address | PENDING FINAL HEAD | Core host job |
| UI status chrome | PENDING FINAL HEAD | Core host job |
| Synth engine tests | PENDING FINAL HEAD | Core host job |
| SDL build | PENDING FINAL HEAD | Core workflow |
| Cardputer ADV build | PENDING FINAL HEAD | Core workflow |
| Normal fixed DRAM | PENDING FINAL HEAD | Core workflow |
| SEQTRAK MIDI-only build/DRAM | PENDING FINAL HEAD | Core workflow |
| Four-axis UI | PENDING FINAL HEAD | repository workflow |
| Phrase Core | PENDING FINAL HEAD | repository workflow |

Known completed evidence before final freeze:

- focused TB303/DST workflow passed on successive implementation heads;
- `tests/run_host_tests.sh` passed in the focused workflow;
- project storage test passed after the bounded copy-loop fix;
- SDL passed on the implementation head;
- later exact-head results supersede these observations.

## Storage smoke

1. Open project `alpha`; edit Synth A at page 2, bank B, slot 7; Save.
2. Open project `beta`; confirm the same address is empty.
3. Return to `alpha`; confirm the pattern remains.
4. Save As to `alpha-copy`; confirm all pages are copied.
5. Clear `alpha-copy`; reboot immediately.
6. Confirm `alpha-copy` stays empty and `alpha` remains intact.
7. Check the canonical address on Synth A, Synth B and Drums.

Expected SD ownership:

```text
/patterns/alpha/page_01.gpp
/patterns/alpha-copy/page_01.gpp
```

## Synth persistence

For Synth A and Synth B, repeat for:

```text
TB303
SID
AY
SH101
SN76489
WAVEMORPH
```

1. Select TYPE.
2. Change every visible parameter, including parameter 5 where present.
3. Record values.
4. Save.
5. Reboot.
6. Load.
7. Compare TYPE and parameters `0..5`.
8. Confirm genre did not replace the patch.

**Current release gate:** this section remains blocked until versioned synth persistence and normal-load patch ownership have host round-trip, legacy and failed-load tests.

## TB303 envelope

For Synth A and Synth B:

- ordinary trigger;
- accent;
- slide between held notes;
- sustain for two seconds;
- NoteOff and listen for two seconds;
- repeated trigger;
- Volume minimum/middle/maximum;
- sub off/on;
- Panic/AllNotesOff.

Expected:

- no full-level onset or release click;
- slide remains connected without a full retrigger;
- release reaches silence;
- Volume changes level monotonically;
- sub changes the low component without sounding doubled;
- Panic leaves no active voice.

## AY/SN pitch

Run the complete checklist from:

```text
docs/tests/SYNTH_PITCH_NOTE_LIFECYCLE_CARDPUTER_ADV.md
```

Confirm chromatic steps, octave behavior and playable register policy on both voices.

## NoteOff

- play out-of-range notes through every supported live path;
- release the original logical note;
- mute during playback;
- change route and target;
- Stop and Panic.

Expected: the clamped physical note always receives its matching cleanup and no internal or external note remains stuck.

## Genre regression

Using the same project data and seed/reference actions as the accepted `dev_0.9` build, compare:

- Acid;
- Rave;
- Techno;
- current Minimal.

Expected:

- no genre-owned variant list or forced Atlas role from rejected PR #110;
- no hidden materialization during PROFILE ONLY or browse;
- no unexpected TYPE/patch replacement after Scene Load;
- observed accepted patterns and sound character remain unchanged except for the intentional TB303 onset/release correction.

## MIDI/SEQTRAK

Smoke:

- Start;
- Stop;
- Continue;
- live keyboard;
- performance arpeggiator;
- SMF playback;
- `U` mute mixer;
- route change;
- page change;
- Panic.

Expected:

- no end-of-bar stall growth;
- no stuck internal or external notes;
- RAW and SEQTRAK-safe routing retain their documented ownership;
- unverified direct `1..9` mute shortcuts are not required for acceptance; use the `U` mixer.

## Runtime soak

Run for at least 30 minutes while alternating:

- playback;
- page switching;
- Save/Load;
- pattern page/bank/slot switching;
- synth TYPE switching;
- dense MIDI;
- SEQTRAK output when connected.

Record:

```text
boot free heap:
minimum free heap:
largest free block:
loop stack high-water:
audio stack high-water:
underruns:
queue overflows:
watchdog/reset:
monotonic memory loss:
```

## Expected behavior

- boot and navigation remain responsive;
- storage namespaces remain isolated;
- Scene Load restores stored ownership rather than genre defaults;
- every advertised synth is chromatic over its supported range;
- TB303 has bounded onset/release behavior;
- DST does not collapse output toward silence;
- no accepted genre pattern changes unexpectedly;
- Stop, mute, route change and Panic clean up notes;
- no watchdog, Guru Meditation, allocation failure or growing memory loss occurs.

## Troubleshooting

### TYPE or parameter 5 changes after reboot

Stop acceptance. Capture engine, voice, values before/after, Scene JSON/version, exact SHA and serial log. Do not compensate by applying Genre again.

### TB303 clicks or release never reaches silence

Record trigger type, accent/slide state, Volume, sub state and an audio clip. Compare normal trigger with legato slide separately.

### Project pages cross-contaminate

Capture both project names, encoded SD folders, page address and filesystem listing. Do not continue unrelated acceptance.

### Cardputer compile reports duplicate type definitions

Check explicit include guards on aliased Arduino headers, especially `src/audio/pattern_paging.h`. Do not remove required production includes to make the build pass.

### MIDI note remains stuck

Record source, logical note, physical/clamped note, route, target and cleanup action. Use Panic only after capturing the sequence.

## Acceptance checklist

### Automated

- [ ] exact final PR head recorded in PR body and test log;
- [ ] focused TB303/DST contract passes;
- [ ] all host/source regressions pass;
- [ ] storage/address/status tests pass;
- [ ] SDL builds;
- [ ] Cardputer ADV normal build and DRAM gate pass;
- [ ] SEQTRAK MIDI-only build and DRAM gate pass;
- [ ] Four-axis UI passes;
- [ ] Phrase Core passes where available.

### Hardware

- [ ] storage smoke passes;
- [ ] all six engines preserve TYPE and parameters `0..5` on A and B;
- [ ] normal Load does not apply genre timbre over the stored patch;
- [ ] TB303 envelope/Volume/sub/Panic pass;
- [ ] AY/SN pitch and logical NoteOff pass;
- [ ] neutral defaults do not start AY/SH101/SN/WAVEMORPH at maximum noise;
- [ ] DST remains audible and stable on A and B;
- [ ] Acid/Rave/Techno/current Minimal regression passes;
- [ ] MIDI/SEQTRAK smoke passes;
- [ ] 30-minute soak and telemetry record pass.

## Known deferred items

- Song/Generation UX redesign;
- TEXTURE/GENERATION page ownership cleanup;
- Tape/Sampler UI restoration;
- Phrase Arranger Stage 2;
- new genres and Atlas material;
- broad dead-code cleanup;
- realtime filter allocation redesign unless separately proven low-risk;
- DC blocker, broad loudness, aliasing, mipmap or oversampling work;
- direct numeric MIDI mute shortcuts until separately accepted.

## Release decision

Required pre-hardware wording after all code/automated blockers are actually closed:

```text
Code complete and automated-build verified.
Cardputer ADV listening, persistence and runtime acceptance pending.
Do not merge or tag 0.9 yet.
```

At the current draft state, synth persistence and normal-load patch ownership are still code blockers, so that wording must not yet be used as the PR status.
