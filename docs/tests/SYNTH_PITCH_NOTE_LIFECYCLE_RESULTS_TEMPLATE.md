# Synth pitch and note lifecycle — hardware result

**Device:** Cardputer-Adv
**Branch/commit:**
**Date:**
**Tester:**
**Output path:** built-in speaker / headphones / recorded line

## AY

| Test | Result | Notes |
|---|---|---|
| D#4 → E4 → F4 → F#4 → G4 distinct and ascending | PASS / FAIL | |
| C1..B4 has no adjacent collapsed semitones | PASS / FAIL | |
| No unacceptable upper-register whistle | PASS / FAIL | |

## SN76489

| Test | Result | Notes |
|---|---|---|
| C1 → A1 → C2 → F#2 → A2 preserves pitch classes | PASS / FAIL | |
| Low register does not collapse to one drone | PASS / FAIL | |
| `Oct+` is root + 1 octave + 2 octaves | PASS / FAIL | |
| Stack remains musically usable on A2/A3/A4 | PASS / FAIL | |

## Live NoteOff

| Test | Result | Notes |
|---|---|---|
| Above-B4 NoteOn releases from original NoteOff | PASS / FAIL | |
| Below-C1 NoteOn releases from original NoteOff | PASS / FAIL | |
| Synth A passes | PASS / FAIL | |
| Synth B passes | PASS / FAIL | |

## Stability

| Test | Result | Notes |
|---|---|---|
| No reset/watchdog/heap error | PASS / FAIL | |
| Underruns do not continually increase | PASS / FAIL | |
| Normal in-range release unchanged | PASS / FAIL | |

## Decision

- [ ] Accepted for merge.
- [ ] Changes requested.

Details:
