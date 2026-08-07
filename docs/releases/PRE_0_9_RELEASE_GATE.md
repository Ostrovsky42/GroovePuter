# GroovePuter 0.9 — Pre-release Gate

## Purpose

Define the reproducible release path for `dev_0.9`, separate code completion from automated verification and keep Cardputer ADV acceptance explicit.

A mergeable PR or a source-only regression is not release evidence. Every automated result must belong to the exact final PR head, and every hardware checkbox remains unchecked until the owner records the result.

## Release baseline

```text
release branch: dev_0.9
base SHA: b28c63801660c9d024e4aad57716d534744fa324
stabilization PR: #131
stabilization branch: release/0.9-final-stabilization
```

The final candidate SHA is not frozen while PR #131 is draft. Record the exact final PR head in `docs/releases/0_9_FINAL_ACCEPTANCE.md` and the PR body immediately before hardware testing.

## Integrated release inputs

### PR #102 — project-scoped pattern storage

Merged into `dev_0.9`. Preserve:

- project-owned pattern-page namespaces;
- Save As page copying;
- New and Clear isolation;
- legacy root-page migration;
- CRC, `.tmp` and `.bak` recovery;
- canonical `page + bank + slot` identity;
- `.gpp` format version 3.

Only the post-merge Cardputer ADV smoke and final evidence record remain.

### PR #125 — synth pitch and note lifecycle

Merged into `dev_0.9`. Automated source/host coverage exists for:

- AY PSG clock and chromatic range;
- SN76489 playable-register octave folding;
- SN `Oct+` behavior;
- original-note NoteOff matching after NoteOn clamp;
- aliased Arduino include guards used by the affected path.

Cardputer ADV listening remains pending.

### PR #110 — rejected experiment

Closed and not a release input. Do not port genre-owned variant lists, forced Atlas P1/P2/P3 materialization, sparse Dub/Trip-Hop repair, Synthwave/Deep Stab renaming, or any other behavior from #110.

Acid, Rave, Techno and the current Minimal behavior in `dev_0.9` are the release reference.

## PR #131 status

### Code complete in the current draft

- TB303 has a separate attack/decay/sustain/release amplitude lifecycle.
- NoteOff starts a bounded release and reaches silence.
- legato slide does not fully retrigger the amplitude envelope.
- TB303 `Volume` controls output once.
- the optional TB303 sub layer is mixed once.
- distortion enable restores an audible drive only when the stored drive is below the working threshold.
- project-page copy uses a bounded file-size loop instead of an ambiguous EOF/`available()` loop.
- `PatternPagingService` has an explicit include guard for Arduino aliased paths.

### Automated verified

The focused PR workflow has passed on successive exact heads. It compiles the real TB303 and distortion sources with `-Wall -Wextra -Werror`, runs the numeric lifecycle contract, and runs `tests/run_host_tests.sh`.

The full Core workflow must be green again on the final documentation head before hardware testing. Earlier failures exposed and led to the pattern-page copy and include-guard fixes; no assertion was removed.

### Release blockers still open

- **P0-1 — versioned synth persistence:** TYPE plus normalized parameters `0..5`, legacy decode, engine-specific param-5 defaults and malformed-version transaction safety are not proven.
- **P0-2 — Scene load ownership:** normal load must guarantee that stored TYPE and stored parameters win over genre timbre application.
- **P1-1 — engine-aware neutral legacy defaults:** must be implemented with the versioned decode so saved user patches are not overwritten.
- **P1-3 — mutation revision evidence:** FEEL/TEXTURE/GENRE commit, preview, failed-transaction, Save and recovery behavior still need one focused evidence matrix.

PR #131 must remain draft while these blockers are open.

## Hardware

- M5Stack Cardputer ADV.
- USB-C data cable.
- FAT32 microSD with known-good Scene, pattern and MIDI fixtures.
- Headphones for clicks, release tails, noise and level comparisons.
- Optional Yamaha SEQTRAK for clock, transport and MIDI recording smoke.

## Wiring

No GPIO wiring is required for the release gate. Use the built-in display, keyboard, speaker/headphone output and microSD slot.

For SEQTRAK, use the already validated powered USB data topology and do not change topology during a comparison run.

## Build and flash

```bash
git fetch origin
git switch release/0.9-final-stabilization
git reset --hard origin/release/0.9-final-stabilization

rm -rf build .pio .pioenvs .piolibdeps

bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Required GitHub Actions for the exact final head:

- Core host regressions;
- SDL build;
- Cardputer ADV build;
- normal fixed-DRAM gate;
- Cardputer ADV SEQTRAK MIDI-only build and its DRAM check;
- Four-axis UI;
- Phrase Core where available;
- Release 0.9 stabilization focused contracts.

Flash with the repository upload command and monitor serial at `115200` baud.

## Expected behavior

A release candidate must:

- boot without a black screen or recovery loop;
- preserve project boundaries and the full pattern address;
- preserve Synth A/B TYPE and every visible parameter after Save, reboot and Load;
- load a Scene without hidden genre replacement of its patch;
- play every advertised synth without stuck notes, gross pitch collapse or unintended maximum noise;
- give TB303 a click-safe onset and bounded release;
- enable DST without a near-silent level collapse;
- keep accepted Acid, Rave, Techno and current Minimal output unchanged;
- stop internal and external notes cleanly;
- survive the documented 30-minute soak without monotonic memory loss.

## Troubleshooting

### Project pages copy but Save As reports failure

Confirm the candidate includes the bounded file-size copy loop in `src/audio/pattern_paging.cpp`. Capture source/target project names, SD paths and serial output.

### Cardputer build reports `PatternPagingService` redefinition

Confirm `src/audio/pattern_paging.h` uses the explicit `GROOVEPUTER_SRC_AUDIO_PATTERN_PAGING_H_` include guard. Do not solve this by removing required includes.

### A synth TYPE or sixth parameter changes after Load

Stop release acceptance. Record engine, voice A/B, all parameter values, Scene name, exact SHA and serial log. This is P0-1/P0-2, not a listening preference.

### A genre changes after browsing without Apply

Capture selected genre/variant, Apply mode, dirty revision before/after and pattern hashes. Browse/preview must not commit Scene state.

## Deferred beyond 0.9

- Song/Generation redesign and PR #101;
- Phrase Arranger Stage 2 and PR #90;
- TEXTURE/GENERATION page removal or navigation rewrite;
- Tape/Sampler UI restoration;
- new genres or Atlas material;
- realtime filter-allocation redesign unless separately proven low-risk;
- per-voice DC blockers without DRAM and listening evidence;
- broad dead-code deletion without linker/reachability proof;
- broad loudness, aliasing or oversampling work.

## Final checklist

### Code and automated

- [ ] Exact final PR head is recorded.
- [ ] P0-1 versioned synth persistence is complete and backward-compatible.
- [ ] P0-2 stored patch ownership is complete.
- [ ] P1-1 neutral engine defaults are covered.
- [ ] P1-3 mutation/recovery matrix passes.
- [ ] Core host regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV build and normal DRAM gate pass.
- [ ] SEQTRAK MIDI-only build and DRAM gate pass.
- [ ] Four-axis UI and Phrase Core pass.

### Hardware

- [ ] #102 storage smoke passes.
- [ ] all six synth engines preserve TYPE and parameters `0..5` on A and B.
- [ ] TB303 trigger/accent/slide/NoteOff/release/Volume/sub/Panic pass.
- [ ] #125 AY/SN pitch and NoteOff checklist passes.
- [ ] neutral defaults and DST listening pass.
- [ ] accepted genre behavior is unchanged.
- [ ] MIDI/SEQTRAK lifecycle smoke passes.
- [ ] 30-minute soak and telemetry record pass.

Do not merge PR #131 or tag 0.9 until every applicable code, automated and hardware item is complete.
