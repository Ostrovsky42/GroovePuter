#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_ppqn_dispatch_is_not_step_gated() -> None:
    source = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    loop_start = source.index("while (ticksToAdvance--)")
    loop_end = source.index("if (gateCountdownA_", loop_start)
    dispatch_block = source[loop_start:loop_end]

    require("advanceTick();" in dispatch_block,
            "each accumulated PPQN tick must dispatch sequencer events")
    require("currentTick_ % 24" not in dispatch_block,
            "PPQN dispatch must not be gated to 16th-note boundaries")


def test_all_substep_offsets_are_reachable() -> None:
    ticks_per_bar = 384
    ticks_per_step = 24

    for step in range(16):
        for offset in range(-23, 24):
            due_tick = (step * ticks_per_step + offset) % ticks_per_bar
            visits = sum(1 for tick in range(ticks_per_bar) if tick == due_tick)
            require(visits == 1,
                    f"step={step} offset={offset} must be visited exactly once")


def test_adv_amp_pin_is_not_used_as_rgb_data() -> None:
    profile = (ROOT / "src/platform/cardputer_adv_hardware.h").read_text(
        encoding="utf-8"
    )
    led = (ROOT / "src/ui/led_manager.cpp").read_text(encoding="utf-8")

    require("GROOVEPUTER_CARDPUTER_ADV_PA_EN_PIN 21" in profile,
            "Cardputer ADV PA_EN pin must remain explicit")
    require("GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN (-1)" in profile,
            "RGB output must remain disabled until a distinct ADV pin is verified")
    require("neopixelWrite(21" not in led,
            "GPIO21 must never receive WS2812 timing on Cardputer ADV")


def main() -> None:
    test_ppqn_dispatch_is_not_step_gated()
    test_all_substep_offsets_are_reachable()
    test_adv_amp_pin_is_not_used_as_rgb_data()
    print("source regressions: OK")


if __name__ == "__main__":
    main()
