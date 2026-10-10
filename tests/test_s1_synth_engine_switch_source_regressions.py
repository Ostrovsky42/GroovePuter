"""0.9.19 S1: the AudioTask must not free or replace a synth engine."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(cond: bool, message: str) -> None:
    if not cond:
        raise AssertionError(message)


voice = (ROOT / "src/dsp/swappable_synth_voice.cpp").read_text(encoding="utf-8")
process = voice[voice.index("float SwappableSynthVoice::process()"):]
process = process[:process.index("\n}\n")]
require("current_ = std::move(next_)" not in process and "next_.reset()" not in process
        and "type_ =" not in process,
        "process() runs in the AudioTask and must not free or replace an engine")
require("fadeDone_.store(true" in process,
        "process() only marks the crossfade settled")

display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
require("withAudioGuard([&]() { mini_acid_.commitSettledSynthEngineSwitches(); })"
        in display,
        "the UI commits a settled engine switch under the audio guard")
tb303 = (ROOT / "src/dsp/mini_tb303.cpp").read_text(encoding="utf-8")
svf = tb303[tb303.index("float TB303Voice::svfProcess("):]
svf = svf[:svf.index("\n}\n")]
require("updateFilterModel()" not in svf.replace("// The filter model", ""),
        "svfProcess runs per sample in the AudioTask and must not rebuild the filter")
model = tb303[tb303.index("void TB303Voice::updateFilterModel()"):]
model = model[:model.index("\n}\n")]
require("make_unique" not in model and "new (std::nothrow)" in model,
        "the filter model is allocated nothrow (a throwing new aborts on the device)")
print("S1 synth engine switch source regressions: PASS")
