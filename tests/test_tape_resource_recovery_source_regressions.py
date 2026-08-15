#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(text: str, needle: str, path: str) -> None:
    if needle not in text:
        raise AssertionError(f"{path}: missing required contract anchor: {needle}")


def cardputer_section(text: str, path: str) -> str:
    marker = "#if defined(ARDUINO_M5STACK_CARDPUTER)"
    start = text.find(marker)
    if start < 0:
        raise AssertionError(f"{path}: Cardputer ADV policy section is missing")
    end = text.find("#else", start)
    if end < 0:
        raise AssertionError(f"{path}: Cardputer ADV policy section has no #else boundary")
    return text[start:end]


def main() -> None:
    tape_fx_h = read("src/dsp/tape_fx.h")
    tape_looper_h = read("src/dsp/tape_looper.h")
    tape_looper_cpp = read("src/dsp/tape_looper.cpp")
    miniacid_cpp = read("src/dsp/miniacid_engine.cpp")

    fx_adv = cardputer_section(tape_fx_h, "src/dsp/tape_fx.h")
    require(fx_adv, "float process(float input) { return input; }", "src/dsp/tape_fx.h")
    require(fx_adv, "bool isEnabled() const { return false; }", "src/dsp/tape_fx.h")
    for forbidden in ("wowBuffer_", "delayBuffer_", "float buffer_[", "std::array<float"):
        if forbidden in fx_adv:
            raise AssertionError(
                "src/dsp/tape_fx.h: Cardputer ADV bypass regained resident DSP buffer state"
            )

    require(
        tape_looper_h,
        "bool storageReady() const { return buffer_ != nullptr && maxSamples_ > 0; }",
        "src/dsp/tape_looper.h",
    )
    require(
        tape_looper_h,
        "Invariant: without usable storage every requested mode resolves to STOP.",
        "src/dsp/tape_looper.h",
    )
    require(tape_looper_h, "void forceStoppedWithoutStorage_();", "src/dsp/tape_looper.h")

    looper_adv = cardputer_section(tape_looper_cpp, "src/dsp/tape_looper.cpp")
    require(looper_adv, "forceStoppedWithoutStorage_();", "src/dsp/tape_looper.cpp")
    require(looper_adv, "return false;", "src/dsp/tape_looper.cpp")
    if "LOOPER_MALLOC_" in looper_adv or "heap_caps_malloc" in looper_adv:
        raise AssertionError(
            "src/dsp/tape_looper.cpp: Cardputer ADV init must not allocate TapeLooper storage"
        )

    require(
        tape_looper_cpp,
        "if (!storageReady()) {\n        forceStoppedWithoutStorage_();\n        return;\n    }",
        "src/dsp/tape_looper.cpp",
    )
    require(
        tape_looper_cpp,
        "if (!storageReady()) {\n        forceStoppedWithoutStorage_();\n        *loopPart = 0.0f;\n        return;\n    }",
        "src/dsp/tape_looper.cpp",
    )

    # Runtime ownership contract: requested Scene mode is applied to the looper,
    # then the effective looper mode is mirrored back into Scene. This is what
    # turns a legacy REC/DUB/PLAY request into STOP when ADV storage is absent.
    require(
        miniacid_cpp,
        "tapeLooper->setMode(tapeState.mode);",
        "src/dsp/miniacid_engine.cpp",
    )
    require(
        miniacid_cpp,
        "sceneManager_.currentScene().tape.mode = tapeLooper->mode();",
        "src/dsp/miniacid_engine.cpp",
    )
    require(
        miniacid_cpp,
        "lastTapeMode_ = tapeLooper->mode();",
        "src/dsp/miniacid_engine.cpp",
    )

    print("Tape resource-recovery source contracts: PASS")


if __name__ == "__main__":
    main()
