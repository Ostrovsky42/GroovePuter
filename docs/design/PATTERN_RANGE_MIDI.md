# Pattern range above B4 for MIDI (C#5, E5 .. C7)

Status: implemented in the working tree, host-tested, not hardware-tested, not committed.

## Problem

A STEPS Pattern could only hold C1..B4 (24..71). A line written for an external
synth (SEQTRAK, USB/DIN MIDI) that goes to C#5 (73) or E5 (76) could not be
entered: NOTE ENTRY refused it, `S`/`A` stopped at 71.

## Where a step note was clipped or refused (before this change)

| Stage | Place | Note above 71 |
|---|---|---|
| Write: NOTE ENTRY | `src/ui/pages/pattern_edit_page.cpp` `kEntryNoteMax = 71`, `kEntryOctaveShiftMax = 0`, `handleNoteEntryKey` | refused with `NOTE OUT OF RANGE`; highest reachable key was A4 (69) |
| Write: `S`/`X`, `A`/`Z`, Meta arrows (STEPS) | `src/state/synth_pattern_edit.h` `adjustNote` (`kMax303Note = 71`) | clamped to 71 |
| Write: legacy pages / HUB | `src/dsp/miniacid_engine.cpp` `adjust303StepNote` -> `clamp303Note` | clamped to 71 |
| Write: Working material edit | `src/dsp/miniacid_engine.h` `adjustWorking303StepNote` -> `clamp303Note` | clamped to 71 |
| Write: MIDI file import | `src/audio/midi_importer.cpp:409-411` | folded by octaves into 24..71 (unchanged, see below) |
| Write: generation | mode/genre generators use their own registers (<= 67..71) | never produced; unchanged |
| Storage | `scenes.h` `SynthStep::note` is `int8_t`; `scenes.cpp:430-431` loader clamps to -2..127 | kept as is (0..127 already legal) |
| Runtime projection | `src/phrase/runtime_synth_events.cpp:83,233` | passed through unchanged |
| Output: internal voice, Pattern/Phrase | `src/dsp/miniacid_engine.cpp` `consumePatternPlaybackActions_`, `followChordWithInternalVoice_` -> `noteToFreq(event.note)` | NOT clipped: played at true pitch (Melody chord notes could already reach it) |
| Output: internal voice, live | `src/dsp/miniacid_engine.cpp:736` `liveNoteOn` -> `clamp303Note`; `src/input/internal_synth_output.cpp:7`; `src/dsp/clamped_live_note_identity.h:12`; `src/midi/midi_input_dispatcher.h` `clampSynthNote` | clamped to 71 (unchanged: not a Pattern path) |
| Output: MIDI | `src/midi/usb_midi_output.cpp` `clampDataByte` (<= 127); `src/platform/cardputer_usb_midi_transport.cpp` `clamp7Bit` (<= 127); `MusicalEventRouter` does not touch notes | passed through 0..127 |

So the only real wall was the editor. The MIDI path already carried 0..127.

## Options

- **(a) Wide steps, MIDI as written, internal voice folded.** Steps hold 24..96;
  MIDI gets the step note; the internal voice plays it folded into <= 71.
- **(b) Wide everywhere, internal at true pitch.** The internal synth runs at
  22.05 kHz (`src/audio/audio_config.h`) with naive saw/square
  (`Wavetable::lookupSaw` is closed-form, not band-limited,
  `src/dsp/audio_wavetables.h`) and a filter whose cutoff does not track the
  key (`mini_tb303.cpp`, cutoff <= min(0.9 * Nyquist, 8 kHz)). At E5 (659 Hz)
  harmonic 17 already folds back; at C7 (2093 Hz) only five harmonics fit and
  harmonics 6, 7 land inharmonically at ~9.5 kHz and ~7.4 kHz, inside the
  filter passband. With a typical acid cutoff of a few hundred Hz the
  fundamental of an E5 is filtered away, so the line gets quiet and thin. No
  DRAM or CPU cost, but it sounds bad and is inconsistent with the live path.
- **(c) Leave steps, add a per-track MIDI octave offset.** Needs a new persisted
  field (scene format, UI, DRAM), and a line spanning B4..E5 still cannot be
  written without moving everything else.

**Chosen: (a).** It is the smallest change that lets the user write the real
pitch, keeps the wire exact, adds no state, and keeps the internal voice in the
range it was voiced for. The fold keeps the pitch class, so the internal synth
still plays the right harmony one octave (or two) lower.

## What changed

- `src/state/synth_pattern_edit.h`: `kMaxPatternNote = 96`; `adjustNote` /
  `adjustOctave` clamp at 96 instead of 71. Below C1 is still a rest.
- `src/dsp/miniacid_engine.h`: `MiniAcid::kMaxPatternNote = 96`;
  `MiniAcid::internalVoiceNote(note)` (constexpr, `while (note > 71) note -= 12`);
  `adjustWorking303StepNote` clamps at 96. `kMax303Note` stays 71 and keeps
  meaning "top of the internal voice"; `clamp303Note` (live path) is unchanged.
- `src/dsp/miniacid_engine.cpp`: `adjust303StepNote` clamps at 96; the three
  internal `startNote(noteToFreq(...))` calls of Pattern/Phrase playback use
  `internalVoiceNote`. `publishPatternNoteOn_` / chord NoteOns still publish the
  step note unchanged. This is a few integer instructions per note start in
  the audio task, no per-sample work, no allocation, no lock, no file system.
- `src/ui/pages/pattern_edit_page.cpp`: `kEntryOctaveShiftMax = 2`
  (rows 4-5 and 5-6 added), `kEntryNoteMax = 96`. With `=` once the upper row
  is C5..A5: `W` = C#5 (73), `T` = E5 (76). With `=` twice it is C6..A6.
- Help (`src/ui/global_help_content.h`), `MANUAL.md`, `src/ui/docs/keys.md`.

Not changed: NoteOff and cancel logic, live keyboard / MIDI IN clamps (live
internal voice still clamps to 71), MIDI importer (still folds into 24..71;
widening it changes import results and is a separate decision), generators,
scene format.

Known edge: a slide from B4 into C5 on the internal voice glides an octave down
(71 -> 60), because the target is folded. MIDI glides 71 -> 72 as written.
Melody chord notes above 71 (already possible before) now sound folded inside
instead of at true pitch.

## Tested (host)

- `tests/test_pattern_range_wire.cpp` (`run_smart_play_tests.sh pattern_range_wire`):
  UsbMidiOutput sends Pattern NoteOn/NoteOff 71, 73, 76, 84, 96, 127 with the
  same note number (GenericMidi and SEQTRAK profiles); edit clamp E4 -> E5,
  C5 -> C#5, saturates at C7, below C1 is a rest. Also reruns
  `test_pattern_mutations_0_9_8_r3` (updated to the new ceiling) and the
  NOTE ENTRY source contract (`kEntryNoteMax = 96`).
- `tests/test_pattern_range_engine.cpp` (`run_smart_play_tests.sh pattern_range_engine`,
  SDL host source set): on the real STEPS page `N`, `=`, `T` writes 76 and `W`
  writes 73; `=` `=` `P` writes 93; `S` E4 -> E5 -> ... saturates at 96; `X`,
  `A`, `Z`; engine `adjust303StepOctave/Note`. Playing the Pattern publishes
  NoteOns 76, 73, 93, 60, 71 toward MIDI while the TB303 target frequency is
  that of 64, 61, 69, 60, 71. `internalVoiceNote` keeps 0..71 unchanged and
  pitch class for 0..127. A scene with 24, 48, 60, 71, 73, 76, 96, TIE saves
  and loads with identical notes.

## To check by ear and on a MIDI receiver

1. STEPS, `N`, `=`, `W` `Enter` `T`: the cells show `C#5`, `E5`. On SEQTRAK /
   a MIDI monitor: note numbers 73 and 76 on the Synth A channel.
2. Same Pattern on the internal synth: C#4 and E4 (one octave lower), in tune
   with the rest of the line; no stuck or missing notes.
3. `S` up to C7 on a step: MIDI 96; internal C4.
4. A slide B4 -> C5 inside: audible octave-down glide (expected, see edge).
5. Load an old project: Patterns sound exactly as before.

## DRAM

| Build | Before | After | Delta |
|---|---|---|---|
| `build_cardputer_dynbuffers.sh` DRAM globals (CDC) | 185796 | **185796** | **+0 B** |
| `build_seqtrak_midi_only.sh` DRAM globals | 186236 | **186236** | **+0 B** |

Budget 191488. The change is constants and a few instructions; no new state.
