#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    pool_h = read("src/sampler/sampler_pool.h")
    track_h = read("src/sampler/drum_sampler_track.h")
    voice_cpp = read("src/sampler/sampler_voice.cpp")
    engine_cpp = read("src/dsp/miniacid_engine.cpp")
    page_h = read("src/ui/pages/sampler_page.h")
    page_cpp = read("src/ui/pages/sampler_page.cpp")
    ref_h = read("src/sampler/sample_ref.h")
    index_cpp = read("src/sampler/sample_index.cpp")
    persistence_h = read("src/sampler/sample_scene_persistence.h")
    boot = read("sampler_boot_registry.ino")

    # Existing DSP architecture recovered, not redesigned.
    require("static constexpr int kMaxVoices = 8;" in pool_h,
            "sampler polyphony contract must remain eight voices")
    require("static constexpr int kNumPads = 16;" in track_h,
            "internal sampler track must retain sixteen pad slots")
    require("static constexpr int kRecoveredPadCount = 8;" in page_h,
            "0.9.3 UI must expose exactly the eight sequenced sampler pads")
    require('constexpr char kSequencedPadKeys[] = "qwertyui";' in page_cpp,
            "direct pad keys must map Q W E R T Y U I to pads 1..8")

    # Keep existing musical/DSP behavior present.
    for token in ("reverse", "loop", "pitch", "startFrame", "endFrame"):
        require(token in voice_cpp or token in track_h,
                f"sampler DSP contract missing {token}")
    require("chokeGroup" in track_h,
            "sampler choke-group behavior must remain available")
    require("samplerTrack->triggerPad" in engine_cpp,
            "drum sequencer must still trigger sampler pads")
    require(engine_cpp.count("samplerTrack->triggerPad") >= 8,
            "all eight recovered drum lanes must retain sampler triggers")
    audio_start = engine_cpp.index("void MiniAcid::generateAudioBuffer")
    audio_end = engine_cpp.index("void MiniAcid::randomize303Pattern", audio_start)
    audio_body = engine_cpp[audio_start:audio_end]
    sampler_render = audio_body.find(
        "samplerTrack->process(&samplerSample, 1, *sampleStore)")
    require(sampler_render >= 0,
            "sampler must render inside the audio frame that owns its triggers")
    require(sampler_render > audio_body.rfind("samplerTrack->triggerPad"),
            "sampler render must follow every in-frame sequencer/retrig trigger")
    require(sampler_render > audio_body.find("advanceTick();"),
            "sampler render must follow the current frame's tick dispatch")
    require("samplerOutBuffer" not in engine_cpp,
            "sampler must not be pre-rendered before current-block triggers")
    require("setPoolSize(32 * 1024)" in engine_cpp,
            "0.9.3 must not increase the accepted 32 KiB sampler pool")

    # Stable identity / persistence remains the D contract.
    require("uint64_t value" in ref_h,
            "SampleRef must remain 64-bit control-side identity")
    require("stableSampleRefForPath" in ref_h,
            "path-derived SampleRef must remain authoritative")
    require("runtimeIdForRef" in index_cpp and "resolveRuntimeId" in index_cpp,
            "stable identity must continue resolving through compact runtime IDs")
    require("kMaxPadObjectBytes = 384" in persistence_h and
            "kMaxOutputBytes = 448" in persistence_h,
            "bounded streaming sampler persistence scratch must remain intact")

    # Boot registry remains before Scene apply; boot hook must never preload PCM.
    require("bindToStore(g_sampleStore)" in boot,
            "Cardputer boot must bind sampler registry before Scene restore")
    require("preload(" not in boot and "loadWavFile" not in boot,
            "boot registry hook must not load PCM")

    # 0.9.4 features stay out of the recovered 0.9.3 UI.
    require("kit_ctrl_" not in page_h and "loadKit(" not in page_cpp,
            "transactional/canonical kit loading is deferred to 0.9.4")
    for forbidden in ("waveform", "slice", "timeStretch", "roundRobin", "recordSample"):
        require(forbidden not in page_cpp,
                f"0.9.4 sampler productization leaked into 0.9.3: {forbidden}")

    print("0.9.3 sampler recovery source regressions passed")


if __name__ == "__main__":
    main()
