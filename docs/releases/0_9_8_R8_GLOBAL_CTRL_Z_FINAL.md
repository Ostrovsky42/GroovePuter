# GroovePuter 0.9.8-R8 — Global Ctrl+Z Final Safe Editing Candidate

## Purpose

Close the public Undo UX gap before any new 0.9.9 generation/activation work is rebased onto Safe Editing.

R8 changes only the public Undo chord and the one colliding Synth Sound reset chord. It does not add generation Undo, Song materialization Undo, pending activation, redo, multi-level history, or another mutation owner.

Exact base: `dev_0.9.8 @ 09d095e10589708a47bd535833166adc4e45d72a`.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- Yamaha SEQTRAK is not required to validate R8 Undo ownership itself; retain the normal SEQTRAK MIDI-only firmware compile as a release regression gate.

## Wiring

No wiring changes. Use the normal GroovePuter Cardputer ADV setup.

R8 adds no GPIO, I2C, SPI, USB-MIDI, display, or audio hardware dependency.

## Public UX contract

- `Ctrl+Z` is the single global Undo gesture.
- The shortcut is promoted to the existing `GROOVEPUTER_APP_EVENT_UNDO` before active-page dispatch.
- Pattern, Song and Phrase remain the restore owners for their 0.9.8 receipts.
- If the active page cannot restore the retained receipt, the receipt is retained and UX reports `UNDO: RETURN PAGE`.
- Empty history reports `UNDO: EMPTY`.
- `Ctrl+U` is no longer a second public Undo chord.

### Synth Sound collision

Before R8, TB303 Sound used `Ctrl+Z/X/C/V` to reset Cutoff/Resonance/Env Amount/Env Decay.

R8 reserves `Ctrl+Z` globally and moves only Cutoff reset to `Ctrl+A`:

- `Ctrl+A` — reset Cutoff.
- `Ctrl+X` — reset Resonance.
- `Ctrl+C` — reset Env Amount.
- `Ctrl+V` — reset Env Decay.

The ordinary `A/Z`, `S/X`, `D/C`, `F/V` quick-adjust pairs are unchanged.

## Build / Flash

Focused cumulative contract:

```bash
bash tests/run_undo_0_9_8_r8_tests.sh
```

Then run the normal release matrix on the same exact SHA:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash the exact software-green R8 SHA to Cardputer ADV. Do not call it final/accepted until physical Undo smoke passes on that same SHA.

## Expected behavior

1. Pattern persistent edit -> `Ctrl+Z` restores the previous Pattern state.
2. Ordinary Song assignment/edit -> `Ctrl+Z` restores the previous Song state atomically.
3. Phrase persistent edit -> `Ctrl+Z` restores the previous Phrase state.
4. Pressing `Ctrl+Z` on a page that does not own the retained receipt does not consume it; `UNDO: RETURN PAGE` is shown.
5. TB303 KNOBS/MORE `Ctrl+A` resets Cutoff; `Ctrl+X/C/V` retain their existing reset behavior.
6. `Ctrl+U` no longer performs global Undo.

Generation/materialization receipts remain outside R8. A generated Song cell is not evidence for 0.9.8 ordinary Song Undo acceptance.

## Troubleshooting

- **Ctrl+Z changes Cutoff instead of Undo:** active page consumed the chord before global promotion; this is an R8 failure.
- **Ctrl+U still undoes:** a second public chord remains wired; remove it rather than supporting aliases silently.
- **Undo disappears after visiting another page:** navigation incorrectly mutated Scene revision or consumed the retained receipt.
- **Wrong-domain receipt is consumed:** central display logic became a restore owner; restore must remain page/domain-owned.
- **Ordinary Song assignment cannot be undone:** stop 0.9.9 work and treat this as an independent 0.9.8 regression.

## Acceptance checklist

Software, one exact SHA:

- [ ] cumulative R2-R8 focused contracts PASS
- [ ] Core host regressions PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV normal build PASS
- [ ] fixed DRAM policy PASS
- [ ] Cardputer ADV SEQTRAK MIDI-only build PASS
- [ ] `Ctrl+Z` globally promoted before page dispatch
- [ ] `Ctrl+U` no longer a public Undo chord
- [ ] TB303 Cutoff reset preserved on `Ctrl+A`
- [ ] no generation/activation ownership pulled into 0.9.8

Physical Cardputer ADV, same SHA:

- [ ] Pattern ordinary persistent edit -> `Ctrl+Z` PASS
- [ ] Song ordinary assignment -> `Ctrl+Z` PASS
- [ ] Phrase ordinary persistent edit -> `Ctrl+Z` PASS
- [ ] wrong-page `Ctrl+Z` retains receipt and reports `UNDO: RETURN PAGE`
- [ ] TB303 `Ctrl+A/X/C/V` reset smoke PASS
- [ ] no reboot / Guru Meditation / stack canary / watchdog

Only after the physical ordinary-Song Undo smoke is clean may this SHA become the **NEW FINAL 0.9.8 SHA** used as the base for the 0.9.9 delta/conflict/ownership audit.
