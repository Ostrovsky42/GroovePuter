# 0.9.1 — P1/P2/P3 Production Selector

Status: `DRAFT / HARDWARE VERDICT PENDING`

Branch:

```text
agent/20260811-08-p-level-production-selector
```

Stacked base:

```text
agent/20260811-07-release-generation-routing
a531a6c74c671d2a97fa6bdc29b57b1a97699630
```

## Purpose

Expose the existing `RealizationLevel` contract as one production generation
request selector without adding another generator or changing the accepted
GENRE / RHYTHM / FEEL ownership model.

The levels keep their existing meanings:

```text
P1  CANONICAL       strongest identity / least transformation
P2  VARIATION       recognizable variation; compatibility default
P3  TRANSFORMATION  stronger fill/reduction/build/break behavior where allowed
```

`Alt+G` CHAOS is deliberately outside this selector. P3 is not CHAOS.

The selector is a device-session preference for the **next generation request**.
It is not Scene musical content. Generated patterns remain persisted normally in
Scene/project storage.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3;
- USB-C data cable;
- headphones or speaker normally used with GroovePuter.

No external accessory is required.

## Wiring

No external wiring.

PORT.A is untouched. Its existing I2C contract remains:

```text
SDA GPIO2
SCL GPIO1
```

## Controls

On `GENRE`, `FEEL`, and the main `DRUMS` grid:

```text
P             cycle P1 -> P2 -> P3 -> P1
```

Generation commands keep their routing from the preceding cleanup:

```text
GENRE G       full current Stage 15 groove at selected P-level
DRUMS G       drums only at selected P-level
Ctrl+Alt+G    Stage 12 1/2/4/8-bar audition at selected P-level
Ctrl+G        selected drum voice randomize; not a P-level command
Alt+G         CHAOS; deliberately outside P1/P2/P3
```

`I/O` legacy synth generation remains blocked on the release-facing GENERATE
screens. `O` remains blocked on the main DRUMS grid. `I` on DRUMS remains the
normal Q-I pattern-slot key.

## Persistence contract

The selector is stored in Cardputer NVS under its own compact device-session
key. It does not change the Scene JSON schema.

Compatibility behavior:

```text
no stored key       -> P2 VAR
invalid stored key  -> P2 VAR
valid P1/P2/P3      -> restored after reboot
```

This preserves the previous live-production behavior because the bridge was
hard-coded to P2 before this PR.

## Build / Flash

Run focused generation tests:

```bash
bash tests/run_generation_stage13_tests.sh
```

Run the full host matrix:

```bash
bash tests/run_host_tests.sh
```

Build firmware:

```bash
bash scripts/build.sh --warnings all
```

Run the normal Cardputer fixed-DRAM gate against the produced ELF using the
repository's existing DRAM-check script.

Flash the branch with the normal upload command, for example:

```bash
bash scripts/upload.sh /dev/ttyACM0
```

For phrase-probe validation, capture Serial at 115200 baud with the repository
serial monitor.

## Expected behavior

### Screen

Pressing `P` on GENRE, FEEL, or DRUMS must show one of:

```text
P1 CANON
P2 VAR
P3 TRANS
```

On GENRE the active status also exposes the current P-level. Changing the level
alone must not regenerate or erase the current pattern.

After `Ctrl+Alt+G`, the audition toast includes the selected level, for example:

```text
AUD 4B P2 VAR EVOLVED #413
AUD 4B P3 TRANS EVOLVED #413
```

A one-bar-only identity may still report `VARIATION` rather than `EVOLVED`; that
is the existing Stage 12 capability boundary, not a selector failure.

### Serial

The Cardputer phrase probe must include the selected level:

```text
[PHRASE-PROBE] status=... level=P2 VAR bars=4 ...
```

The existing stack, internal heap, largest-block and duration fields remain
present.

### Musical behavior

For the same GENRE / VARIANT / RHYTHM / FEEL and pattern address:

- P1 should preserve the clearest canonical rhythm identity;
- P2 should remain close to the current accepted Stage 14/15 behavior;
- P3 may transform more strongly where the vocabulary permits it;
- P1/P2/P3 must not silently switch to a different genre or incompatible rhythm;
- generated Synth A/B pitch still goes through the shared Stage 15 tonal path;
- Alt+G should remain recognizably more chaotic and separate from P3.

The three levels are not required to differ on every archetype. A vocabulary
entry may legally have less transformation headroom, but the selected level must
reach the same request owner and must never fall into legacy generated-pitch
routing.

## Quick hardware listening matrix

Use one fixed GENRE / RHYTHM selection at a time and repeat:

```text
P -> P1 CANON -> G -> listen
P -> P2 VAR   -> G -> listen
P -> P3 TRANS -> G -> listen
```

Minimum representative set:

```text
House / Techno     strong four-floor identity
Acid               bass + articulation relationship
UK Garage          syncopated / two-step identity
Drum & Bass        break-oriented identity
HipHop             backbeat identity
LoFi               sparse/slower material
```

Then set FEEL `REPEATS=4` and repeat P1/P2/P3 with `Ctrl+Alt+G` for UK Garage,
Drum & Bass and Electro. Preserve the complete `[PHRASE-PROBE]` line from at
least one P1, P2 and P3 run.

Finally choose `P3 TRANS`, reboot Cardputer, return to GENRE and verify the
selector still reports P3. Restore P2 afterward if you want the compatibility
default for subsequent tests.

## Troubleshooting

### P changes the pattern immediately

Fail. `P` is selector-only and must not materialize or clear anything. Generation
must still require `G` or `Ctrl+Alt+G`.

### P1/P2/P3 sounds identical

Retry with a transformation-capable rhythm, especially UK Garage, DnB or
Electro, and compare the same pattern address. Some archetypes have intentionally
limited transformation headroom.

### P3 sounds like Alt+G CHAOS

Fail if the route itself changed. `Alt+G` remains the separate legacy CHAOS
command and is not the implementation of P3.

### Level resets after reboot

Check the `gp-generation` / `p-level` NVS preference path. Missing or invalid
storage intentionally falls back to P2; a valid user-selected level must persist.

### Stage 12 audition says VARIATION

This can be expected for one-bar-only identities. It does not mean P-level was
ignored. Confirm the toast/Serial line reports the requested level.

## Acceptance checklist

```text
[ ] Focused Stage 13/14/15 generation matrix passes.
[ ] P-level request-state host test passes under GCC.
[ ] P-level request-state host test passes under Clang when available.
[ ] P-level request-state sanitizer run passes.
[ ] Full host regressions pass.
[ ] SDL build passes.
[ ] Cardputer ADV normal build passes.
[ ] Cardputer ADV fixed-DRAM gate passes.
[ ] Cardputer ADV SEQTRAK MIDI-only build passes.
[ ] Default with no stored preference is P2 VAR.
[ ] Invalid stored preference sanitizes to P2 VAR.
[ ] P cycles P1 -> P2 -> P3 -> P1 on GENRE.
[ ] P cycles the same shared selector on FEEL.
[ ] P cycles the same shared selector on DRUMS.
[ ] Pressing P alone does not mutate current patterns.
[ ] GENRE G consumes the selected P-level.
[ ] DRUMS G consumes the selected P-level and remains drums-only.
[ ] Ctrl+Alt+G consumes the selected P-level.
[ ] Phrase probe Serial line reports the selected P-level.
[ ] P1/P2/P3 keep the chosen genre/rhythm identity recognizable.
[ ] P2 remains compatible with the preceding accepted sound/behavior.
[ ] P3 remains distinct from Alt+G CHAOS routing.
[ ] Ctrl+G selected-voice behavior is unchanged.
[ ] Alt+G CHAOS behavior is unchanged.
[ ] Stage 15 remains the sole generated tonal projection path on strong routes.
[ ] Selected level survives a Cardputer reboot.
[ ] No Scene JSON schema field was added for P-level.
[ ] Hardware musical verdict is recorded on the exact tested SHA.
[ ] Three consecutive clean reviews are completed on one unchanged final SHA.
```
