# 0.9.10 R8 — MIDI input UI

R8 exposes the persisted R7 controller configuration on the existing `PROJECT -> MIDI` surface. It does not add a top-level page and keeps outbound Device Profile visibly separate from incoming controller routing.

Rows:
- `Device` — existing outbound profile preview/save behavior;
- `MIDI Input` — ON/OFF;
- `Input Ch` — OMNI or 1..16;
- `Input To` — SYN A / SYN B / DRUMS.

Left/right edits input settings live through the authoritative `CardputerMidiSettingsSession` R7 API. The settings owner persists first and then applies the router's cleanup-first configuration. Enter advances/toggles the focused input value; Device Profile retains its existing Enter-save/reboot semantics.

The fixed GM drum map remains implicit and is not duplicated as a UI selector. No OutputOwnership, DeviceProfile, Scene, or DSP state is mutated by MIDI Input UI actions.
