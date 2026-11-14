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

## 0.9.29 follow-up

- **Macro listening and physical control feel:** SDL verifies readable values and actions only. Whether the four mappings sound useful and whether the Cardputer keyboard is comfortable remains pending for owner acceptance of 0.9.35.
- **Per-loop mutation and permanent commit:** MUTATION is deterministic per source event, and live macro output does not add events or edit the Phrase. Per-loop variation and explicit materialization are not included.
- **Phrase integration in the host suite:** the focused P3 gate covers it; the canonical host suite does not currently invoke `run_pattern_phrase_p3_tests.sh`.

## 0.9.30 follow-up

- **Live chord-aware macro TENSION:** Graph and Legacy harmony already constrain generated material before commit, and current Song chord snapshots reject stale participants. The sequencer macro owner still receives only project key/scale. Keep TENSION on its bounded next-onset heuristic until a fixed, coherent control-side chord publication is available; do not read Song harmony storage in the sequencer callback. See `HARMONIC_FOLLOW.md`.
- **Legacy M1-P1 expectation test:** `run_0_9_9_m1_p1_tests.sh` expects `SparseCall` and `Mirror` for the Electro profile. It fails at the exact 0.9.24 baseline because the current profile selects `AngularCell` and also differs on the next literal expectation. This test contract is not updated in 0.9.30; see the verification record.

## 0.9.31 follow-up

- **Arrangement listening:** INTRO, MAIN, DEVELOPMENT, BREAK, BUILD, DROP and OUTRO are bounded starting contours over existing energy levels and source rows. Their musical usefulness has not been evaluated by listening and remains pending for owner acceptance of 0.9.35.
- **Per-section identity and motif development:** role names are transient FORM labels; materialized Song rows remain ordinary editable positions. The templates do not create independent motif variations or persist section-role metadata.

## 0.9.32 follow-up

- **Owner listening and performance feel:** host transforms and SDL readability are checked, but macro combinations, perceived timing, internal sound and physical controls need owner evaluation with the 0.9.35 candidate.
- **Pitch Bend, aftertouch and motion/gesture input:** deferred P2. Add only after output-route support and lifecycle behavior are specified. No Cardputer IMU is assumed.
- **Macro-to-CC mapping and automatic expression:** deferred; macros remain musical transformations and do not send controller messages.
- **Live profile rebinding:** still unsupported. The active output uses its startup route snapshot; profile changes take effect on the next startup.
- **Hardware MIDI claims:** USB enumeration, DIN behavior and Yamaha SEQTRAK reception remain `NOT TESTED` until owner acceptance of 0.9.35.
