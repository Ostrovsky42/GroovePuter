# P1 — Audible Chord Quality Feasibility

**Status:** static feasibility decision / physical internal-audio admission blocked  
**Production base:** `fc42763e7798866e61895bf1b8d62339ec59e0a7`  
**H6 dependency:** decision context only; no H6 runtime vocabulary is added here

## Decision

P1 answers exactly one question:

> What is the smallest bounded contract that can make Major / Minor / supported `ChordQuality` materially different in pitch without creating a new rhythm owner?

The answer is split by the ownership boundary rather than collapsed into a misleading single GO:

```text
bounded semantic pitch-set contract        GO
ESP32-S3 static projector resources        GO
external/MIDI binding architecture         FEASIBLE / hardware lifecycle audition pending
current universal internal-audio binding   NO-GO on IMonoSynthVoice
H6 harmonic runtime admission              BLOCKED
```

The selected semantic contract is a bounded multi-note chord-quality projection with at most four tones for one already-owned harmonic event. The current internal synth architecture cannot render that pitch-set as a simultaneous chord without adding a new bounded physical owner or changing the mono synth contract. P1 therefore does **not** admit H6 harmonic vocabulary.

P1 does **not** add the eight H6 progressions, new ChordRhythm masks, genre routing, Scene state, P-level behavior, inversions, voice leading, open/closed voicings, drop voicings, SATB, dynamic spreading or arpeggiation.

## Frozen Stage15 facts

The current Stage15 tonal path is monophonic per role onset:

```text
TonalMaterializationPlan
  onsets
  continuations
  onsetCount
  onsetSteps[16]
  midiNotes[16]     // one absolute MIDI note per role onset
```

The physical swappable synth contract is also explicitly monophonic:

```text
IMonoSynthVoice::startNote(float freqHz, ...)
SwappableSynthVoice : IMonoSynthVoice
```

Therefore enum presence alone cannot make chord quality audible, and a four-tone semantic result cannot simply be copied into the existing `SynthPattern` / `IMonoSynthVoice` path.

## Architecture options

| Option | P1 verdict | Reason |
|---|---|---|
| Bounded multi-note chord-quality projection | **GO as semantic contract** | Keeps one harmonic event while returning a fixed pitch-set of at most four tones. No timing fields exist in the API. |
| Synth-owned chord/quality representation | **HOLD / fallback** | Could preserve one trigger, but would expand or fork the current common mono-engine contract across TB303/SID/AY/SH101/SN76489/WAVEMORPH. P1 does not assign harmony semantics to each engine. |
| Arpeggiation / extra temporal NoteOn steps | **REJECT** | Converts pitch quality into new timing events and violates ChordRhythm ownership. |
| Split one chord across Synth A + Synth B | **REJECT** | Consumes independent role ownership and conflicts with bass/secondary/melodic routing. |

## Selected bounded contract

P1 introduces an isolated `ChordQualityProjectionRequest -> ChordQualityProjectionPlan` boundary.

```text
HarmonicEvent
  degree
  quality
  rootOffsetSemitones

+ explicit TriadPolarity for generic ChordQuality::Triad
+ global root / ScaleType
+ register corridor

        |
        v

ChordQualityPitchSet
  toneCount <= 4
  rootAnchorMidi
  midiNotes[4]
```

The request intentionally contains **no**:

```text
StepMask
onset positions
continuations
retrigger state
SynthPattern
DrumPattern
MIDI transport
```

It is structurally incapable of adding or moving rhythm events.

Absolute MIDI conversion stays delegated to the existing `TonalProjector`. P1 supplies exact semitone intervals relative to the harmonic root and does not create a second absolute-pitch authority.

## Bounded quality signatures

These are fixed quality-signature pitch sets, not a general voicing engine:

```text
Triad Major   0  4  7
Triad Minor   0  3  7
Minor7        0  3  7 10
Major7        0  4  7 11
Dominant7     0  4  7 10
Sus4          0  5  7
Diminished    0  3  6
Minor9        0  3 10 14   // fifth omitted under 4-tone cap
Major9        0  4 11 14   // fifth omitted under 4-tone cap
```

For `ChordQuality::Triad`, P1 refuses to infer Major/Minor polarity silently from scale. The prototype carries explicit `TriadPolarity`; a later production PR must decide where that semantic bit belongs before H6 harmonic vocabulary is admitted.

## Musical feasibility matrix

Host acceptance proves:

```text
same root: Major != Minor pitch-set
same root: Major7 != Dominant7 != Minor7
same root path: quality change changes pitch-set
root / scale / chromatic offset transpose the complete pitch-set coherently
all current ChordQuality values remain <= 4 tones
narrow register failure is atomic
```

Negative ownership invariants are source-gated:

```text
quality projection MUST NOT know chord onset positions
quality projection MUST NOT know continuation/retrigger topology
quality projection MUST NOT write SynthPattern
quality projection MUST NOT emit MIDI
quality projection MUST NOT touch drums/bass/melodic rhythm/P-level state
```

The PR does not wire the projector into the live migration path. Therefore current Stage15 behavior remains the only product behavior while P1 is a feasibility branch.

## Host result

On the P1 candidate before history squash:

```text
ownership/source regressions   PASS
GCC C++17                      PASS
Clang C++17                    PASS
ASAN + UBSAN                   PASS
host -Os stack usage           144 B   (gate <= 192 B)
```

This is sufficient to call the bounded semantic pitch-set representation **GO** as a deterministic host contract.

## ESP32-S3 static resource result

The P1 workflow builds the exact Cardputer ADV source in `normal` and `midi-only` profiles twice:

```text
default product link
probe-retained product link (-u grooveputerP1ChordQualityProbe)
```

The probe symbol has no runtime side effect and is not declared as a product API. Forced retention measures the actual ESP32-S3 linked cost of the isolated projector without wiring it into live generation.

Measured evidence:

```text
NORMAL
  linked text delta          +596 B
  linked data delta           +32 B
  linked text+data delta     +628 B
  fixed DRAM delta             +0 B
  ELF bss delta                +0 B
  projector object          985/0/0 B text/data/bss
  Xtensa projector stack      128 B

MIDI-ONLY
  linked text delta          +580 B
  linked data delta           +32 B
  linked text+data delta     +612 B
  fixed DRAM delta             +0 B
  ELF bss delta                +0 B
  projector object          985/0/0 B text/data/bss
  Xtensa projector stack      128 B
```

Feasibility gates:

```text
linked text+data delta <= 2048 B    PASS
fixed DRAM delta      <= 16 B       PASS
ELF bss delta         <= 16 B       PASS
Xtensa stack          <= 256 B      PASS
```

The exact `P1_RESOURCE` rows are hard-gated in CI for both profiles. These measurements cover only the isolated pitch-set projector. They do **not** estimate the cost of a future polyphonic internal-audio owner.

No runtime heap or largest-internal-block delta is claimed for this isolated projector because it allocates no persistent state and is intentionally not called by the product. Those measurements become meaningful only after a physical renderer exists.

## Physical binding verdicts

P1 keeps three separate verdicts:

1. **Semantic pitch-set feasibility — GO.** Existing harmonic semantics can produce a bounded exact pitch-set cheaply and deterministically.
2. **External/MIDI architecture — FEASIBLE, not hardware-accepted.** Existing USB-MIDI transport already has ordinary note-on/note-off operations, so a bounded simultaneous note-set does not inherently require sequencer step creation. Note lifecycle, stop/route cleanup and actual receiver audition still require a separate physical prototype before production admission.
3. **Universal internal audio on current architecture — NO-GO.** The shared physical synth boundary is `IMonoSynthVoice`; the current one-note `SynthPattern` adapter cannot render a 3–4-tone simultaneous pitch-set. P1 rejects stealing Synth A/B, hidden arpeggiation, or engine-specific semantic hacks as substitutes.

This is an ownership blocker, not a projector memory blocker.

## Hardware-only gate for any future physical renderer

CI cannot prove audio-task CPU or musical quality for a renderer that does not yet exist. A later physical prototype must exercise:

```text
4-note quality set
+ dense existing ChordRhythm
+ drums
+ bass
+ secondary track
+ MIDI output
```

and measure/accept:

```text
runtime CPU/audio underruns
largest internal block
heap watermark
control/audio task stack watermark
Major vs Minor audibility
Triad vs seventh audibility
unchanged onset/retrigger topology
feature OFF = exact Stage15 behavior
```

P1 deliberately does not fabricate these numbers from the static projector probe.

## Admission rule

P1 is a feasibility PR, not H6 vocabulary admission.

```text
bounded semantic contract                 GO
static ESP32 projector resources          GO
current universal internal renderer       NO-GO
                 |
                 v
H6 harmonic P4 remains BLOCKED
                 |
                 +--> choose/prove a bounded physical chord owner
                 +--> run hardware/runtime resource acceptance
                 +--> only then admit bounded H6 harmonic vocabulary
```

The preferred semantic boundary is therefore retained, but **direct integration into the current mono internal-audio path is rejected**.

P2 multi-bar ChordRhythm and P3 same-chord retrigger remain independent owners and do not need to wait for a future physical chord renderer.
