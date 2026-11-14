# Golden Path at the 0.9.24 baseline

Baseline: `28918d9b78e9a527ce2b5ce201a465af5126f76a`. This is an implementation inventory, not a claim that the full end-to-end scenario has passed.

| Step | Baseline capability | Evidence / gap | State |
|---|---|---|---|
| First launch | Quickstart, on-device help, workspace launcher and SDL application exist. | README/docs navigation still describes 0.9.19 and the manual says 0.9.22; reconcile before productization. | PARTIAL |
| Generate an idea | Genre, rhythm, harmony and phrase generation can materialize editable Scene data. | Host generation stages and golden MIDI/TSV probes exist; broad end-to-end repeatability still needs one scripted path. | PARTIAL |
| Keep a result | Pattern, Phrase and Song material workflows can accept and edit material. | Ownership and transactional paths exist; integrate into one user-level scenario. | PARTIAL |
| Develop it | Bounded phrase/rhythm development and Undo owners exist. | Confirm the candidate's exact user sequence and protected-anchor behavior in SDL. | PARTIAL |
| Arrange a Song | Song/Phrase/FORM and section-transition mechanisms exist. | No single assisted composition path yet maps all requested section roles; reuse existing structures. | PARTIAL |
| Perform | Performance keyboard and transport are present. | The four requested musical macros are not implemented. | GAP |
| Route MIDI | USB/UART output, role projections and profiles exist. | No Internal profile entry; safe live route-change transaction is not exposed; test endpoint-specific failures. | PARTIAL |
| Save and restore | Scene JSON, transactional SD save, pattern-page persistence and migration code exist. | Add/verify old-version fixtures, including `.gpp` v5; run full save/reload scenario. | PARTIAL |
| Export MIDI | MIDI files can be parsed, streamed and played; import can project file events. | No user-facing SMF export of the current Song is documented or present. | GAP |
| Hardware acceptance | A Cardputer ADV FS1B firmware build path exists. | No test of this integration candidate has run on hardware; first device run is reserved for 0.9.35 owner acceptance. | PENDING_HARDWARE_ACCEPTANCE |

## Verification boundary

Host tests and SDL may establish software state and UI flow. They cannot establish physical Cardputer keyboard ergonomics, internal audio quality, USB Host/MIDI endpoint behavior, electrical role/VBUS behavior, SEQTRAK compatibility, or physical timing. Record those only in `HARDWARE_TEST_0_9_35.md` after owner testing.
