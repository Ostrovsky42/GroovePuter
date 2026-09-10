#!/usr/bin/env python3
"""Workflow switching must be reachable from the event, on every platform.

WorkflowPages::hardwareWorkflowModifierHeld() (src/ui/workflow_mode.h:289)
returns M5Cardputer.Keyboard.keysState().fn on device and a hard-coded false
everywhere else. nextPage()/previousPage() consulted only that, so in the SDL
build no workflow other than the one carried by the session file could ever be
reached -- not a screenshot problem but a hole in the emulator's fidelity.

On the Cardputer Fn already arrives in the UIEvent as `meta`: the Fn+M launcher
(miniacid_display.cpp) is built on exactly that. So the fix is to read the
modifier the event already carries, and to keep the hardware query alongside it
so device behaviour is strictly unchanged rather than replaced.

This file pins that contract. It is a source contract, not a behavioural test:
the navigation it describes needs a live shell, and the thing worth protecting
is that nobody quietly drops either half of the condition.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DISPLAY_H = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")
DISPLAY_CPP = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
WORKFLOW_H = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
SDL_MAIN = (ROOT / "platform_sdl/sdl_main.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


def main() -> None:
    # 1. The navigation entry points accept the modifier as an argument.
    require("void nextPage(bool workflowModifier = false);" in DISPLAY_H,
            "nextPage must accept the workflow modifier from its caller")
    require("void previousPage(bool workflowModifier = false);" in DISPLAY_H,
            "previousPage must accept the workflow modifier from its caller")

    # 2. Both halves of the condition survive. Dropping the hardware query would
    #    change the device; dropping the argument would re-strand the emulator.
    for name, direction in (("nextPage", "1"), ("previousPage", "-1")):
        body = between(DISPLAY_CPP,
                       f"void MiniAcidDisplay::{name}(bool workflowModifier)",
                       "}\n\nvoid MiniAcidDisplay::")
        require("WorkflowPages::hardwareWorkflowModifierHeld()" in body,
                f"{name} stopped consulting the hardware Fn state")
        require("workflowModifier ||" in body,
                f"{name} ignores the modifier carried by the event")
        require(f"switchWorkflow_({direction})" in body,
                f"{name} no longer switches workflow in its direction")

    # 3. The shell passes what the event carries. Fn arrives as meta on device,
    #    and as KMOD_GUI in the SDL build, so one expression serves both.
    require("nextPage(ui_event.meta)" in DISPLAY_CPP or
            "nextPage(event.meta)" in DISPLAY_CPP,
            "the ']' handler must forward the event's modifier")
    require("previousPage(ui_event.meta)" in DISPLAY_CPP or
            "previousPage(event.meta)" in DISPLAY_CPP,
            "the '[' handler must forward the event's modifier")

    # 4. The SDL fallback path is reached when the UI declines the key; it must
    #    not silently drop the modifier and switch pages instead of workflows.
    require("nextPage((SDL_GetModState() & KMOD_GUI) != 0)" in SDL_MAIN,
            "SDL fallback for ']' drops the workflow modifier")
    require("previousPage((SDL_GetModState() & KMOD_GUI) != 0)" in SDL_MAIN,
            "SDL fallback for '[' drops the workflow modifier")

    # 5. The hardware query itself is untouched: this change adds a path, it
    #    does not reroute the device.
    require("M5Cardputer.Keyboard.keysState().fn" in WORKFLOW_H,
            "the device Fn query must remain the device's source of truth")

    print("PASS: workflow switching is reachable from the event on both platforms")


if __name__ == "__main__":
    main()
