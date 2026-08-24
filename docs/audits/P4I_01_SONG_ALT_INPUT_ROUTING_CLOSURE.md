# 0.9.9-P4I-01 — Song Alt Input Routing Closure

## Purpose

Close `INTEGRATION-HW-P4I-01` without changing Pattern Picker, generation,
PatternLease, Song persistence, Phrase, display, MIDI, or bus semantics.

Hardware-tested source before this closure:

`8f0910381d1c73e23642a2003379ff676d15ae46`

Observed on Cardputer ADV:

- SONG `Alt+H`: no visible response.
- SONG `Alt+Enter`: no visible response.
- An unrelated-page `Alt+Enter` control was captured.

The source already contains the intended sinks:

- `MiniAcidDisplay::handleEvent()` owns global `Alt+H` help before page dispatch.
- `SongPage::handleEvent()` owns `Alt+Enter` and calls the existing
  `openPatternPicker()` implementation.

## Root cause closed in code

The Cardputer raw adapter accepts both `KeysState::hid_keys` and
`KeysState::word`. `GroovePuter.ino` deliberately filters word control bytes and
word letters while Alt/Ctrl is held because those keys are normally expected to
arrive through HID.

That assumption was already known to vary by M5Cardputer representation: Tab has
an existing word-only compatibility path. Before P4I-01 there was no equivalent
fallback for a word-only Alt-letter or Alt+Enter representation. Such an input
could therefore be discarded before `MiniAcidDisplay::handleEvent()` even though
both downstream UI handlers were correct.

P4I-01 extends the existing edge/normalization boundary only:

1. Prefer the HID copy when the same Alt-letter/Enter exists in `hid_keys`.
2. If an Alt-only letter/Enter exists only in `word`, temporarily encode it
   through the existing raw word filter.
3. Restore the original logical key immediately before `UIEvent` dispatch.
4. If Alt is activated while a word-only letter is already held, emit the new
   modified edge, matching the existing HID edge contract.
5. Plain Enter and Ctrl paths retain their previous behavior.

No permanent hardware diagnostic logging is added.

### Evidence boundary

The original device run did not record raw `hid_keys` versus `word` contents for
the failed combinations. Therefore this audit does **not** claim that the tested
device was proven to emit word-only Alt+H/Alt+Enter. It closes the deterministic
raw-to-logical routing gap that can lose exactly those combinations and is
consistent with the hardware finding. Physical closure still requires the device
retest below.

## Event path

### Before

```text
M5Cardputer scan
  -> HID event, if present: canonical UIEvent
  -> word event, if present
       -> Alt-letter / Enter raw filter may discard it
  -> MiniAcidDisplay::handleEvent()
       -> global Alt+H
       -> current page
            -> SongPage::handleEvent()
                 -> Alt+Enter
                 -> existing openPatternPicker()
```

### After

```text
M5Cardputer scan
  -> GroovePuterInput edge normalization
       -> HID copy wins when present
       -> word-only Alt-letter / Alt+Enter is preserved
  -> canonical UIEvent (Alt + logical key)
  -> MiniAcidDisplay::handleEvent()
       -> global Alt+H help
       -> current page
            -> SongPage::handleEvent()
                 -> existing Alt+Enter gesture
                 -> existing openPatternPicker()
```

## Ownership preserved

Pattern Picker semantics are unchanged. P4I-01 does not edit Song picker
implementation files.

PatternLease ownership is unchanged. The existing P4I path continues to use:

`PhrasePatternLease::patternLeaseOwner()`

No allocator, lease owner, free-slot search, generation policy, persistent Song
mutation path, Phrase owner, or Undo owner is added here.

## Hardware list

- M5Stack Cardputer ADV running the P4I-01 firmware.
- USB cable for flash and serial output.

No external unit is required for this acceptance test.

## Wiring

No wiring change.

If PORT.A is otherwise connected in the test setup, its established I2C policy
remains unchanged:

- SDA: GPIO2
- SCL: GPIO1

P4I-01 does not touch display, audio, MIDI, I2C, SPI, or power wiring.

## Build / test

Focused and cumulative P4I routing contracts:

```bash
bash tests/run_0_9_9_p4i_01_tests.sh
```

Full host regressions:

```bash
bash tests/run_host_tests.sh
```

SDL:

```bash
make -C platform_sdl clean all CXX=g++
```

Cardputer ADV:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
```

SEQTRAK MIDI-only:

```bash
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

## Expected host behavior

The focused test proves:

- word-only Song `Alt+H` becomes logical `Alt+h` and reaches the existing global
  help contract;
- word-only Song `Alt+Enter` becomes logical `Alt+Enter` and reaches the existing
  Song Pattern Picker contract;
- activating Alt on an already-held word-only letter creates the modified edge;
- HID+word representations dispatch only the canonical HID copy;
- plain `H` is not `Alt+H`;
- plain `Enter` is not `Alt+Enter` and keeps the old raw word behavior;
- unrelated-page HID `Alt+Enter` remains a single canonical event;
- existing Song `G` and word-only Tab behavior remain unchanged.

The source contract additionally pins the real production sinks in
`MiniAcidDisplay` and `SongPage` and verifies that the input closure does not
reference PatternLease or Song allocation code.

## Cardputer ADV hardware acceptance

Hardware status remains **PENDING** until this exact closure is flashed and
retested.

1. Boot Cardputer ADV.
2. Open SONG.
3. Press `Alt+H`.
   - Expected: Song-context help is visible.
4. Close help.
5. Press `Alt+Enter`.
   - Expected: Pattern Picker opens.
6. Cancel Picker.
   - Expected: return to the same Song state.
7. Select another Song cell and repeat `Alt+Enter`.
   - Expected: Picker opens for the newly selected cell.
8. Repeat the previously working unrelated Alt-shortcut control.
   - Expected: its existing behavior is unchanged.
9. Inspect serial output.
   - Expected: no repeated exception, assertion, watchdog reset, or reboot loop.

## Troubleshooting

If either Song shortcut still fails on hardware, capture one existing `[KEY]`
line for each failed press. Record `src`, `alt`, `ctrl`, `key`, and `sc` from the
existing input diagnostic; do not add a second scanner. This distinguishes:

- no raw edge;
- raw edge normalized to the wrong logical key;
- canonical event reaching UI but not being handled.

If `Alt+H` reaches `MiniAcidDisplay` but help is not visible, investigate display
or overlay state separately; do not move the shortcut into SongPage.

If `Alt+Enter` reaches `SongPage` but Picker does not open, investigate the
existing P4I modal state separately; do not add another PatternLease owner.

## Acceptance checklist

- [x] Exact tested source used as branch start: `8f0910381d1c73e23642a2003379ff676d15ae46`.
- [x] Existing global `Alt+H` sink retained.
- [x] Existing Song `Alt+Enter` / `openPatternPicker()` sink retained.
- [x] No second keyboard scanner.
- [x] No SongPage direct keyboard polling.
- [x] No central-dispatch bypass.
- [x] Pattern Picker semantics unchanged.
- [x] PatternLease owner unchanged.
- [x] Plain-key negative controls covered.
- [x] Unrelated-page control covered.
- [x] Existing Song hotkey control covered.
- [ ] Focused/cumulative suite green on final SHA.
- [ ] Full host regressions green on final SHA.
- [ ] SDL build green on final SHA.
- [ ] Cardputer ADV compile green on final SHA.
- [ ] Fixed DRAM check green on final SHA.
- [ ] SEQTRAK MIDI-only build green on final SHA.
- [ ] Physical Cardputer ADV `Alt+H` retest PASS.
- [ ] Physical Cardputer ADV `Alt+Enter` retest PASS.
- [ ] Physical serial acceptance PASS.
