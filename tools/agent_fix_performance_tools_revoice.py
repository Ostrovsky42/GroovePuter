#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if old not in text:
        raise RuntimeError(f"expected block not found in {path}")
    if text.count(old) != 1:
        raise RuntimeError(f"expected exactly one block in {path}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def main() -> None:
    header = ROOT / "src/input/performance_keyboard.h"
    source = ROOT / "src/input/performance_keyboard.cpp"
    tests = ROOT / "tests/test_performance_patterns.cpp"
    docs = ROOT / "docs/stages/WAVEMORPH_PERFORMANCE_LOGIC_STAGE.md"

    replace_once(
        header,
        "    void triggerDirectTransformed(uint32_t nowMicros);\n"
        "    void restartAfterConfigurationChange();\n",
        "    void triggerDirectTransformed(uint32_t nowMicros);\n"
        "    void stopOutputForConfigurationChange();\n"
        "    void restartHeldOutputAfterConfigurationChange();\n",
    )

    replace_once(
        source,
        "void PerformanceKeyboard::restartAfterConfigurationChange() {\n"
        "    panic();\n"
        "}\n",
        "void PerformanceKeyboard::stopOutputForConfigurationChange() {\n"
        "    serviceHardwareClock();\n"
        "    if (target_ == MusicalEventTarget::Drums) return;\n"
        "\n"
        "    // Release both ownership domains without forgetting which physical\n"
        "    // keys are still held. Generated notes use the Arpeggiator source,\n"
        "    // while untransformed notes use PerformanceKeyboard.\n"
        "    stopGeneratedOutput();\n"
        "    emitAllNotesOff();\n"
        "    resetStepClock();\n"
        "}\n"
        "\n"
        "void PerformanceKeyboard::restartHeldOutputAfterConfigurationChange() {\n"
        "    if (target_ == MusicalEventTarget::Drums ||\n"
        "        !liveInputAllowed() || heldCount_ == 0) {\n"
        "        return;\n"
        "    }\n"
        "\n"
        "    if (stepEngineEnabled()) {\n"
        "        resetStepClock();\n"
        "        service(lastServiceMicros_);\n"
        "    } else if (transformedPlaybackEnabled()) {\n"
        "        triggerDirectTransformed(lastServiceMicros_);\n"
        "    } else {\n"
        "        emitNoteOn(held_[heldCount_ - 1]);\n"
        "    }\n"
        "}\n",
    )

    replace_once(
        source,
        "void PerformanceKeyboard::setChordMode(PerformanceChordMode mode) {\n"
        "    if (mode >= PerformanceChordMode::Count || chordMode_ == mode) return;\n"
        "    restartAfterConfigurationChange();\n"
        "    chordMode_ = mode;\n"
        "}\n",
        "void PerformanceKeyboard::setChordMode(PerformanceChordMode mode) {\n"
        "    if (mode >= PerformanceChordMode::Count || chordMode_ == mode) return;\n"
        "    stopOutputForConfigurationChange();\n"
        "    chordMode_ = mode;\n"
        "    restartHeldOutputAfterConfigurationChange();\n"
        "}\n",
    )

    replace_once(
        source,
        "    panic();\n"
        "    chordMode_ = PerformanceChordMode::Memory;\n"
        "    return chordMemoryCount_ > 0;\n"
        "}\n\n"
        "void PerformanceKeyboard::clearChordMemory() {\n"
        "    panic();\n"
        "    for (uint8_t& interval : chordMemoryIntervals_) interval = 0;\n"
        "    chordMemoryCount_ = 0;\n"
        "    if (chordMode_ == PerformanceChordMode::Memory) {\n"
        "        chordMode_ = PerformanceChordMode::Off;\n"
        "    }\n"
        "}\n",
        "    if (chordMemoryCount_ == 0) return false;\n"
        "    stopOutputForConfigurationChange();\n"
        "    chordMode_ = PerformanceChordMode::Memory;\n"
        "    restartHeldOutputAfterConfigurationChange();\n"
        "    return true;\n"
        "}\n\n"
        "void PerformanceKeyboard::clearChordMemory() {\n"
        "    const bool affectsOutput =\n"
        "        chordMode_ == PerformanceChordMode::Memory;\n"
        "    if (affectsOutput) stopOutputForConfigurationChange();\n"
        "    for (uint8_t& interval : chordMemoryIntervals_) interval = 0;\n"
        "    chordMemoryCount_ = 0;\n"
        "    if (affectsOutput) {\n"
        "        chordMode_ = PerformanceChordMode::Off;\n"
        "        restartHeldOutputAfterConfigurationChange();\n"
        "    }\n"
        "}\n",
    )

    replace_once(
        source,
        "void PerformanceKeyboard::setArpeggiatorEnabled(bool enabled) {\n"
        "    if (arpeggiatorEnabled_ == enabled) return;\n"
        "    restartAfterConfigurationChange();\n"
        "    arpeggiatorEnabled_ = enabled;\n"
        "}\n",
        "void PerformanceKeyboard::setArpeggiatorEnabled(bool enabled) {\n"
        "    if (arpeggiatorEnabled_ == enabled) return;\n"
        "    stopOutputForConfigurationChange();\n"
        "    arpeggiatorEnabled_ = enabled;\n"
        "    restartHeldOutputAfterConfigurationChange();\n"
        "}\n",
    )

    replace_once(
        source,
        "    restartAfterConfigurationChange();\n"
        "    arpDirection_ = static_cast<PerformanceArpDirection>(next);\n"
        "}\n\n"
        "const char* PerformanceKeyboard::arpDirectionName() const {\n",
        "    const bool affectsOutput = arpeggiatorEnabled_;\n"
        "    if (affectsOutput) stopOutputForConfigurationChange();\n"
        "    arpDirection_ = static_cast<PerformanceArpDirection>(next);\n"
        "    if (affectsOutput) restartHeldOutputAfterConfigurationChange();\n"
        "}\n\n"
        "const char* PerformanceKeyboard::arpDirectionName() const {\n",
    )

    replace_once(
        source,
        "    restartAfterConfigurationChange();\n"
        "    strumMs_ = kStrumOptionsMs[next];\n"
        "}\n\n"
        "void PerformanceKeyboard::cycleRatchet(int direction) {\n",
        "    stopOutputForConfigurationChange();\n"
        "    strumMs_ = kStrumOptionsMs[next];\n"
        "    restartHeldOutputAfterConfigurationChange();\n"
        "}\n\n"
        "void PerformanceKeyboard::cycleRatchet(int direction) {\n",
    )

    replace_once(
        source,
        "    restartAfterConfigurationChange();\n"
        "    ratchetCount_ = static_cast<uint8_t>(next);\n"
        "}\n\n"
        "void PerformanceKeyboard::cycleEuclideanPulses(int direction) {\n",
        "    stopOutputForConfigurationChange();\n"
        "    ratchetCount_ = static_cast<uint8_t>(next);\n"
        "    restartHeldOutputAfterConfigurationChange();\n"
        "}\n\n"
        "void PerformanceKeyboard::cycleEuclideanPulses(int direction) {\n",
    )

    replace_once(
        source,
        "    restartAfterConfigurationChange();\n"
        "    euclideanPulses_ = kEuclideanPulseOptions[next];\n"
        "}\n\n"
        "void PerformanceKeyboard::rotateEuclidean(int direction) {\n",
        "    stopOutputForConfigurationChange();\n"
        "    euclideanPulses_ = kEuclideanPulseOptions[next];\n"
        "    restartHeldOutputAfterConfigurationChange();\n"
        "}\n\n"
        "void PerformanceKeyboard::rotateEuclidean(int direction) {\n",
    )

    replace_once(
        source,
        "    restartAfterConfigurationChange();\n"
        "    euclideanRotation_ = static_cast<uint8_t>(next);\n"
        "}\n",
        "    const bool affectsOutput = euclideanPulses_ > 0;\n"
        "    if (affectsOutput) stopOutputForConfigurationChange();\n"
        "    euclideanRotation_ = static_cast<uint8_t>(next);\n"
        "    if (affectsOutput) restartHeldOutputAfterConfigurationChange();\n"
        "}\n",
    )

    test_text = tests.read_text(encoding="utf-8")
    marker = "\n    return 0;\n}\n"
    if marker not in test_text:
        raise RuntimeError("test insertion marker missing")
    regression = r'''

    {
        // Changing a tool while a physical note remains held must revoice the
        // same held key under the new logic instead of calling panic() and
        // forgetting the hardware state.
        Fixture f;
        assert(f.keyboard.keyDown('a', 101));
        assert(f.keyboard.heldCount() == 1);
        f.sink.clear();

        f.keyboard.setChordMode(PerformanceChordMode::Major);

        assert(f.keyboard.heldCount() == 1);
        assert(f.keyboard.isPhysicalKeyHeld('a'));
        assert(count(f.sink.events, MusicalEventType::AllNotesOff) == 1);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 3);
        assert(f.sink.events[1].note == 36);
        assert(f.sink.events[2].note == 40);
        assert(f.sink.events[3].note == 43);
    }

    {
        Fixture f;
        f.keyboard.service(4000000u);
        assert(f.keyboard.keyDown('a', 96));
        f.sink.clear();

        f.keyboard.setArpeggiatorEnabled(true);

        assert(f.keyboard.heldCount() == 1);
        assert(f.keyboard.isPhysicalKeyHeld('a'));
        assert(count(f.sink.events, MusicalEventType::AllNotesOff) == 1);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 1);
        assert(f.sink.events.back().source == MusicalEventSource::Arpeggiator);

        f.sink.clear();
        f.keyboard.cycleArpDirection(1);
        assert(f.keyboard.heldCount() == 1);
        assert(count(f.sink.events, MusicalEventType::NoteOff) >= 1);
        assert(count(f.sink.events, MusicalEventType::NoteOn) == 1);
    }
'''
    tests.write_text(test_text.replace(marker, regression + marker, 1), encoding="utf-8")

    doc_text = docs.read_text(encoding="utf-8")
    doc_section = r'''

## Live tool-change acceptance

While one or more musical keys are held on `PERFORM`, changing `ARP`, `DIR`,
`CHORD`, `STRUM`, `RATCHET`, `EUCLID`, or `ROTATE` must release the old logical
output and immediately revoice the still-held physical keys with the new logic.
The change must not call the user-facing panic path, clear the held-key table, or
require releasing and pressing the musical keys again.

- [ ] Hold `A`, open `Tab`, press `3`: the note becomes the selected chord mode without silence.
- [ ] Hold `A`, press `1`: arpeggiation starts immediately.
- [ ] While arpeggiating, press `2`, `6`, `7`, or `8`: timing changes and playback continues.
- [ ] Release `A`: all generated notes stop and no stuck note remains.
- [ ] On the Drums target, tool changes do not interrupt held drum pads.
'''
    if "## Live tool-change acceptance" not in doc_text:
        docs.write_text(doc_text.rstrip() + doc_section + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
