# G6 nonresident Song material — FAIL / not repaired

Actual existing workflow builds accepted eight-bar A/B Melody on page1, returns to page0, and saves Song refs to global slot16. With its Melody files damaged, Load succeeds because validation covers resident page0 only. The Song references unusable material. Primary reproducer/log attached; host only, physical NOT TESTED. The independent code review also identifies this remaining boundary.

Next repair must validate referenced material without a second Scene or overwriting staged resident banks. Preserve page A/B generation, legacy fallback and JSON-only project compatibility. Do not reject a legitimate legacy project merely to avoid staging complexity. This is a known release blocker, not covered by the passing narrow G6 regression suite.
