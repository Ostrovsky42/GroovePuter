# Performance Layer Core — design

**Status:** implementation brief derived from the accepted Performance Layer roadmap in `PLAN.md`.

## Goal

Establish one bounded semantic input boundary for Cardputer and external MIDI, then carry external note ownership through the existing MIDI dispatcher, queue, and authoritative `PerformanceKeyboard`.

## Contracts

- Input adapters emit fixed-size intents and identify an input session independently of USB/TinyUSB/key-matrix details.
- An external note identity includes adapter source, session generation, MIDI channel, and original input note. NoteOff and disconnect release only matching owners.
- Same-pitch holds from different identities share one sounding pitch; releasing one owner cannot stop the pitch while another still holds it.
- The first keyboard vertical slice preserves exact MIDI note and velocity in RAW mode, supports sustain through the existing hold/latch owner, and uses bounded queues.
- Cardputer/QWERTY APIs remain compatible. Existing chord/arp owners remain authoritative; Smart roles build later on the semantic boundary.
- Recovery releases only external-input ownership and does not stop transport, patterns, or other note owners.
- No heap allocation, file access, blocking lock, or semantic work in the sample loop.

## Out of scope for Core slice

Smart BASS/LEAD interpretation, harmonic timeline integration, new arp source/contour semantics, performance controls/scenes, MIDI Learn, controller profiles, Development commands, and recording into material remain later roadmap slices.

## Acceptance

Host tests prove:
- intents are fixed-size and source-neutral;
- MIDI source/session/channel/note identity survives dispatcher and bounded queue;
- equal pitches from separate identities do not release each other;
- stale generation releases cannot affect a new session;
- disconnect and queue recovery clear only external ownership;
- RAW note 0 and 127 reach the semantic output unchanged;
- existing Cardputer, chord/arp, transport, and unrelated owner behavior remains intact.
