# PERFORM silence / Ctrl+A squeal — diagnostic image

The user reports silence with OUT=INTERNAL and continued squeal after Ctrl+A.
The earlier Ctrl+A change was action feedback and unsupported-engine refusal;
the TB303 reset remains cutoff=800 Hz, retaining resonance/envelope/filter/FX.
Neither physical audio symptom is fixed or assigned a confirmed cause here.

## Evidence

- Output ownership/queues/Scene persistence/source contract gate: PASS.
  Legacy compatibility displays LAYER but routes PERFORM only to MIDI; this is
  a misleading display state, but OUT=INTERNAL silence rules it out as a sole
  explanation. No routing or legacy persistence behavior changed in this image.
- Real engine test through MiniAcidDisplay PERFORM -> keyboard -> router ->
  InternalSynthOutput -> MiniAcid::generateAudioBuffer: PASS in STOP, MONO,
  explicit INTERNAL. Note 36 becomes the engine live note; PCM peak=857, and
  key release clears the live note. Fixture uses master=50%, track=100%,
  Synth A unmuted. This is nonzero host PCM, not a hardware audibility verdict
  or proof of the user's patch/settings. Log: `/tmp/gp-perform-real-pcm.log`.
- Base TB303 release/distortion contracts: PASS. They do not reproduce the
  user's unknown engine/filter/resonance/FX settings.

## New diagnostic surface

Fn+D on PERFORM or Synth A/B now shows audio state in the lower rows. Other
pages retain the existing USB latency/raw/clock panel. Existing memory and
stack rows remain visible. Engine state is copied at an audio block boundary;
the gate is released before formatting/drawing. No audio/DSP constants, task
stack, DMA size, routing, or synth settings changed. No new resident counters.

- KEY: target, MONO/POLY, performer enabled (`en`), note mode (`nm`), ARP.
- NOTE: physical/external held count (`h`), performer note (`key`), selected
  internal voice's live note (`liveA` or `liveB`). -1 means no live note.
- INT/MUTE/PAT/LEAD/VOL: actual local PERFORM permission, selected voice mute,
  Pattern ownership, lead mode index, track volume percent.
- ENG/MASTER/TONE: selected voice engine, master percent, test-tone flag.
- TB303-only CUT/R/F and AMP/ENV/DEC: cutoff Hz, resonance, filter profile,
  synth amplitude percent, envelope amount and decay milliseconds.

For PERFORM hold an ordinary note with Fn released. For Ctrl+A use the affected
Synth SOUND page, then inspect the patch fields. This image is instrumentation,
not a claim that either audio symptom has been repaired.

## Artifact and upload

Source: dirty `/home/gg/.codex/worktrees/hgs-review-5e98` at HEAD `5e98acbd`.
Clean FS1B diagnostic ADV build PASS; existing CDCOnBoot=default, PSRAM disabled,
huge_app, GROOVEPUTER_USB_ACCEPT_DIAG profile preserved.
Flash 1,670,890 bytes; static DRAM 189,652 bytes (unchanged), within the
provisional 191,488-byte gate. Build: `build/review-audio-diagnostic-20261014/`.

ELF SHA-256: `1277465c229fed4304640d9245e7d0bc4e0477daf5507ffb1cc1ba79fb1c9a49`.
BIN SHA-256: `3d9ca2c4eafd10ce657c2fdfa7ec1753642a419a1a960c73c51e9ac4f1144c75`.
Build log: `/tmp/gp-perform-audio-diag-build.log`.
Upload PASS on ttyACM0, exit 0, all written segments hash-verified.
Upload log: `/tmp/gp-perform-audio-diag-upload.log`.
Physical startup/audio diagnostic values are pending user observation.

Runtime observation channel is the Fn+D screen. CDCOnBoot=default disables
the guarded Serial.begin(); the image preserves runtime USB Host/Device role
selection. A ttyACM bootloader upload port is not proof of an application serial
console, and USB Host can remove it after application startup. The user reported
a missing ttyACM monitor port; subsequent board discovery found no serial device.
No runtime log or application-startup claim was established by the upload.
The flashed ELF was checked to contain the new KEY/NOTE/INT/ENG/CUT format strings.

## Physical follow-up

User reports: liveA=-1, INT=1, MUTE=0, PAT=0, LEAD=0, MONO. After disabling
ARP, built-in sound returns. This localizes the observed silence to the enabled
clocked performance path; the direct physical internal path now has a positive
user observation. The exact ARP pitch source/configuration and reason for no
generated notes remain unconfirmed. Squeal is no longer observed by the user;
its original cause is not established and DSP was not changed.

## ARP source and CHORD follow-up

User confirms HELD ARP works; other ARP sources did not sound. User also
reports CHORDS leaves MIDI notes stuck on SEQTRAK until receiver track restart.
Exact source/context and chord switching sequence remain pending.

Focused host groups arp, trace, keyboard, ownership pass. A temporary integration
probe (/tmp/gp-arp-midi-probe.cpp) connects real PerformanceKeyboard to real
UsbMidiOutput with a fake accepting wire transport. HELD/STOP produces balanced
On/Off pairs: 16/16 at 1ms service, 6/6 at 40ms, 3/3 at 80ms. Stale NoteOn
drops respectively 0, 10, 13; delayed service can lose attacks, but this does not
prove the physical source-specific silence cause. Switching to Major with
a held physical key and releasing leaves zero MIDI gates with ARP on or off.
Neither host probe reproduces the reported physical stuck notes.

MEMORY requires captured intervals and a held/latched root. HARMONY and ROLE
require a valid harmonic snapshot; project scale alone does not create that
snapshot. Firmware obtains it from generated origin/recipe and a coherent
Song position/material; capture can retain a Song position while stopped,
so a blanket claim that transport must be playing would be inaccurate.
No additional behavioral changes or flash were made during this investigation.

## Correction to physical CHORD observation

User clarifies transport was stopped. HELD ARP works. The stuck CHORD sound
is INTERNAL to GroovePuter, not stuck MIDI on SEQTRAK. It occurs without
ARP when multiple chord notes are triggered. Earlier MIDI attribution above
is superseded; MIDI panic is not an established cause. A real-engine PCM
release probe for both internal TB303 voices and mono/poly chord switching
has been added to test_performance_boundary_ui.cpp; results pending.
Requested physical held-count/live-note and latch state after release to
distinguish lost ownership cleanup from ongoing DSP/FX sound. No fix yet.

Real-engine CHORD probe results (temporary -O0 compiler wrapper): both TB303
voices, MONO/POLY, Major selected before press or with a key held, two held
physical keys followed by releasing both: all eight scenarios report live=-1
and final releasePeak=0 after 256 audio blocks. Initial runner execution
segfaulted (exit139, output buffered); gdb rerun exited normally, an unbuffered
rerun passed, and ten subsequent ordinary executions exited0. Initial crash
remains unexplained; this is not unconditional gate closure or proof that
the physical hanging sound is fixed. No production change was made.

## EUCLID OFF / physical DSP diagnostic

User reproduces no hang with EUCLID ON, then hang after setting OFF and
playing a note, transport stopped. h=0 and liveA/B=-1; both MONO and POLY.
New notes coexist with the old sound until switching voice mode or starting
transport. These observations do not prove a DSP-envelope bug: liveNote is
logical engine bookkeeping, not a measurement of the physical envelope/PCM.

The host crash is explained by its core: drawWaveformOverlay ->
drawPerformanceHud -> MiniAcidDisplay::update. The test constructed MiniAcid
but omitted engine.init(), leaving waveform counts uninitialized. Added
engine.init() before UI/audio testing. The full boundary runner and a repeat
now pass: eight direct-chord scenarios and four EUCLID ON -> OFF scenarios
(two voice modes, disable before re-press or while held), live=-1 and final
PCM peak0. Prior uninitialized-fixture success claims are superseded by these
initialized results. Production initialization was already present.

Additional read-only diagnostic getters are compiled only with
GROOVEPUTER_USB_ACCEPT_DIAG. Fn+D TB303 footer now replaces the patch rows:
- DSP A<active>:<amplitude envelope> B<active>:<amplitude envelope> P<PCM peak>.
  Envelope -1 means that voice is not TB303. P is the final waveform block peak,
  not an isolated dry synth measurement. Snapshot is under audio mutation gate.
- FX D<selected synth delay enabled> T<tape FX enabled> L<tape mode enum>
  Q<scheduled event depth> DROP<cumulative stale generated NoteOn drops>.

No new resident state, per-sample instrumentation, routing, DSP behavior,
stack, DMA or synth settings changes. This is a diagnostic image, not a fix.

Build build/review-euclid-diagnostic-20261014, same ADV PSRAM-disabled
huge_app/CDCOnBoot=default diagnostic profile. FS1B candidate-only link PASS,
flash1670974, static DRAM189652 (provisional budget191488).
ELF SHA256 a0305162180952fef1c397887710b30a903d90e89b1e3bb87ebc7130c95eace4
BIN SHA256 df5d49f7d3abb1a6db69005bd622ab24a2de792aea6e23b8547b1884c1421cec
Build log /tmp/gp-euclid-diagnostic-build.log; test log
/tmp/gp-euclid-initialized-test.log. No ttyACM/ttyUSB port currently present,
so this image has NOT been flashed. Device still has prior audio diagnostic.

Explicit Opt+held-note HOLD idea captured in personal inbox for separate
behavior design; accidental hanging is not retained as the feature contract.

Final C++17 compatibility rebuild supersedes the previous hashes above for
review-euclid-diagnostic-20261014; see
[experimental checkpoint](2026-10-14-performance-experimental-checkpoint.md).
FS1B and static budget pass, no hardware upload.
