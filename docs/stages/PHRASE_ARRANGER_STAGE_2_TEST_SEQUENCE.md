# Phrase Arranger Stage 2 compact test sequence

```text
1. Create A=4B, B=2B, C=1B, D=2B on PHRASE CORE.
2. Tab to PHRASE ARRANGE.
3. Enter 1 1 2 1 3 1 2 4.
4. Confirm A A B A C A B D and TOTAL 23B.
5. Move to an empty 23-row Song destination and press W.
6. Confirm 23 consecutive rows and expected playback form.
7. Repeat W on the same destination: expect OCCUPIED and no change.
8. Press Alt+W: expect full overwrite; Wave Overlay must not toggle.
9. Leave Phrase and press Alt+W: Wave Overlay must toggle.
10. Clear Phrase B: all B entries must disappear from the chain.
11. Reboot after autosave: remaining chain must restore exactly.
```
