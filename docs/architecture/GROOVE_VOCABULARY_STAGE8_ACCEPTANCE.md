# Generation Stage 8 — Deterministic Feel Acceptance

Status: host-complete; Cardputer/SEQTRAK listening judgment pending

Implementation base record:

```text
current origin/dev_0.9_test: 0a2fe15696eb7c8f0bdcc1a986aed3ca342dd948
stage base:                 f26e3a63c33022625ad6a6e97f5f71ef0d4f8669
merge-base with devtest:    0a2fe15696eb7c8f0bdcc1a986aed3ca342dd948
devtest advanced in stage:  no
```

## Ownership contract

```text
ideal Phrase event + role + duration
                 |
                 v
       stable FEEL profile + amount
                 |
                 v
barOrigin + idealTick + boundedRoleOffset
                 |
                 v
     existing physical pattern timing
```

- `Straight`, `SwingCompatible`, `LaidBack`, and `PushPullControlled` are stable
  persisted IDs. A legacy Scene with no `profile` field decodes as `Straight`.
- The interpreter is fixed-capacity and allocation-free. It has no Genre,
  transport, BPM, MIDI routing, or physical voice knowledge.
- Offset is derived from each ideal coordinate and is bounded to at most one
  quarter of the active grid interval (six ticks at 96 PPQN / 16th grid).
- An onset cannot leave its protected bar. Distinct ideal coordinates cannot be
  reversed, and NoteOff remains after NoteOn with the requested duration.
- Existing `swingPct` / `swingMask` remain the sole swing owner. Stage 8 does not
  delay MIDI Clock, Start, or Stop.
- The production adapter uses the semantic rhythm role and explicit physical
  binding after Stage 7C materialization. It does not infer a synth role.
- Timing correctness is host-accepted. Whether a profile subjectively feels
  laid-back is not accepted without Cardputer listening.

## Host validation

From the repository root:

```bash
ASAN_OPTIONS=detect_leaks=0 bash tests/run_feel_stage8_tests.sh
ASAN_OPTIONS=detect_leaks=0 bash tests/run_rhythm_stage7c_tests.sh
```

The matrix covers GCC, Clang when installed, and ASan/UBSan. It proves:

- deterministic replay for identical seed/profile/identity coordinates;
- eight-bar no-drift behavior across a 128-event fixed-capacity phrase;
- bounded offsets over every profile and a 64-seed corpus;
- protected bar boundaries, event ordering, and NoteOn/NoteOff duration;
- transactional invalid/overflow fallback;
- identical MIDI Clock/Start/Stop fixture bytes with Feel OFF and ON;
- `Straight` leaves vocabulary timing on the ideal grid;
- `LaidBack` reaches ordinary Stage 7C production materialization;
- Scene round-trip and legacy `Straight` decoding;
- source ownership: no Genre checks, global RNG, transport writer, or second
  playback Feel interpreter.

## Hardware smoke

1. Record the exact Stage 8 SHA, build profile, Scene, Genre/Variant, named
   rhythm identity, pattern address, Feel profile, amount, swing, BPM, and seed.
2. At 70–88 BPM compare `STRAIGHT` and `LAID BACK` with the same named rhythm.
3. Compare `SWING COMPAT` at swing 50 and 58; confirm the swing control remains
   independently audible and does not double-trigger events.
4. Compare `PUSH/PULL` at 25, 50, and 100 amount. Listen for stuck notes,
   reversed hits, bar-boundary flams, or accumulating drift.
5. With optional SEQTRAK attached, verify Clock/Start/Stop lock and unchanged
   routing while switching profiles and regenerating.
6. Save, reboot, and load; confirm the selected profile returns.

## Acceptance checklist

- [ ] All five FEEL rows fit the 240x135 Cardputer display.
- [ ] Legacy Scene loads as `STRAIGHT`.
- [ ] Profile selection survives Save/reboot/Load.
- [ ] Same identity/address/profile repeats the same offsets.
- [ ] `STRAIGHT` matches ideal vocabulary event timing.
- [ ] Slow `LAID BACK` fixture has no bar-to-bar drift.
- [ ] `SWING COMPAT` does not compete with the swing control.
- [ ] No MIDI clock, start/stop, routing, pitch, topology, or synth TYPE change.
- [ ] Listening result is recorded as `HARDWARE_ACCEPTED` or
      `HARDWARE_PENDING` against the frozen SHA.
