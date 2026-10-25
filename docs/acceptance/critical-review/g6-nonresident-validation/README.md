# Stored nonresident material validation — software checkpoint

Base a65d99bf, accepted #495 ancestry. No push/merge/flash; physical device still #495.

RED: Song references accepted A+B Melody on page1/global16; corrupt payload files do not prevent successful Load. Additional RED: failed A reread after a newer invalid B is ignored by existing page selection. GREEN: before Scene publication, all existing pages and advertised Melody files validate; absent pages referenced by either song's SynthA/B/drums refuse. The vocal column is not a page reference. Failed A reread now propagates refusal/fallback.

A metadata-only reader reuses the same header/CRC decoder and candidate-selection order as normal page load. Bank bytes stream through64B chunks; only32 descriptors are retained. No Scene scratch/index mutation and no second Scene, owner or queue. This targeted extraction is required to preserve the already-parsed resident candidate. Legacy JSON-only page0 remains supported.

Tests: existing G6 suite; nonresident header/CRC/missing/oversized, pageCRC/pageMissing, drums-only missing page, JSON-only0 with valid nonresident page, and global255 boundary. Metadata v3–v6, generation choice, NVS precedence, alternate and legacy fallback, refusal preserving output, whole scratch-byte/index preservation. Metadata reader ASan/UBSan PASS. Save-keeps-Undo UI, source/memory instrumentation contracts, SDL and ADV PASS. Independent read-only review found no introduced Critical/Important defect and confirmed single-owner scope.

.data41996/.bss142256/fixed184252 unchanged; flash1595582 (+1332 over prepared-hydration repair; +3924 over #495). DRAM gate191488 unchanged. ESP32 compiler frames: Scene loader3216, metadata reader480, payload reader320B. Actual runtime heap/high-water/CPU/SD latency remain OPEN. Scan now rejects corrupt unreferenced existing pages too, intentionally stricter than the former partial check; extra SD reads must be timed on hardware.

This verifies stored files at Load time, not future SD reliability. Nonselected/nonresident material remains on SD; later faults can affect on-demand playback. Late runtime publication, failed selection-pointer restoration, default-scene fallback and physical acceptance remain separate boundaries. G6 NOT PASS. Power-cut atomicity UNVERIFIED.
