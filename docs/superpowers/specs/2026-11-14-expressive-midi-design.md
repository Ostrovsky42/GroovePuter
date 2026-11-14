# Expressive MIDI Design — 0.9.32

## Intent

Make existing performance expression predictable across internal playback and
the selected external MIDI profile. GroovePuter already transforms generated
Pattern/Phrase events before its shared note-lifetime owner and already has a
bounded, user-controlled CC sweep. This milestone closes the profile-routing
and pacing verification gaps around those owners instead of adding a competing
expression system.

Base: 0.9.31 result `91a97488bb2b25103eccf3533e99c2a5375f1103`.

## Current behavior found in source

- The four transient macros are applied to one Pattern/Phrase event before
  `RuntimeSynthPlaybackState` accepts it. Internal synthesis and Pattern MIDI
  consume that accepted event, including its velocity and duration.
- Note lifetime and Note Off remain owned by
  `RuntimeSynthPlaybackState`/`UsbMidiOutput`; CC does not alter note ownership.
- The synth MIDI page exposes seven explicitly selected standard controllers
  (CC 74, 73, 75, 91, 94, 7 and 10). The output rejects other controller numbers,
  deduplicates the last delivered value per synth/controller, retries refused
  values and invalidates delivery knowledge on disconnect.
- `CutoffVoice` owns one bar-based sweep and one selected/held controller per
  synth. The MIDI dispatcher attempts a value at most every 20 ms per
  synth/controller. A fresh controller is unknown and receives no unsolicited
  value; Stop cancels its sweep while retaining the held level.
- Profile routing is frozen at startup. SEQTRAK uses Synth A/B channels 8/9;
  GM and Generic MIDI use channels 1/2. The synth MIDI page currently prints
  `SEQTRAK CH08/09` for every profile, which is false for GM and Generic.
- A Custom profile has no complete Performance route table. Cardputer retains
  its legacy Synth A/B fallback channels 8/9; the UI must present this as the
  current fallback rather than imply that Custom defines a full route profile.

## Goals

- Show the active profile and the actual Performance Synth A/B wire channel on
  the synth MIDI page for SEQTRAK, GM, Generic and Custom fallback routing.
- Keep controller traffic on the existing `UsbMidiOutput` owner and its
  allowlist, output-mode gate, deduplication and reconnect recovery.
- Make the 20 ms limiter's first-attempt and wrap-safe deadline behavior
  directly host-testable without adding dynamic state or blocking work.
- Prove with fake transports that selected CC reaches the correct profile
  channel and unknown/unsupported CC remains suppressed.
- Verify that the existing transformed velocity/duration reaches Pattern MIDI
  and retains its owned Note Off lifetime.
- Record what remains pending for owner listening and hardware acceptance.

## Non-goals

- No macro-to-CC mapping or automatic CC emission from musical macros.
- No new vendor-specific CC, device profile, route rebinding or persisted
  project setting.
- No second MIDI output, note owner, dispatcher or controller state model.
- No Pitch Bend, aftertouch or motion/gesture controls; these remain P2/deferred.
- No hardware, USB endpoint, SEQTRAK or sound-quality claim before 0.9.35.

## Approaches considered

1. **Map macros to CCs for every profile.** Rejected: it couples musical
   controls to device-specific timbre, risks sending values a target did not
   request, and duplicates the existing manual CC controls.
2. **Reuse the shared event and CC owners, close route/pacing gaps.** Selected
   by the implementation: fix the misleading route label, expose the existing
   20 ms attempt policy as a small wrap-safe host-tested helper, and extend fake
   MIDI/UI integration tests. This preserves profile isolation and the current
   real-time design.
3. **Add Pitch Bend/aftertouch expression now.** Rejected for 0.9.32: the
   roadmap places these at P2 and no target-profile support matrix or output
   lifecycle contract is needed to complete P0/P1.

## Technical decisions

- Use `projectMidiPerformanceRoutes()` and the applied profile snapshot to draw
  the displayed profile/channel. For the incomplete Custom profile, show the
  existing Cardputer fallback channel explicitly as Custom fallback.
- Add a pure deadline predicate for the dispatcher's 20 ms send interval. Pass
  an explicit `hasAttempted` state so the first send is allowed even after a
  long uptime; retain wrap-safe signed deadline comparison thereafter. Store
  first-attempt state in a two-byte per-synth bit mask and keep the existing
  fixed per-controller deadlines.
- Extend existing profile-binding and cutoff tests with CC traces for SEQTRAK,
  GM and Generic routes. Keep CC semantics as explicit user-selected controls;
  automatic sweeps remain bounded and deduplicated.
- Extend the existing Pattern macro integration test to assert transformed
  velocity/duration and the corresponding scheduled Note Off. Keep note cleanup
  in the existing owner.
- Do not change project schema or Scene/Song material.

## Constraints

- Real-time paths use fixed data, no heap allocation, file I/O, waits or USB
  calls from the audio callback.
- The MIDI dispatcher owns transport sends; the 20 ms policy limits retries
  and automation traffic per synth/controller.
- Host tests, SDL tests and firmware builds are software evidence. Hardware
  status remains `NOT TESTED` until owner acceptance of 0.9.35.

## Acceptance

- SEQTRAK/GM/Generic UI labels match the Performance route projection; Custom
  clearly identifies its legacy channel fallback.
- CC allowlist, channel mapping, deduplication, failed-send retry, reconnect
  recovery and wraparound pacing pass host fake tests.
- Macro-adjusted velocity and duration reach the Pattern MIDI event, with
  Note Off scheduled from the same duration and released by the existing owner.
- Focused SDL confirms labels fit at 240×135 and profile selection does not
  mutate musical material.
- Default and FS1B firmware builds pass without an undocumented memory
  allowance. No hardware claim is made.
