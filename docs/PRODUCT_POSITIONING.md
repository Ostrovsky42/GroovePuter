# GroovePuter Product Positioning

> **Status:** product direction and architectural boundary document.
>
> This document describes what GroovePuter is intended to become and how future work
> should be prioritized. It is not a claim that every workflow below is already
> implemented in the current firmware.

## One-sentence positioning

**GroovePuter is a portable standalone groovebox and hardware musical brain that generates, varies, performs, and commits editable musical material for its own synths and drums, external instruments such as Yamaha SEQTRAK, and DAWs such as REAPER.**

It must remain useful with nothing connected, while becoming equally comfortable as a
companion/controller for external sound engines and as a source of clean, role-separated
MIDI material for a DAW.

## Product identity

GroovePuter is deliberately three things at once:

```text
                         GroovePuter
                              |
          +-------------------+-------------------+
          |                   |                   |
      STANDALONE          COMPANION           DAW BRAIN
          |                   |                   |
 internal synths         external synths       REAPER / VST
 drums + Song/Phrase     SEQTRAK first-class   editable MIDI clips
 live performance        generic MIDI devices  arrangement / mix
```

None of these modes is a fallback.

### 1. Standalone instrument

GroovePuter remains a self-contained groovebox on Cardputer ADV.

A user must be able to:

- generate rhythm, bass, harmonic and melodic material;
- edit patterns, Song and Phrase material;
- use the internal synth and drum engines;
- perform with NOTE mode and musical performance controls;
- save/load projects and continue working without a computer or external synth;
- use Song/Phrase as a compact arrangement environment.

External MIDI, SEQTRAK and REAPER are optional integrations, not runtime dependencies.

### 2. Hardware companion / external-instrument brain

GroovePuter can also be the musical front end for another synthesizer or groovebox.

The device owns musical structure and performance intent while the external instrument
may own sound generation.

Typical flow:

```text
GroovePuter
  generate / edit / vary / perform
              |
              v
        role-based MIDI
              |
     +--------+---------+
     |                  |
  SEQTRAK          other synths
```

Yamaha SEQTRAK is a **first-class reference integration**, but the core architecture
must remain generic. Musical material should not be encoded around SEQTRAK-specific
assumptions. Device-specific channel maps, capabilities and transport quirks belong in
Device Profiles and routing projections.

The same bass phrase that drives a SEQTRAK synth should also be able to drive a generic
hardware synth or a VST without changing its musical identity.

### 3. DAW musical brain

With REAPER, GroovePuter is not intended to become a second DAW.

Its role is to produce and perform useful editable material quickly:

```text
Generate
   -> Audition
   -> Regenerate / Variation
   -> Commit
   -> Record to REAPER
   -> arrange / automate / mix in the DAW
```

REAPER owns the long timeline, detailed editing, automation, comping, audio work,
plugin management, mixing and mastering.

GroovePuter owns musical decisions, bounded performance controls and fast generation
of reusable material.

## Product north star

A high-value end-to-end workflow is:

> In a few minutes, create synchronized **drums, bass, chords and melody**, audition
> alternatives through GroovePuter or SEQTRAK, commit the chosen material, and record
> it into separate REAPER tracks with stable timing and clean note lifecycle.

The project should increasingly be judged by how well it supports that workflow rather
than by the raw number of generators, genres, pages or internal arranger features.

## Priority order

### P0 — Reliable MIDI I/O and transport foundation

MIDI reliability is foundational for both companion and DAW use.

Required direction:

- explicit clock ownership;
- stable `Start / Stop / Continue` behavior;
- predictable NoteOn/NoteOff ownership;
- scoped cleanup and global panic;
- bounded input/output queues and dispatch;
- deterministic channel/role routing;
- no duplicate scheduler, transport or active-note owner;
- stable recording into REAPER without accumulated timing drift;
- Device Profiles for at least `Standalone`, `Generic MIDI`, `SEQTRAK`, and `REAPER`.

Clock source and output profile are orthogonal concepts:

```text
CLOCK SOURCE
  Internal
  External MIDI / DAW
  device-specific external clock where supported

OUTPUT PROFILE
  Standalone
  Generic MIDI
  SEQTRAK
  REAPER
```

Selecting a SEQTRAK profile must not silently imply that SEQTRAK owns the clock.

### P1 — Editable musical material

Generation is not an end in itself.

Rhythm, bass, chords, melodic phrases, variations, fills and endings should converge on
one idea: **bounded editable musical material**.

The generation architecture developed through Stages 7–15 is therefore a source of
musical clips/material, not merely a way to randomize the current Song.

Conceptually:

```text
Musical Generator
      |
      +-- Rhythm
      +-- Bass
      +-- Chords
      +-- Melody
      +-- Phrase / Variation / Fill / Ending
      |
      v
Editable material
      |
      +-- internal engines
      +-- SEQTRAK
      +-- generic MIDI synth
      +-- REAPER / VST
```

Sound destination must not determine generation semantics.

### P1 — Generate / Audition / Commit workflow

Generation should support fast exploration without treating every audition as a
persistent edit.

Target workflow:

```text
GENERATE
   |
   v
candidate material
   |
   +--> AUDITION
   |       |
   |       +--> VARIATION
   |       +--> REGENERATE
   |       +--> change role / density / energy
   |
   v
COMMIT
   |
   +--> persistent Scene material
   +--> Undo receipt / revision
   +--> activation at the correct musical boundary
```

Architectural ownership remains:

```text
PREPARE / candidate creation
        -> no persistent mutation
        -> no dirty revision
        -> no Undo receipt

COMMIT
        -> canonical persistent mutation
        -> exactly one Undo owner
        -> exactly one Scene revision

ACTIVATE
        -> runtime / musical boundary
        -> no second Undo
        -> no second persistent revision
```

This extends the existing PREPARE -> COMMIT -> ACTIVATE direction rather than replacing
it.

### P1 — DAW-friendly lengths

`1 / 2 / 4 / 8 bars` should be treated as a strong cross-system contract for generated
and captured musical material wherever musically applicable.

That makes GroovePuter material naturally map to DAW Items/clips and keeps Phrase/Song
composition predictable.

Examples:

```text
MAIN A      4 bars
MAIN B      4 bars
VARIATION   4 bars
BREAK       2 bars
FILL        1 bar
ENDING      2 bars
```

The device may support finer pattern grids internally, but exported/committed musical
sections should prefer these bounded lengths.

### P1 — Role-based MIDI routing

Musical roles should be primary; MIDI channels are a device-profile projection.

Canonical roles include at least:

```text
DRUMS
BASS
CHORDS
MELODY
```

A REAPER-oriented projection may be:

```text
DRUMS  -> CH1
BASS   -> CH2
CHORDS -> CH3
MELODY -> CH4
```

A SEQTRAK profile can map those same roles to SEQTRAK-compatible destinations. Generic
MIDI can expose configurable channel assignments.

Generators must not hardcode destination channels.

Preferred architecture:

```text
Musical Role
    -> Role Router
    -> Device Profile
    -> MIDI Channel / Destination
    -> existing MIDI dispatcher
```

The goal is simultaneous capture of independent roles into separate REAPER MIDI tracks
without producing one mixed stream that must be manually separated later.

### P2 — Musical performance macros

GroovePuter should favor controls that express musical intent:

- density;
- variation strength;
- fill / break / ending;
- energy;
- mute;
- transpose;
- regenerate one role;
- phrase/section switching;
- bounded swing/feel controls where appropriate.

A macro such as `ENERGY +1` may influence several musical dimensions at once while
remaining deterministic and bounded.

The device should **not** grow into a general DAW automation editor.

## Clear product boundary: what GroovePuter should not become

GroovePuter is not trying to replace REAPER.

Low-priority or out-of-scope directions include:

- large arbitrary automation-lane editors;
- a full mixer comparable to a DAW;
- mastering workflows;
- long unrestricted desktop-style arrangement timelines;
- plugin hosting/management as a central product feature;
- audio comping and detailed waveform editing;
- duplicating features that are substantially easier and more powerful in a DAW.

A useful filter is:

> If a control expresses a **musical decision**, it may belong in GroovePuter.
> If it expresses detailed **production automation**, it probably belongs in REAPER.

This does not remove Song mode. Song and Phrase remain valuable for standalone use,
performance structure, compact sections, capture and handoff. The boundary only means
that they should not be expanded into a desktop DAW clone.

## SEQTRAK integration principle

SEQTRAK remains the reference external hardware target because it complements
GroovePuter well as a portable sound engine and performance device.

However:

```text
Generic musical core
        |
        +--> SEQTRAK profile
        +--> REAPER profile
        +--> Generic MIDI profile
        +--> future device profiles
```

SEQTRAK-specific behavior belongs in:

- channel/destination mapping;
- drum-note projection;
- supported Program/CC behavior;
- transport/capability policy;
- receiver-mode quirks;
- validation and acceptance tests.

It should not leak into the core rhythm/bass/chord/melody representation.

## Standalone-first invariant

New MIDI/DAW work must not make the device dependent on a host.

The following remains a product invariant:

```text
No computer connected
No SEQTRAK connected
No external MIDI connected
        |
        v
GroovePuter still boots, generates, edits, performs, saves and plays music.
```

Where an external-output feature has no destination, it should degrade cleanly rather
than compromising local playback or project editing.

## Architectural invariants

Future work should preserve these rules:

1. **One owner per realtime responsibility.** Do not add a second transport, scheduler,
   MIDI dispatcher or active-note owner.
2. **GENRE != FEEL != GENERATION != SOUND.** Musical identity, timing feel, realization
   request and timbre remain separate domains.
3. **PREPARE != COMMIT != ACTIVATE.** Candidate creation, persistence and musical
   publication are separate phases.
4. **Role != channel.** Roles are musical; channels are projections selected by a
   Device Profile.
5. **Standalone is first-class.** External devices and DAWs remain optional.
6. **SEQTRAK is first-class, not hardcoded into the musical core.**
7. **Bounded realtime behavior.** No unbounded queues, filesystem work, JSON, heap churn,
   waits or new mutex ownership on critical audio/MIDI paths.
8. **Generated material must remain editable.** Avoid designs that only create opaque
   audio-like outcomes that cannot become patterns/phrases/MIDI events.
9. **Prefer 1/2/4/8-bar musical units** for reusable generated/captured sections.
10. **The DAW boundary is intentional.** GroovePuter should become better at musical
    structure and performance rather than broader at desktop production tasks.

## Roadmap interpretation

This positioning does not invalidate current release work.

The existing architecture maps naturally to it:

```text
0.9.8
  safe persistent editing
  PREPARE -> COMMIT -> Undo / revision
        |
        v
0.9.9
  material lifecycle
  COMMIT -> pending -> ACTIVATE
        |
        v
0.9.10 and MIDI closure work
  MIDI input/output, routing, lifecycle, clock/transport reliability
        |
        v
clip-oriented workflow
  Generate -> Audition -> Variation -> Commit
  1/2/4/8-bar reusable material
        |
        v
DAW workflow
  role-separated recording to REAPER
        |
        v
performance layer
  density / energy / fill / transpose / regenerate-role / mute
```

Release scope remains authoritative in each release document. This product-positioning
file should guide prioritization when choosing between competing future features.

## Product success criteria

A mature GroovePuter should pass all three tests.

### Standalone test

A musician can take only Cardputer ADV, create/edit a groove, arrange compact sections,
perform it and save the project.

### Companion test

A musician can connect an external synth or SEQTRAK, keep GroovePuter as the musical
brain, and play the same material through external sound generation with predictable
routing, clock and note cleanup.

### DAW test

A musician can connect REAPER, record several bars repeatedly without accumulated
clock drift, capture independent musical roles into separate MIDI tracks, replace the
sound source with VST instruments, and continue arranging/editing without reconstructing
GroovePuter's output by hand.

Passing all three is the intended product identity.