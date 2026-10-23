# DSP preset selector

User approved replacing SOURCE with presets; source switching remains Alt+R.

Design: reuse SelectionPopup, replace MORE row SRC with PRESET. Enter opens a three-item engine-specific list, confirmation applies to the current voice only; Escape cancels. Left/right step presets. Unsupported engines show an unavailable row. Derive name/CUSTOM from actual parameter values, including after Load. Preserve source/material, routing, volume and FX. Alt+R also operates from params. No DSP algorithm changes.

Plan:
1. Add real-engine regression for recipe selection, CUSTOM, unsupported engine and A/B isolation; demonstrate failure before implementation.
2. Add MiniAcid recipe application/matching through existing parameter APIs. Wire row, modal input/draw and Alt+R.
3. Run project/preset tests, SDL and ADV builds; inspect software behavior and memory. Hardware acceptance remains separate.

Software verification complete: clean real-engine tests, SDL and ADV builds PASS. Hardware selector check and runtime heap remain OPEN.
