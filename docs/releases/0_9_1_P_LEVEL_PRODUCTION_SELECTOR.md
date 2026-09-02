# 0.9.1 — P1/P2/P3 Production Selector

Status: `HARDWARE_ACCEPTED / STACKED_IN_#226`

Hardware-tested source head:

```text
agent/20260811-08-p-level-production-selector
6800e5a2c8f22a641feb816bdb26e70e892647f5
```

The tested implementation completed `3/3 CLEAN` and was squash-merged into the
#226 routing branch as:

```text
68e0cccb790cf500ceca1ab1c14b56de7994c9ff
```

Subsequent #226 changes after that squash are source-contract tests and release
/ architecture documentation only. They do not alter the hardware-tested
production P-level implementation. The complete 0.9.1 RC still requires one
final hardware/SEQTRAK/soak pass after #226 is merged into the Stage 15
integration line.

## Purpose

Expose the existing `RealizationLevel` contract as one production generation
request selector without adding another generator or changing the accepted
GENRE / RHYTHM / FEEL ownership model.

```text
P1  CANONICAL       strongest identity / least transformation
P2  VARIATION       recognizable variation; compatibility default
P3  TRANSFORMATION  stronger fill/reduction/build/break where allowed
```

`Alt+G` CHAOS remains outside this selector. P3 is not CHAOS.

The selector is runtime/session state for the **next generation request**, not
Scene musical content. Generated material persists through the existing Scene /
project storage path.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- USB-C data cable.
- Headphones, built-in speaker, or the normal GroovePuter audio output.
- Optional Yamaha SEQTRAK for the final release smoke.

No external accessory is required.

## Wiring

No external wiring.

PORT.A is untouched. Existing I2C remains:

```text
SDA GPIO2
SCL GPIO1
```

## Controls

On `GENRE`, `FEEL`, and the main `DRUMS` grid:

```text
P             cycle P1 -> P2 -> P3 -> P1
```

Generation commands:

```text
GENRE G       full Stage 15 groove at selected P-level
DRUMS G       drums only at selected P-level
Ctrl+Alt+G    Stage 12 1/2/4/8-bar audition at selected P-level
Ctrl+G        selected drum voice randomize; not a P-level command
Alt+G         CHAOS; deliberately outside P1/P2/P3
```

`I/O` legacy synth generation is blocked on the release-facing GENERATE pages.
`O` is blocked on the main DRUMS grid. `I` on DRUMS remains the normal Q-I
pattern-slot key.

## Runtime-state contract

P-level has one runtime/session owner and does not change the Scene JSON schema.

```text
boot / firmware start -> P2 VAR
invalid raw value     -> P2 VAR
P press               -> runtime selector only
```

P2 is the compatibility default because production was hard-coded to
`P2Variation` before this selector existed.

Changing P-level performs no synchronous flash/NVS write. Any future persistence
must use a deferred session path and is outside 0.9.1.

## Build / Flash steps

Focused generation matrix:

```bash
bash tests/run_generation_stage13_tests.sh
```

Full host matrix:

```bash
bash tests/run_host_tests.sh
```

Normal Cardputer ADV build:

```bash
bash scripts/build.sh --warnings all
```

Run the repository fixed-DRAM gate against the resulting ELF, then flash with
the normal upload command. Capture phrase-probe Serial at 115200 baud when
checking `Ctrl+Alt+G`.

## Expected behavior

### Screen

Pressing `P` on GENRE, FEEL, or DRUMS shows:

```text
P1 CANON
P2 VAR
P3 TRANS
```

Changing the level alone does not regenerate or erase the current pattern.

`Ctrl+Alt+G` includes the selected level in its toast, for example:

```text
AUD 4B P2 VAR EVOLVED #413
AUD 4B P3 TRANS EVOLVED #413
```

A one-bar-only identity may report `VARIATION` instead of `EVOLVED`; that is the
bounded Stage 12 fallback, not a selector failure.

### Serial

The phrase probe reports a compact level token:

```text
[PHRASE-PROBE] status=EVOLVED level=P2 bars=4 ...
```

The line retains stack high-water, internal heap, largest-block and timing
metrics.

### Musical behavior

For one fixed GENRE / VARIANT / RHYTHM / FEEL and pattern address:

- P1 preserves the clearest canonical identity;
- P2 remains the compatibility behavior;
- P3 transforms more strongly only where the vocabulary permits it;
- P1/P2/P3 do not silently switch genre or incompatible rhythm identity;
- generated Synth A/B pitch remains on the Stage 15 tonal path;
- Alt+G remains a separate CHAOS route.

The three levels are not required to differ on every archetype.

## Hardware acceptance performed

The hardware-tested source head was:

```text
6800e5a2c8f22a641feb816bdb26e70e892647f5
```

Acceptance covered the shared selector and its production consumers, including
P-level cycling, non-mutating selector behavior, generation at the selected
level, phrase audition/probe propagation, compatibility P2 behavior, and
separation from Ctrl+G / Alt+G ownership. The checkpoint was recorded as
`3/3 CLEAN` before squash transfer into #226.

This hardware verdict applies to the P-level implementation itself. It does not
replace the final complete 0.9.1 RC acceptance after all stacked routing and
Stage 15 integration commits converge on one SHA.

## Troubleshooting

### P changes the pattern immediately

Fail. `P` is selector-only. Materialization still requires `G` or
`Ctrl+Alt+G`.

### P1/P2/P3 sounds identical

Use a transformation-capable rhythm such as UK Garage, DnB, or Electro and
compare the same pattern address. Some archetypes intentionally have limited
transformation headroom.

### P3 behaves like Alt+G CHAOS

Fail if the route changed. P3 must remain vocabulary-bounded; Alt+G is the
separate legacy CHAOS owner.

### Level returns to P2 after reboot

Expected. P-level persistence is deliberately deferred in 0.9.1.

### Stage 12 audition says VARIATION

Expected for one-bar-only identities. Confirm the toast and Serial line still
report the selected P-level.

## Acceptance checklist

- [x] P-level request-state host coverage exists for GCC/Clang/sanitizers.
- [x] Boot/default P-level is P2 VAR.
- [x] Invalid raw P-level sanitizes to P2 VAR.
- [x] GENRE / FEEL / DRUMS share one P-level owner.
- [x] Pressing P alone does not mutate material.
- [x] Pressing P performs no synchronous Preferences/NVS write.
- [x] GENRE G consumes selected P-level.
- [x] DRUMS G consumes selected P-level and remains drums-only.
- [x] Ctrl+Alt+G consumes selected P-level.
- [x] Phrase probe reports `level=P1|P2|P3`.
- [x] Ctrl+G selected-voice behavior remains separate.
- [x] Alt+G CHAOS remains separate from P3.
- [x] No Scene JSON P-level field was added.
- [x] P-level implementation hardware verdict recorded on `6800e5a2...`.
- [x] P-level checkpoint completed `3/3 CLEAN` before squash transfer.
- [ ] Final combined #226 exact-head automated matrix passes.
- [ ] Final Stage 15 RC Cardputer ADV acceptance passes after stack merge.
- [ ] Final SEQTRAK smoke and release soak pass on the same RC SHA.
