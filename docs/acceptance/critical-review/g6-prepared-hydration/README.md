# Prepared selected Melody — G6 narrow repair, acceptance OPEN

Parent5ecca63b, based on accepted #495. Hardware still runs #495. No push/merge/flash.

RED: storage returns a successfully validated target, then a fault damages its Melody files before MiniAcid hydration. The second SD read loses both selected Melody buffers while Load reports SUCCESS. GREEN: temporary A/B buffers are prepared before Scene publication and final reader acknowledgement; successful Load/boot hydration copies those buffers instead of reading SD again. Slots derive the same clamped JSON observer selections used by the committed manager, not duplicate selection state. No persistent field/owner/queue or second Scene was added.

The fault test accepts either a safe refusal retaining old canonical/project/Working or a complete prepared target with both eight-event Melody hashes intact. Post-preparation fault yields the complete target; a fault between initial validation and preparation refuses and retains old state. Intact loads, existing header/CRC/missing/nonselected/oversized tests, clean host suite, filter ownership, Save-keeps-Undo UI, source contracts, SDL and ADV PASS. Runtime memory instrumenter applies to both current source and clean #495; anchor adaptation preserves paired diagnostic builds.

Independent read-only review identifies no introduced Critical/Important functional defect. Recovery retries overwrite successful Melody preparation; Pattern slots ignore stale temporary contents. Ordinary low-level callers retain optional callback defaults. This internal callback threads existing storage preparation, not a new transaction framework.

.data41996/.bss142256/fixed184252 unchanged; flash1594250 (+2592 vs #495). ESP32 compiler frames: explicit Load2672, boot load2624, Scene loader2944, Melody loader2816, decoder1344, preparation helper64. Explicit-load chain subtotal9776B excluding storage/filter, function dispatch and caller frames; actual runtime high-water/heap remain OPEN. Build success is not stack headroom proof.

Nonresident Song references still FAIL; see ../g6-nonresident-open. Nonselected material is validated but retained on SD; later operations still perform reads. Runtime publication/failure, default-scene fallback behavior, physical SD faults and cold cycles remain OPEN. G6 is NOT PASS.
