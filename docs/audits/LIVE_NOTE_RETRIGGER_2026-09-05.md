# Live keyboard notes retrigger — 2026-09-05

Recorded because a plausible diagnosis was built, acted on, and then refuted by
measurement. The cause is still unknown; what follows is meant to stop the same
wrong path being taken again.

## Symptom

Notes played from the Cardputer keyboard into an external SAM2695 sound gritty
and retriggered. Sequenced notes from the same instrument sound clean. The
operator's own isolation — pattern playback fine, keyboard playing not — is what
narrowed this to the live path.

## The diagnosis that was wrong

Live notes go through `reconcilePerformanceKeys()` in `loop()`, which calls
`PerformanceKeyboard::releaseMissingKeys()` against one keyboard scan. That
released a note the moment its key was absent from a single scan, and
`shouldDispatchHid()` would then treat the key's reappearance as a fresh press.
So a single dropped scan would produce NoteOff immediately followed by NoteOn: a
retrigger. Sequenced notes never pass through either function, which fitted the
operator's isolation exactly.

The supporting evidence was runs of five identical `[KEY] press=` events for one
key with `repeat=0` in a serial log.

## What was done, and what it cost

A two-scan hysteresis was added to `releaseMissingKeys()`: release only after the
key is absent from two consecutive scans. Host tests were written first, the new
one reproduced the single-scan release, and three existing expectations were
moved onto the new contract. All of it passed, and the firmware was flashed.

On hardware the retrigger was replaced by stuck notes, including on the internal
Synth A, which is worse than the defect it was meant to fix. It was reverted and
the previous firmware restored.

## The measurement that settles it

`src/diag/p3_key_scan_trace.h` logs every change to the set of note keys the
matrix reports, with a millisecond stamp. Diagnostic images only; the product
ELF contains none of it.

Holding and releasing single keys produced, without exception:

```
ms=9612   mask=0x00000020 count=1
ms=10634  mask=0x00000028 count=2
ms=10699  mask=0x00000008 count=1
ms=11905  mask=0x00000000 count=0
```

One transition into presence, one out, nothing between. Holds measured 1074,
482, 414, 317, 275, 210, 188 and 173 ms. Not one intermediate flicker in the
whole capture.

So the matrix reports held keys continuously, `containsHid(previous, hid)` stays
true for the whole hold, and `shouldDispatchHid()` cannot re-dispatch. Scan
dropouts do not happen and never explained the retrigger.

The five-in-a-row presses were the other reading of that evidence all along:

```
76782 → 76992   210 ms
77361 → 77549   188 ms
77678 → 77851   173 ms
```

Someone playing the same note several times. That alternative was considered and
not checked, which is the whole error.

## Assumptions worth not repeating

Two premises were also wrong and are recorded so they are not reused:

- The UI draw does not block every loop pass. It is throttled to once per 40 ms,
  and `delay(5)` sets the loop at roughly 200 Hz. The keyboard is sampled at
  about 200 Hz, not the 25 Hz that "one scan per UI frame" suggested.
- `reconcilePerformanceKeys()` runs unconditionally every pass, directly in
  `loop()`, with no gate above it.

## Still open

The retrigger's cause. Candidates not yet examined: the SAM2695's own behaviour
under rapid note messages, the 435 `panic` events counted in a session log, and
note delivery jitter of up to 4.66 ms.

Separately, and visible in the source rather than inferred:
`reconcilePerformanceKeys()` collects held note keys only when no modifier is
down, so pressing Shift, Fn, Ctrl or Alt while holding notes passes an empty set
to `releaseMissingKeys()` and releases all of them at once. Not fixed here.
