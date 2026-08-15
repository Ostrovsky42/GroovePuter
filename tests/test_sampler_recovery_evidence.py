#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

sample_store = (ROOT / "src/sampler/ram_sample_store.h").read_text()
sample_store_cpp = (ROOT / "src/sampler/ram_sample_store.cpp").read_text()
sample_index = (ROOT / "src/sampler/sample_index.cpp").read_text()
sampler_voice = (ROOT / "src/sampler/sampler_voice.cpp").read_text()
sampler_pool = (ROOT / "src/sampler/sampler_pool.h").read_text()
drum_track = (ROOT / "src/sampler/drum_sampler_track.h").read_text()
drum_track_cpp = (ROOT / "src/sampler/drum_sampler_track.cpp").read_text()
engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text()
engine_h = (ROOT / "src/dsp/miniacid_engine.h").read_text()
boot = (ROOT / "GroovePuter.ino").read_text()
scene = (ROOT / "scenes.h").read_text()
sampler_page = (ROOT / "src/ui/pages/sampler_page.cpp").read_text()
workflow = (ROOT / "src/ui/workflow_mode.h").read_text()

# Runtime architecture that 0.9.3 recovers rather than rewrites.
assert "static constexpr int kMaxSampleSlots = 64;" in sample_store
assert "static constexpr int kMaxVoices = 8;" in sampler_pool
assert "static constexpr int kNumPads = 16;" in drum_track
assert "std::array<SamplerPad, kNumPads> pads_;" in drum_track
assert "SamplerPool pool_;" in drum_track
assert "SampleHandle handle_;" in (ROOT / "src/sampler/sampler_voice.h").read_text()
assert "store.acquireHandle(params.id)" in sampler_voice
assert "store.releaseHandle(handle_)" in sampler_voice
assert "const float sample = s0 + frac * (s1 - s0);" in sampler_voice
assert "playbackRate_ * srScale" in sampler_voice
assert "p.volume * velocity" in drum_track_cpp
assert "p.reverse || forceReverse" in drum_track_cpp
assert "pool_.stopByTag(i);" in drum_track_cpp

# Engine integration: sampler is constructed, rendered once per block, mixed into
# the master path, and drum sequencer lanes 0..7 trigger sampler pads 0..7.
assert "samplerOutBuffer(std::make_unique<float[]>(AUDIO_BUFFER_SAMPLES))" in engine
assert "samplerTrack(std::make_unique<DrumSamplerTrack>())" in engine
assert "samplerTrack->process(samplerOutBuffer.get(), numSamples, *sampleStore);" in engine
assert "samplerSample = samplerOutBuffer[i];" in engine
assert "sample += samplerSample;" in engine
for pad in range(8):
    assert f"samplerTrack->triggerPad({pad}," in engine
assert "step.fx == (uint8_t)StepFx::Reverse" in engine
assert "sampleStore->setPoolSize(32 * 1024)" in engine
assert "ISampleStore* sampleStore = nullptr;" in engine_h
assert "std::unique_ptr<float[]> samplerOutBuffer;" in engine_h
assert "std::unique_ptr<DrumSamplerTrack> samplerTrack;" in engine_h

# Persistence schema exists and the current writer consumes every SamplerPadState
# field. Assert the semantic write operations instead of C++ escaped JSON spelling.
assert "SamplerPadState samplerPads[16];" in scene
for expression in (
    "scene_->samplerPads[i]",
    "writeInt(p.sampleId)",
    "writeFloat(p.volume)",
    "writeFloat(p.pitch)",
    "writeInt(p.startFrame)",
    "writeInt(p.endFrame)",
    "writeInt(p.chokeGroup)",
    "writeBool(p.reverse)",
    "writeBool(p.loop)",
):
    assert expression in scene

# Evidence of the current boot-order defect: scene init/restore happens before
# sample scan and registry population. This assertion is intentionally expected
# to change in PR 0.9.3-C when the lifecycle is fixed.
init_pos = boot.index("g_miniAcidInstance.init();")
scan_pos = boot.index('sampleIndex.scanDirectory("/sd/samples")')
register_pos = boot.index("g_sampleStore.registerFile(file.id, file.fullPath);")
assert init_pos < scan_pos < register_pos
assert "if (p.id.value != 0 && sampleStore) sampleStore->preload(p.id);" in engine
assert "not found in registry" in sample_store_cpp

# Evidence of unstable legacy identity: current IDs are filename-only FNV-1a.
# PR 0.9.3-B must replace/migrate this contract rather than weakening this test.
assert "info.id.value = calculateHash(info.filename.c_str());" in sample_index
assert "calculateHash(info.fullPath.c_str())" not in sample_index

# Evidence of UI amputation and unsafe historical preload boundary. SamplerPage
# exists, but current workflow has no sampler workspace/page. PR 0.9.3-F will
# deliberately evolve these assertions after the lifecycle PRs are accepted.
assert "SamplerPage::SamplerPage" in sampler_page
assert "Sampler" not in workflow
assert "sampleStore->preload(p.id);" in sampler_page
assert "audio_guard_([&]()" in sampler_page
assert 'const char* triggerKeys = "qwertyu";' in sampler_page
assert "Q-I triggered pads 1-8" in sampler_page

print("sampler recovery evidence baseline passed")
