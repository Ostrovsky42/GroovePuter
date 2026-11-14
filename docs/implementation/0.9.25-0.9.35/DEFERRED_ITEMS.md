# Deferred items for 0.9.25–0.9.35

This list separates known unfinished work from verified baseline behavior. Items remain deferred until a later milestone explicitly accepts them.

## Carried from 0.9.24

- **H1 — LIVELY form intensity:** remains blocked. The attempted CALM/NORMAL/LIVELY event ordering did not meet the intended counts and was reverted. No sound-quality claim is made.
- **H10 — form listening kit:** remains skipped because it depended on H1. No owner listening has occurred.
- **Hardware acceptance of 0.9.24:** not performed. It is not a prerequisite for host-side work in this goal.

## Baseline gaps for later milestones

- **Live route/profile rebinding:** active USB output consumes a startup snapshot. Profile edits are persisted for the next boot; they do not update running MIDI lanes. Preserve that explicit behavior until a bounded, ownership-aware rebind transaction is implemented and tested.
- **Custom Performance route table:** current persisted settings cannot fully describe A/B/DX/drum Performance routes. Custom projection fails closed; do not infer mappings from Pattern routes.
- **Integrated route and endpoint lifecycle traces:** component tests cover ownership, pending releases, tee failover and UART queue pressure, but tests still need to prove the complete dispatcher path across route changes, per-endpoint loss/rejoin, project replacement and Song transitions.
- **SMF v5 fixture:** source has a v5 reader, but the reviewed legacy fixtures directly cover v3 and v4 only. Add a v5 fixture before changing `.gpp` storage.
- **Public MIDI export:** current SMF parser/player/import paths do not provide user-facing export of the current Song. This remains planned for 0.9.33.
- **M5Launcher package path:** the existing guide is a draft and the uploader flashes an image; exact Launcher-compatible packaging has not been established.
- **User documentation alignment:** README/navigation describe v0.9.19 and the manual says v0.9.22. Product documentation refresh belongs to 0.9.34.
- **Release-profile measurement:** local baseline firmware was built with `CDCOnBoot=cdc`. A separate `CDCOnBoot=default` FS1B baseline is required before release-profile byte comparisons.
- **Hardware and musical acceptance:** all physical keyboard, internal audio, USB MIDI, DIN MIDI and SEQTRAK behavior remains **NOT TESTED** until the owner accepts the 0.9.35 candidate.
