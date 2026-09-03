# GF2 Magnitude Contract

A standing requirement for every semantic checkpoint in the GF2 integration
series. Written after GF2-I2, because I2 is the case that shows why it is
needed.

## The failure it prevents

GF2-I2 made the generation profile's FEEL prior causal. Every test passed. The
target matrix was green. Remote CI was green. The build was flashed, and the
musician could not hear any difference when changing PROFILE.

Nothing was broken. The chain was correct end to end — resolution, propagation,
materialization, and playback all verified. The effect was simply below the
threshold of perception: at the shipped default FEEL AMOUNT, the entire profile
character amounted to **one event displaced by 7 ms**.

The checkpoint answered "is it connected?" and reported PASS. Nobody had asked
"is it big enough to exist?"

```text
causally wired   ≠   musically real
```

Both claims are worth making. They need different evidence.

## The rule

> Every semantic checkpoint must define, **before implementation**, a measurable
> magnitude threshold for its effect, at shipped defaults, on a named reference
> corpus — and pin that threshold as named constants in a regression test.

Three parts, all load-bearing:

**Before implementation.** A threshold chosen after seeing the result is not a
threshold, it is a description. Pick it at the design gate, from the measurement,
alongside the musician.

**At shipped defaults.** Not at maximum setting, not on the one fixture that
shows the effect best. If the effect requires the musician to find a control
first, the checkpoint has not delivered it.

**On a named reference corpus.** A single recipe proves nothing about the
instrument. The corpus must include at least one case where the effect is
expected to be strong, one where it is expected to be weak, and one where it is
expected to be absent by design — and the test must assert all three, so an
expected zero is distinguishable from a broken zero.

## Required form

The threshold is a statement about the material, not about the code:

```text
At the shipped default <control>, on <named corpus>, every <non-neutral
identity> must produce at least N <units of difference> against the neutral
identity, and no two identities may produce identical output.
```

The mutual-distinctness half matters as much as the magnitude half. An effect
that is large but identical across all identities is one effect with several
names, which is exactly what Gate B exists to detect.

## The anchor rule

Magnitude is not only how much moved, but **what** moved.

On Minimal Space under LAID BACK, ten events were displaced and the kick was not
one of them — and the bar contains exactly one kick. Displacement measured
against nothing steady is not perceived as displacement. Any magnitude metric
for a timing- or placement-shaped checkpoint must therefore report separately
whether the anchor element moved, and the threshold must say what it requires of
the anchor.

The same idea generalizes: for a density checkpoint, whether the change lands on
structurally salient positions or only on ornament; for a phrase checkpoint,
whether the difference falls where a listener is attending.

## What this is not

- **Not a replacement for causality tests.** Ownership, single-resolution,
  determinism, topology invariance and persistence contracts all still apply. The
  magnitude contract is additional.
- **Not a licence to retune.** Discovering that an effect is too small is a
  finding to report, not permission to edit vocabularies, weights or defaults
  inside the same checkpoint. Amplitude decisions belong to the musician and, if
  they are large, to their own checkpoint — GF2-I2A is the precedent.
- **Not a taste judgement.** The threshold is a number on a corpus. Whether the
  result is *good* is a separate conversation; whether it *exists* is not.

## Where it applies

| checkpoint | plausible magnitude metric | neutral reference |
|---|---|---|
| GF2-I2A | events displaced and max offset in ticks/ms at the default FEEL AMOUNT; anchor moved yes/no | STRAIGHT |
| GF2-I3 | material difference between bar 1 and bars 2/3/4 of one phrase, per law | Loop |
| GF2-I4 | onset count spread between the sparsest and densest corridor at defaults | corridor bounds ignored |
| GF2-I5 | events attributable to the secondary role, per role depth | Chord at minimum depth |

The metrics above are starting points for each checkpoint's design gate, not
prescriptions. What is prescribed is that each checkpoint arrives at one,
declares it before implementing, and leaves it behind as a test.

## Reporting

Every checkpoint's final report gains one line that cannot be answered by the
test suite alone:

```text
MAGNITUDE AT DEFAULTS   <metric> = <value>   threshold <N>   PASS / FAIL
ANCHOR                  moved / not moved
```

A checkpoint may close with a FAIL on this line — GF2-I2 did, deliberately, and
recorded why. What it may not do is omit the line and report PASS.
