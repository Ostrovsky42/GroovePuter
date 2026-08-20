#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    header = (ROOT / "src/eye_pair_sync/eye_output_mode.h").read_text(
        encoding="utf-8"
    )
    implementation = (ROOT / "src/eye_pair_sync/eye_output_mode.cpp").read_text(
        encoding="utf-8"
    )
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")

    require(
        "#define GROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW 0" in header,
        "production Cardputer build must keep ESP-NOW disabled by default",
    )
    require(
        "#define GROOVEPUTER_GVEP_R01_MEMORY_PROBE 0" in header,
        "R0.1 instrumentation must be opt-in",
    )

    init_start = implementation.index("void eye_output_mode_init(void)")
    init_end = implementation.index(
        "bool eye_output_mode_transport_enabled(void)", init_start
    )
    init_block = implementation[init_start:init_end]
    require(
        "WiFi.mode" not in init_block and "esp_now_init" not in init_block,
        "eye_output_mode_init must remain transport-neutral during setup",
    )

    flush_start = implementation.index("void eye_output_mode_flush(void)")
    flush_end = implementation.index("bool eye_gvep_build_packet", flush_start)
    flush_block = implementation[flush_start:flush_end]
    require(
        "ensureEspNowStarted();" in flush_block,
        "ESP-NOW must start from the post-setup control loop",
    )

    ensure_start = implementation.index("void ensureEspNowStarted()")
    ensure_end = implementation.index("bool sendRaw", ensure_start)
    ensure_block = implementation[ensure_start:ensure_end]
    for token in (
        "free_internal_before_radio",
        "largest_internal_before_radio",
        "esp_now_init()",
        "free_internal_after_radio",
        "largest_internal_after_radio",
        "radio_init_failures",
    ):
        require(token in ensure_block, f"R0.1 radio init must record {token}")

    require(
        "uxTaskGetStackHighWaterMark(::g_audioTaskHandle)" in implementation,
        "R0.1 must measure the existing AudioTask stack high-water mark",
    )
    require(
        "queue_dropped.fetch_add" in implementation,
        "visual queue overflow must remain measurable and fail-soft",
    )

    setup_start = sketch.index("void setup()")
    loop_start = sketch.index("void loop()", setup_start)
    setup_block = sketch[setup_start:loop_start]
    loop_block = sketch[loop_start:]
    require(
        "startAudioTask();" in setup_block,
        "AudioTask must be reserved during setup before the radio experiment",
    )
    require(
        "eye_output_mode_flush();" not in setup_block,
        "radio-starting flush must not run from setup",
    )
    require(
        "eye_output_mode_flush();" in loop_block,
        "radio-starting flush must run only after setup completes",
    )

    require(
        'EXTRA_CPP_FLAGS="${EXTRA_CPP_FLAGS:-}"' in build
        and "${EXTRA_CPP_FLAGS}" in build,
        "build.sh must preserve required flags while allowing R0.1 opt-in defines",
    )

    print("GVEP R0.1 source contract: PASS")


if __name__ == "__main__":
    main()
