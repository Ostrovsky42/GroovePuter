# Phrase Arranger Stage 2 compact test sequence

```text
1. Create A=4B, B=2B, C=1B, D=2B on PHRASE CORE.
2. Tab to PHRASE ARRANGE.
3. Enter 1 1 2 1 3 1 2 4.
4. Confirm A A B A C A B D and TOTAL 23B.
5. Confirm arrows navigate only; reorder requires reassignment.
6. Press plain Backspace: expect no change.
7. Press Ctrl+Backspace: remove one selected position and close the gap.
8. Press Ctrl+Shift+Backspace: clear the complete chain.
9. Rebuild A A B A C A B D.
10. Move to an empty 23-row Song destination and press W.
11. Confirm 23 consecutive rows and expected playback form.
12. Repeat W on the same destination: expect OCCUPIED and no change.
13. Press Alt+W: expect full overwrite; Wave Overlay must not toggle.
14. Leave Phrase and press Alt+W: Wave Overlay must toggle.
15. In CORE, press Ctrl+Backspace: selected Phrase must remain intact.
16. Clear Phrase B with unmodified Backspace in CORE: all B chain entries disappear.
17. Set A/B/C/D to 8B and fill all 16 positions: expect TOTAL 128B.
18. Write from zero-based row 0 in a fresh Song: exact 128-row success.
19. Repeat from zero-based row 1 in another fresh Song: RANGE and no changes.
20. Reboot after autosave: remaining chain must restore exactly.
```
