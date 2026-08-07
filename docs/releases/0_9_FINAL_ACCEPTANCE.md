# GroovePuter 0.9 — Final Acceptance Record

## Purpose

Record automated and owner-run Cardputer ADV evidence for PR #131, the final 0.9 release candidate after merged synth persistence/load ownership (#143).

## Exact SHA

```text
release base: dev_0.9 @ 0afd24fe5f4b0b2d549214abe2ed1a7eb7f3448c
candidate branch: release/0.9-final-stabilization
candidate head: record from PR #131 after the final release-scope commit
```

All final results must belong to the same final head.

## Included correctness work

The candidate contains the release baseline plus:

- versioned Synth A/B TYPE + normalized `params[0..5]` persistence from #143;
- transactional malformed-version handling and legacy TB303 raw decode;
- engine-native defaults for legacy non-TB scenes missing patch data;
- normal Scene Load ownership: saved patch wins over Genre;
- explicit Save/Load/recovery revision-result semantics;
- TB303 amplitude lifecycle and bounded release;
- TB303 Volume/sub output ownership;
- distortion drive restoration;
- FEEL/Genre mutation revision boundaries.

## Critical-path policy

- #132 is post-0.9.
- #142 is post-0.9.
- #139 is optional only after a clean Cardputer ADV SID listening smoke.
- #134 may enter before RC only if integration onto the #143 Scene baseline is low-risk and combined legacy Scene regression is green; otherwise it is post-0.9.

## Hardware

- M5Stack Cardputer ADV;
- USB-C data cable;
- FAT32 microSD with known-good Scene, pattern and MIDI fixtures;
- headphones;
- optional Yamaha SEQTRAK with the already validated powered USB data topology.

## Wiring

No external GPIO wiring is required. Do not change USB/MIDI topology during comparison runs.

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

Flash with the repository upload command and monitor serial at `115200` baud.

## Automated results

| Gate | Result | Evidence |
|---|---|---|
| synth persistence/load ownership | PENDING FINAL #131 HEAD | Synth persistence workflow |
| TB303/DST focused contracts | PENDING FINAL #131 HEAD | Release workflow |
| FEEL/Genre revision ownership | PENDING FINAL #131 HEAD | Release workflow |
| Core host regressions | PENDING FINAL #131 HEAD | Core workflow |
| SDL build | PENDING FINAL #131 HEAD | Core workflow |
| Cardputer ADV build + DRAM | PENDING FINAL #131 HEAD | Core workflow |
| SEQTRAK MIDI-only build + DRAM | PENDING FINAL #131 HEAD | Core workflow |
| three-page Generate contract | PENDING FINAL #131 HEAD | Release workflow |
| Phrase Core | PENDING FINAL #131 HEAD | Release/Phrase workflow |

The #143 prerequisite was merged only after its exact head passed focused persistence, host, Phrase Core, SDL, Cardputer ADV + fixed DRAM and SEQTRAK MIDI-only gates.

## Storage smoke

1. In project `alpha`, edit Synth A address `2B7` and Save.
2. Open project `beta`; confirm `2B7` is empty.
3. Return to `alpha`; confirm the pattern remains.
4. Save As to `alpha-copy`; confirm pages were copied.
5. Clear `alpha-copy`; reboot immediately.
6. Confirm `alpha-copy` stays empty and `alpha` remains intact.
7. Verify addresses on Synth A, Synth B and Drums.

## Synth persistence

For Synth A and Synth B, repeat for `TB303`, `SID`, `AY`, `SH101`, `SN76489`, `WAVEMORPH`:

1. select TYPE;
2. change every visible parameter, including parameter 5 when exposed;
3. Save explicitly;
4. change TYPE and values to obviously different settings;
5. Load the saved Scene;
6. compare TYPE and values;
7. confirm Genre did not replace the patch before an explicit Apply/Materialize action.

Also verify:

- a legacy TB303 Scene preserves raw TB303 parameter meaning;
- a legacy AY/SH101/SN76489/WAVEMORPH Scene without synth parameters starts from sane engine-native defaults;
- a quick Save immediately after TYPE switch preserves the pending TYPE and its parameters consistently;
- failed Save remains dirty;
- failed Load leaves current Scene intact;
- recovered autosave does not falsely mark an explicit clean Save baseline.

## TB303 envelope

Test normal trigger, accent, held slide, two-second sustain, NoteOff, two-second release, repeated trigger, Volume min/mid/max, sub off/on and Panic.

Expected: no full-level click, connected slide, bounded release to silence, monotonic Volume response, no doubled sub, and no active voice after Panic.

## AY/SN pitch and NoteOff

Run [`docs/tests/SYNTH_PITCH_NOTE_LIFECYCLE_CARDPUTER_ADV.md`](../tests/SYNTH_PITCH_NOTE_LIFECYCLE_CARDPUTER_ADV.md).

Expected: chromatic steps, correct octave behavior, playable register folding and cleanup of the clamped physical note by releasing the original logical note.

## Genre regression

Compare Acid, Rave, Techno and current Minimal against the accepted `dev_0.9` reference using the same project data and actions.

Expected: no behavior from rejected #110, no hidden materialization during PROFILE ONLY/browse, and no TYPE/patch replacement after Load.

## Generate/FEEL regression

- workflow is `GENRE 1/3 → FEEL 2/3 → GENERATION 3/3`;
- TEXTURE page is absent;
- legacy page ID 8 resolves to FEEL;
- Genre/variant/morph and FEEL preset browsing do not dirty Scene;
- committed FEEL or Genre changes increment revision once.

## MIDI/SEQTRAK

Smoke Start, Stop, Continue, live keyboard, arpeggiator, SMF playback, `U` mute mixer, route change, page change and Panic.

Expected: no bar-boundary stall growth and no stuck internal or external notes. Direct numeric mute shortcuts are not required; use `U`.

## Runtime soak

Run for at least 30 minutes while alternating playback, page switching, Save/Load, pattern page/bank/slot switching, synth TYPE switching, dense MIDI and SEQTRAK output when connected.

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
- project namespaces remain isolated;
- Generate has three pages and TEXTURE is not restored;
- Scene Load restores stored synth ownership;
- every synth is usable over its supported range;
- TB303 onset/release is bounded;
- DST does not collapse level;
- Stop, mute, route change and Panic clean notes;
- no watchdog, Guru Meditation, allocation failure or monotonic memory loss occurs.

## Troubleshooting

### TYPE or parameter changes after Load

Stop acceptance. Record engine, voice, values before/after, Scene name, exact SHA and serial log.

### TB303 clicks or never reaches silence

Record trigger/accent/slide/Volume/sub state and an audio clip.

### Projects share pages

Record project names, encoded SD folders, address and directory listing; stop unrelated testing.

### TEXTURE appears in normal navigation

Stop acceptance. The release base removed it; legacy ID 8 must resolve to FEEL only.

### MIDI note remains stuck

Record source, logical note, physical/clamped note, route, target and cleanup action before Panic.

## Acceptance checklist

### Automated

- [ ] exact final #131 head recorded;
- [ ] synth persistence/load ownership focused contract passes;
- [ ] focused TB303/DST and revision contracts pass;
- [ ] all host/source regressions pass;
- [ ] SDL builds;
- [ ] Cardputer normal and SEQTRAK builds/DRAM gates pass;
- [ ] three-page Generate and Phrase Core pass.

### Hardware

- [ ] storage smoke passes;
- [ ] all engines preserve TYPE and parameters on A/B;
- [ ] normal Load does not apply genre timbre over the patch;
- [ ] legacy missing-patch defaults are sane;
- [ ] failed Save/Load and recovery revision behavior passes;
- [ ] TB303 envelope/Volume/sub/Panic pass;
- [ ] AY/SN pitch and NoteOff pass;
- [ ] neutral defaults and DST pass;
- [ ] genre regression passes;
- [ ] Generate/FEEL navigation and revision pass;
- [ ] MIDI/SEQTRAK smoke passes;
- [ ] 30-minute soak passes.

## Known deferred items

- #132 GenreManager ownership refactor;
- #142 AY articulation follow-up;
- #139 SID articulation unless hardware smoke explicitly promotes it;
- #134 TextureMode runtime removal if Scene migration integration is not demonstrably low-risk;
- Song/Generation UX redesign;
- Tape/Sampler UI;
- Phrase Arranger Stage 2;
- new genres or Atlas material;
- broad dead-code cleanup;
- realtime filter allocation redesign;
- DC blocker, broad loudness, aliasing, mipmap and oversampling work;
- direct numeric MIDI mute shortcuts.

Do not merge PR #131 or tag 0.9 until the exact final head passes the automated gate and required Cardputer ADV acceptance is recorded.
