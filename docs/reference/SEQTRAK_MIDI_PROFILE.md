# Yamaha SEQTRAK MIDI 1.0 profile

**Reference baseline:** SEQTRAK OS V2.00  
**Yamaha release date:** 2026-01-14  
**Repository snapshot:** GroovePuter `dev`, reviewed 2026-08-03

This document records the public MIDI contract used by GroovePuter and the smaller subset currently implemented.

## Terminology

Do not call this a **SEQTRAK MIDI API** unless the surrounding text clearly means a device-facing MIDI protocol.

The public interface is not a software or project API with operations such as:

```text
createPattern()
writeStep()
setProject()
loadScene()
```

It is a standard MIDI 1.0 interface with:

- a fixed Yamaha track-to-channel layout;
- documented Control Change assignments;
- MIDI Clock and transport messages;
- ordinary Note On and Note Off messages;
- USB-MIDI and DIN MIDI connectivity.

The SEQTRAK App can edit and back up projects, but Yamaha does not document those app operations as a public MIDI project-editing protocol. GroovePuter must not infer undocumented pattern, project, scene, or storage commands.

Preferred repository terms:

```text
SEQTRAK MIDI profile
SEQTRAK channel map
SEQTRAK MIDI control surface
SEQTRAK target capabilities
```

## Public documentation baseline

The current public Yamaha User Guide describes **SEQTRAK OS V2.00**. Yamaha published the V2.00 updater and SEQTRAK App V2.0.0 on **2026-01-14**.

Official references:

- SEQTRAK User Guide: https://manual.yamaha.com/mi/de/seqtrak/en/
- SEQTRAK downloads and Data List: https://usa.yamaha.com/products/music_production/music-production-studios/seqtrak/downloads.html

Treat the Yamaha manual and Data List as authoritative. This repository document is an implementation-oriented summary and may lag a later Yamaha release.

## MIDI channel model

Yamaha documents this one-based channel map:

| MIDI channel | SEQTRAK track |
|---:|---|
| 1 | Kick |
| 2 | Snare |
| 3 | Clap |
| 4 | Hat 1 |
| 5 | Hat 2 |
| 6 | Perc 1 |
| 7 | Perc 2 |
| 8 | Synth 1 |
| 9 | Synth 2 |
| 10 | DX |
| 11 | Sampler |

This is **not General MIDI drum routing**. General MIDI normally places a drum kit on channel 10 and distinguishes drum voices by note number. SEQTRAK instead exposes seven dedicated drum tracks on channels 1–7.

GroovePuter stores MIDI channels as zero-based values internally. Documentation and UI must display them as one-based channels.

## Current GroovePuter SEQTRAK-native routing

### Melodic lanes

```text
Synth A -> CH8  / internal channel 7
Synth B -> CH9  / internal channel 8
DX      -> CH10 / internal channel 9
```

### Drum lanes

The current native profile sends note 60 to the corresponding SEQTRAK drum channel:

| GroovePuter voice | SEQTRAK destination | MIDI message |
|---|---|---|
| Kick | Kick | CH1, note 60 |
| Snare | Snare | CH2, note 60 |
| Clap | Clap | CH3, note 60 |
| Closed Hat | Hat 1 | CH4, note 60 |
| Open Hat | Hat 2 | CH5, note 60 |
| Mid Tom | Perc 1 | CH6, note 60 |
| Rim | Perc 1 | CH6, note 60 |
| High Tom | Perc 2 | CH7, note 60 |

Mid Tom and Rim intentionally share the same wire identity, CH6 + note 60. `UsbMidiOutput` reference-counts ownership by channel and note so one logical owner cannot release a note still owned by another source.

Relevant implementation files:

- `src/midi/midi_companion_settings.cpp`
- `src/midi/usb_midi_output.h`
- `src/midi/usb_midi_output.cpp`
- `src/platform/cardputer_usb_midi_transport.cpp`
- `src/midi/pattern_drum_gate_scheduler.h`

## Public Control Change surface

The Yamaha V2.00 guide publicly documents at least the following controls relevant to GroovePuter.

| CC | Parameter | Channels / notes |
|---:|---|---|
| 5 | Portamento time | CH8–10; synth/DX must be mono |
| 7 | Track volume | CH1–11 |
| 10 | Track pan | CH1–11 |
| 23 | Track mute | CH1–11; receive-only on SEQTRAK |
| 24 | Solo | CH1–11; receive-only on SEQTRAK |
| 25 | Drum pitch | CH1–7 |
| 26 | Mono / Poly / Chord | CH8–10 |
| 27 | Arpeggiator type | CH8–10 |
| 28 | Arpeggiator gate | CH8–10 |
| 29 | Arpeggiator speed | CH8–10 |
| 65 | Portamento switch | CH8–10 |
| 71 | Filter resonance | CH1–11 |
| 73 | Attack time | CH1–11 |
| 74 | Filter cutoff | CH1–11 |
| 75 | Decay / release time | CH1–11 |
| 91 | Reverb send | CH1–11 |
| 94 | Delay send | CH1–11 |
| 116 | FM algorithm | CH10 / DX |
| 117 | FM modulation amount | CH10 / DX |
| 118 | FM modulator frequency | CH10 / DX |
| 119 | FM modulator feedback | CH10 / DX |

Yamaha also documents assigned effect parameters on CC102–115. Those assignments are part of the public control surface but are not currently a GroovePuter priority.

Do not assume that every documented CC is transmitted by SEQTRAK. In particular, Yamaha marks Mute and Solo as receive-only.

## Current GroovePuter message surface

The accepted implementation primarily uses:

```text
Note On
Note Off
MIDI Clock
Start
Stop
Continue receive
CC123 All Notes Off for bounded recovery / panic paths
```

Continue transmission and Song Position Pointer must remain capability-gated and hardware-tested before being presented as supported target behavior.

GroovePuter does **not** currently implement the complete SEQTRAK control surface. In particular:

- SMF track mute is local filtering inside GroovePuter;
- local SMF mute does not send SEQTRAK CC23;
- local solo does not imply SEQTRAK CC24;
- sound-design CCs are not yet a complete remote editor;
- no public project, pattern-write, scene-load, or step-write MIDI command is assumed;
- no undocumented SysEx contract is part of the supported profile.

## Local mute versus remote mute

Keep these operations distinct:

```text
LOCAL SMF MUTE
  Drop or clean up events owned by an SMF track before wire dispatch.

REMOTE SEQTRAK MUTE
  Send CC23 to a SEQTRAK destination track.
```

They have different ownership and failure semantics. Local mute can be verified internally. Remote mute only proves that GroovePuter transmitted a CC; without feedback it cannot prove the target applied it.

Any future CC23/CC24 implementation must:

- be an explicit SEQTRAK capability, not generic SMF behavior;
- use one-based UI channels and zero-based wire encoding correctly;
- preserve local note ownership and cleanup;
- avoid turning a local track-selection operation into an unexpected remote state change;
- report `SENT`, not claim that target state changed;
- include direct Cardputer-ADV to SEQTRAK hardware acceptance.

## Capability layers

Use these layers when planning work:

### Implemented and accepted

- SEQTRAK-native channel routing;
- Note On / Note Off output;
- sample/deadline-based Pattern and SMF dispatch;
- MIDI Clock and Start/Stop output;
- external transport receive path, including Continue receive;
- scoped active-note ownership and CC123 recovery;
- General MIDI as a separate selectable profile.

### Partial or in-flight

- complete Continue/SPP lifecycle semantics;
- immediate ownership-safe SMF track mute cleanup;
- target capability declarations and hardware acceptance.

### Deferred control-surface work

- CC23 remote track mute;
- CC24 remote solo;
- volume, pan, filter, envelope, send, portamento, arpeggiator, and DX controls;
- custom per-track control mapping;
- additional device profiles;
- carefully scoped SysEx only if Yamaha publishes or hardware testing establishes a stable contract.

## Documentation rule

When a document says "SEQTRAK API," replace it with the narrowest accurate term:

```text
MIDI profile
channel map
Control Change mapping
transport capability
hardware behavior
```

Use "API" only for an actual callable software interface implemented inside GroovePuter or explicitly published by Yamaha.

## Acceptance checklist for future SEQTRAK controls

- [ ] The message and value range are present in the Yamaha OS V2.00 documentation or a newer cited version.
- [ ] Receive-only versus transmit behavior is recorded.
- [ ] Channel numbering is tested at the UI and wire boundaries.
- [ ] The command reuses the single `MidiDispatchTask` writer.
- [ ] No dynamic allocation is added to the dispatch path.
- [ ] Shared channel + note ownership remains correct.
- [ ] Stop, route change, disconnect, and panic leave no stuck notes.
- [ ] UI uses honest `SENT` wording when target acknowledgement is unavailable.
- [ ] Direct Cardputer-ADV to SEQTRAK hardware acceptance is documented.
