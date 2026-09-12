# MATERIAL-R0 Reconciliation Design

## Purpose

MATERIAL-R0 collapses the two remaining 0.9.11 production authorities into one exact descendant before MW-L begins.

Input authorities:

- A2-B descendant authority: `9faedc67b9d5dcc7db3e932f01a314e54eb4d11f`
- MW-K Working/modified-state authority: `ac5a77267428aa55ffaf0e92b7977203b0e2bbc0`
- Observed merge-base: `3af3807be3c3e1218fe85376cc83381e95261119`

`9faedc67` already contains merge commit `85f5f758c9b3bec441d5e3fd258a2be55281646b` (`fix(0.9.11): unify M-WORKING with identity-bound material authority`). Therefore MATERIAL-R0 does not re-integrate M-WORKING from scratch. It reconciles the post-merge MW-K capability line with the identity-bound A2/A2-B authority and the later audible Pattern publication fix.

## Authority model

Stable `MaterialId` identity remains authoritative. Slot/page/bank/pattern coordinates may locate a payload, cache a target, or address legacy Pattern storage, but they may not become the identity proof for a Material operation.

A replacement at the same storage address is a different Material if its `MaterialId` differs. Ordinary edits preserve identity. A2/A2-B identity resolution and promotion remain fail-closed when the supplied identity does not resolve to the expected resident material.

MATERIAL-R0 must not reintroduce any production caller that promotes or resolves a Material by bare address alone.

## Working storage

There remains exactly one bounded session-owned `WorkingMaterialStorage` per synth voice.

Requirements:

- `sizeof(WorkingMaterialStorage) <= sizeof(RuntimeSynthEventBuffer)` remains a compile-time invariant.
- Pattern and Melody reuse the same already-paid storage footprint.
- Working has an explicit empty state distinct from a valid empty Melody.
- Pattern target metadata stored inside the bounded payload is locator metadata, not Material identity authority.
- Pattern/ Melody playback-source toggles must not implicitly replace retained Working material.

No second musical owner or sidecar payload allocation is introduced in MATERIAL-R0.

## Manual Pattern edit ownership

Manual note editing must mutate Working, not the accepted Scene Pattern directly.

The control-side transaction is:

1. resolve the exact current editable Pattern target;
2. prepare a complete Pattern value;
3. publish its runtime/audible representation through the existing audio mutation/publication boundary;
4. store the same value as Working;
5. leave Accepted unchanged until a later explicit lifecycle operation.

The existing audible Pattern publication guarantee from `9faedc67` must remain true.

## Modified state

MW-K modified-state is derived, not a manually maintained dirty flag.

For the exact bound Working Pattern, `modified` is true iff Working content differs from the corresponding Accepted Pattern content. A Working payload bound to another locator is not evidence that the current material is modified.

The comparison must survive Pattern/Melody playback-source toggles because playback representation is not Working identity.

MATERIAL-R0 does not add ACCEPT, DISCARD, retarget refusal, or Working-aware Undo. Those are later lifecycle slices beginning with MW-L.

## UI ownership

Legacy Pattern editor and Sequencer Hub note rendering/editing must read and mutate the current Working Pattern when it is the bound editable payload. They may fall back to Accepted only when no matching Working Pattern exists.

UI navigation or Pattern/Phrase representation changes must not themselves accept, discard, or replace Working.

## Preserved gates

The exact MATERIAL-R0 descendant must preserve all previously closed behavior:

- stable MaterialId persistence and legacy compatibility;
- A2 identity-bound resolution;
- A2-B identity-bound promotion transaction;
- no production bare-address promotion callers;
- bounded Working storage;
- Melody Working compatibility;
- manual Pattern Working edit transaction;
- derived MW-K modified-state;
- M3 ACTIVE authority;
- M4 NEXT causality;
- P2/project persistence contracts;
- audible Pattern runtime publication;
- existing M-WORKING behavioral gates with no hard STOP.

## Embedded acceptance

MATERIAL-R0 closes only after:

- all exact-head host/contract gates are GREEN;
- no `M-WORKING STOP` is emitted;
- strict Cardputer ADV build succeeds;
- static DRAM is `<= 191488 B`.

Final ELF/BIN release provenance and physical `G / Alt+G / Undo` acceptance are deliberately deferred until the later lifecycle/release candidate. MATERIAL-R0 may produce build evidence, but not a final hardware certificate.

## Out of scope

MATERIAL-R0 does not implement:

- MW-L manual/editor retarget refusal;
- ACCEPT or DISCARD;
- Working-aware Undo/Redo or recovery;
- LENGTH / MAKE PHRASE lifecycle changes;
- legacy mutation census beyond regressions needed for reconciliation;
- new Melody SD persistence ownership;
- Hybrid Song orchestration;
- G4 genre expansion;
- NanoKEY2/MIDI integration;
- broad UI redesign.

## Completion authority

When the acceptance gates pass, the resulting exact SHA becomes the only 0.9.11 Material production development root. The old A2-B and M-WORKING/MW-K branches remain evidence/history only. MW-L begins only from this MATERIAL-R0 descendant.
