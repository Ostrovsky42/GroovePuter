# Expressive MIDI — 0.9.32

## Scope

0.9.32 closes host-verification gaps around the existing Performance MIDI
expression path. It keeps note ownership in `RuntimeSynthPlaybackState` and
`UsbMidiOutput`, uses the existing selected-controller allowlist, and makes the
visible Synth A/B route agree with the selected output profile. Musical macros
remain transient projections of material; they do not generate MIDI CCs or
rewrite a saved Scene, Phrase or Song.

## Contracts

- TENSION, ENERGY, SPACE and MUTATION transform the selected Pattern/Phrase
  event before the shared note-lifetime owner. Internal playback and Pattern
  MIDI receive the same accepted event.
- Pattern MIDI Note Off follows the transformed event duration. The regression
  checks source duration preservation, a shorter projected duration, no release
  one subtick before the resulting deadline, and one Note Off at that deadline.
- Manual CC and sweep output remain on `UsbMidiOutput`. The synth page exposes
  seven selected controllers: CC74 CUTOFF, CC73 ATTACK, CC75 RELEASE, CC91
  REVERB, CC94 DELAY, CC7 VOLUME and CC10 PAN. Unsupported controller numbers
  remain suppressed.
- The output retains per-synth/controller deduplication, retry after a refused
  send, and resending a held value after reconnect. CC does not alter note
  ownership.
- The dispatcher limits retries/automation attempts to at most one per 20 ms
  for each synth/controller. A small attempted bit mask distinguishes a fresh
  deadline from an initialized one; deadline comparison is wrap-safe. First
  sends remain eligible after long uptime.
- Profile route labels use the existing Performance route projection. SEQTRAK
  displays channels 8/9; General MIDI and Generic MIDI display 1/2. Custom has
  no complete persisted Performance route table, so the page labels the
  Cardputer's existing fallback channels 8/9 as `CUSTOM`.
- Profile routing remains a startup snapshot. Changing a saved profile does
  not rebind an active output in this milestone.

## Host evidence

| Check | Evidence |
|---|---|
| Deadline first attempt, exact 20 ms threshold and `uint32_t` wrap | `tests/test_cutoff_sweep.cpp`; targeted build/run and canonical host suite |
| CC channel projection for Synth A/B on SEQTRAK, GM and Generic; unsupported CC5 suppressed; Custom fallback channels | `tests/test_midi_performance_route_binding_0_9_7_tests.sh` |
| Controller allowlist, clamping, dedupe, refused-send retry, disconnect/reconnect resend, output-mode gate, and note ownership unchanged | `tests/test_usb_midi_cutoff.cpp` via canonical host suite |
| Profile label/channel and compact-screen fit for SEQTRAK, GM, Generic and Custom | `tests/run_midi_perform_ui_polish_tests.sh` |
| Macro-transformed Pattern event, Note On velocity, source preservation, shortened gate and matching Note Off | `tests/run_pattern_phrase_p2_p0_compatibility.sh` (GCC repeat, Clang parity, ASan and UBSan) |

The fake transports verify software message routing. They do not prove USB
enumeration, device compatibility, audible results, DIN timing, or physical
Cardputer keyboard usability.

## Deferred

- Owner listening for musical usefulness of each macro and its combined values.
- Physical USB MIDI and Yamaha SEQTRAK validation.
- Pitch Bend and aftertouch pending an explicit supported-route/lifecycle
  contract; motion and gesture input are also deferred. No IMU capability is
  assumed.
- Macro-to-CC mapping, automatic CC emission from macros, live route rebinding,
  and persisted macro state.

**Hardware status:** `NOT TESTED`  
**Milestone status:** `HOST-VALIDATED / HARDWARE-UNVERIFIED`
