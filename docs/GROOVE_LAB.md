# Groove Lab — Historical Page Note

## Status in 0.9.1

The old standalone **GROOVE LAB / Mode Page is retired** and is not an active 0.9.1
workflow page.

The current GENERATE workflow is:

```text
GENRE 1/2 -> FEEL 2/2
```

Do not use this document as a current key map or runtime ownership contract. The old
Mode/Flavor control surface, `generation_page.cpp` UI and direct `applySoundMacros`
workflow described by earlier revisions were implementation history and should not be
revived as a second musical owner.

## Current ownership

0.9.1 uses these release-facing boundaries:

- **GENRE** — musical corridor, Variant/recipe, Rhythm identity, Apply policy and
  explicit full Stage 15 generation;
- **FEEL** — timing profile, swing, bounded feel amount, velocity variation, repeat
  cycle and FEEL presets;
- **P1/P2/P3 request state** — session-scoped realization strength shared by current
  generation surfaces;
- **Synth A/B KNOBS/MORE** — synth TYPE, parameters and sound design;
- **Tonal / rhythm generation stack** — current production generation implementation,
  without exposing a second Mode Page owner.

Legacy `GrooveboxMode`/Flavor data may still exist inside compatibility or generation
implementation paths. Their presence does not make the retired GROOVE LAB page
reachable.

## Current controls

Use:

- [`../README.md`](../README.md) for the release overview;
- [`../MANUAL.md`](../MANUAL.md) for the 0.9.1 workflow manual;
- [`../src/ui/docs/keys.md`](../src/ui/docs/keys.md) for the canonical key map;
- [`releases/0_9_1_RELEASE.md`](releases/0_9_1_RELEASE.md) for the release freeze.

## Historical purpose

Earlier GroovePuter revisions used GROOVE LAB to expose Mode/Flavor corridors and
sound-macro experiments. Those revisions are useful architecture history, but they do
not define current UI behavior. New post-0.9.1 work should extend the current
Genre/Feel/generation-request ownership model instead of restoring the removed page by
accident.
