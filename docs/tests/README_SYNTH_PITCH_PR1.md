# PR1 quick acceptance

Run:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Then validate:

```text
AY:       D#4 E4 F4 F#4 G4
SN Uni:   C1 A1 C2 F#2 A2
SN Oct+:  A2 A3 A4
NoteOff:  one note below C1 and one above B4 on Synth A and Synth B
```

Pass criteria:

- AY notes are distinct and ascending.
- SN preserves pitch classes instead of one fixed lower drone.
- `Oct+` stacks upward.
- Every original out-of-range key release silences its clamped note.
- No reset, watchdog, heap error, or continuously rising underruns.

Full protocol: `docs/tests/SYNTH_PITCH_NOTE_LIFECYCLE_CARDPUTER_ADV.md`.
