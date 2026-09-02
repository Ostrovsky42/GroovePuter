# M1-A1 composition profile admission contract audit

## Purpose and ancestry

Audit-only checkpoint from `ac2cdbe06b4f12fae00e9c27187db3eeb49e6f8b`; M1-T1/T1F remain unchanged.

## Definitions and owner

Enum validity is 16 modes and 18 recipes. Route validity is separate. Resolver acceptance is separate. Direct composition-profile admission is owned by `kProfiles` in `src/generation/composition/generation_profile.cpp`: `definitionFor()` first performs exact `(generativeMode, recipe)` lookup. A second, later lookup explicitly falls back to the same mode's base recipe. Thus direct vs fallback is observable in production source and has 33 direct pairs.

## Fallback, UI, and 198

Fallback is the second lookup in `definitionFor`; it is not direct admission. UI `recipeChoicesForGenre()` and `normalizeRecipeForGenre()` restrict presentation choices, but do not replace the core owner. Search and history did not reproduce 198 from an existing production catalog, UI matrix, or historical fixture; its provenance is undocumented on this base.

## Candidate and decision

The preserved Electro/base/address-7 T6 evidence is directly admitted because `(Electro, 0)` is in `kProfiles`; this audit does not freeze a physical M1 fixture. Decision A: existing authority found. Next checkpoint is M1-T1F2, using `kProfiles` exact-membership semantics to classify fixtures. Production delta is zero; hard stop.
