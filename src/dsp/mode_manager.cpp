#include "mode_manager.h"
#include "miniacid_engine.h"
#include "advanced_pattern_generator.h"
#include "groove_profile.h"
#include "../debug_log.h"
#include <algorithm>

void GrooveboxModeManager::setMode(GrooveboxMode mode) {
    currentMode_ = mode;
    engine_.setGrooveboxMode(mode); // Sync to engine
}

void GrooveboxModeManager::toggle() {
    int idx = static_cast<int>(currentMode_);
    idx = (idx + 1) % 5;
    setMode(static_cast<GrooveboxMode>(idx));
}

void GrooveboxModeManager::setFlavor(int flavor) {
    if (flavor < 0) flavor = 0;
    if (flavor >= flavorCount()) flavor = flavorCount() - 1;
    currentFlavor_ = flavor;
}

void GrooveboxModeManager::shiftFlavor(int delta) {
    int v = currentFlavor_ + delta;
    while (v < 0) v += flavorCount();
    while (v >= flavorCount()) v -= flavorCount();
    currentFlavor_ = v;
}

void GrooveboxModeManager::apply303Preset(int voiceIndex, int presetIndex) {
    int count;
    const TB303ModePreset* presets = get303Presets(count);
    if (presetIndex < 0 || presetIndex >= count) return;
    
    const TB303ModePreset& p = presets[presetIndex];
    engine_.set303Parameter(TB303ParamId::Cutoff, p.cutoff, voiceIndex);
    engine_.set303Parameter(TB303ParamId::Resonance, p.resonance, voiceIndex);
    engine_.set303Parameter(TB303ParamId::EnvAmount, p.envAmount, voiceIndex);
    // envDecay needs mapping from 0..1 to 20..2200
    float decayMs = 20.0f + p.decay * (2200.0f - 20.0f);
    engine_.set303Parameter(TB303ParamId::EnvDecay, decayMs, voiceIndex);
    
    if (voiceIndex == 0) {
        engine_.set303DistortionEnabled(0, p.distortion);
        engine_.set303DelayEnabled(0, p.delay);
    } else {
        engine_.set303DistortionEnabled(1, p.distortion);
        engine_.set303DelayEnabled(1, p.delay);
    }
}

bool GrooveboxModeManager::applyLegacyGenreSoundPreset(
        GenerativeMode genre, int presetIndex, int voiceIndex) {
    const TB303ModePreset* presets = nullptr;
    int count = 0;

    switch (genre) {
        case GenerativeMode::Acid:
            presets = kAcidPresets;
            count = static_cast<int>(sizeof(kAcidPresets) / sizeof(kAcidPresets[0]));
            break;
        case GenerativeMode::Techno:
            // Techno's legacy sound lineage is the old Electro/Detroit TB303
            // preset family. Selecting it here must not change currentMode_,
            // because currentMode_ is also an input to deterministic generation.
            presets = kElectroPresets;
            count = static_cast<int>(sizeof(kElectroPresets) / sizeof(kElectroPresets[0]));
            break;
        default:
            return false;
    }

    if (presetIndex < 0 || presetIndex >= count) return false;

    const TB303ModePreset& p = presets[presetIndex];
    engine_.set303Parameter(TB303ParamId::Cutoff, p.cutoff, voiceIndex);
    engine_.set303Parameter(TB303ParamId::Resonance, p.resonance, voiceIndex);
    engine_.set303Parameter(TB303ParamId::EnvAmount, p.envAmount, voiceIndex);
    const float decayMs = 20.0f + p.decay * (2200.0f - 20.0f);
    engine_.set303Parameter(TB303ParamId::EnvDecay, decayMs, voiceIndex);
    engine_.set303DistortionEnabled(voiceIndex, p.distortion);
    engine_.set303DelayEnabled(voiceIndex, p.delay);
    return true;
}

struct Scale {
    const char* name;
    int intervals[7];
    int count;
};

const Scale kScales[] = {
    {"Minor Pentatonic", {0, 3, 5, 7, 10}, 5},
    {"Phrygian", {0, 1, 3, 5, 7, 8, 10}, 7},
    {"Aeolian", {0, 2, 3, 5, 7, 8, 10}, 7},
    {"Dorian", {0, 2, 3, 5, 7, 9, 10}, 7}
};

int quantizeToScale(int note, const Scale& scale) {
    // Protect against negative notes
    if (note < 0) return note;
    
    int octave = note / 12;
    // Normalize pitch for negative modulo behavior in C++
    int pitch = ((note % 12) + 12) % 12;
    
    int bestPitch = scale.intervals[0];
    int minDiff = 12;
    
    for (int i = 0; i < scale.count; i++) {
        int diff = abs(pitch - scale.intervals[i]);
        if (diff < minDiff) {
            minDiff = diff;
            bestPitch = scale.intervals[i];
        }
    }
    return octave * 12 + bestPitch;
}


namespace {

uint32_t mixGenerationSeed(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

uint32_t hashGenerationByte(uint32_t hash, uint8_t value) {
    return (hash ^ value) * 16777619u;
}

uint32_t hashSynthPattern(const SynthPattern& pattern) {
    uint32_t hash = 2166136261u;
    for (const SynthStep& step : pattern.steps) {
        hash = hashGenerationByte(hash, static_cast<uint8_t>(step.note));
        hash = hashGenerationByte(hash, step.slide ? 1u : 0u);
        hash = hashGenerationByte(hash, step.accent ? 1u : 0u);
        hash = hashGenerationByte(hash, step.ghost ? 1u : 0u);
        hash = hashGenerationByte(hash, step.velocity);
        hash = hashGenerationByte(hash, static_cast<uint8_t>(step.timing));
        hash = hashGenerationByte(hash, step.fx);
        hash = hashGenerationByte(hash, step.fxParam);
        hash = hashGenerationByte(hash, step.probability);
    }
    return hash;
}

uint32_t hashDrumPattern(const DrumPattern& pattern) {
    uint32_t hash = 2166136261u;
    for (const DrumStep& step : pattern.steps) {
        hash = hashGenerationByte(hash, step.hit ? 1u : 0u);
        hash = hashGenerationByte(hash, step.accent ? 1u : 0u);
        hash = hashGenerationByte(hash, step.velocity);
        hash = hashGenerationByte(hash, static_cast<uint8_t>(step.timing));
        hash = hashGenerationByte(hash, step.fx);
        hash = hashGenerationByte(hash, step.fxParam);
        hash = hashGenerationByte(hash, step.probability);
    }
    return hash;
}

uint32_t hashDrumPatternSet(const DrumPatternSet& patternSet) {
    uint32_t hash = 2166136261u;
    for (const DrumPattern& voice : patternSet.voices) {
        hash ^= mixGenerationSeed(hashDrumPattern(voice));
        hash *= 16777619u;
    }
    return hash;
}

int boundedRandom(DeterministicRng& rng, uint32_t upperExclusive) {
    return static_cast<int>(rng.bounded(upperExclusive));
}

}  // namespace

uint32_t GrooveboxModeManager::ensureGenerationSeed() const {
    if (generationSeed_ == 0) {
        // Capture boot-seeded libc entropy once. Pattern generation never
        // consumes the global stream again after this boundary.
        uint32_t seed = static_cast<uint32_t>(::rand()) ^ 0xA511E9B3u;
        generationSeed_ = seed == 0 ? kFallbackGenerationSeed : seed;
    }
    return generationSeed_;
}

DeterministicRng GrooveboxModeManager::makeGenerationRng(
        GenerationDomain domain, uint32_t contentHash) const {
    uint32_t seed = ensureGenerationSeed();
    seed ^= static_cast<uint32_t>(domain);
    seed ^= mixGenerationSeed(contentHash);
    seed ^= static_cast<uint32_t>(currentMode_) * 0x9E3779B9u;
    seed ^= static_cast<uint32_t>(currentFlavor_ + 1) * 0x85EBCA6Bu;
    return DeterministicRng(mixGenerationSeed(seed));
}


void GrooveboxModeManager::generatePattern(SynthPattern& pattern, float bpm) const {
    DeterministicRng rng = makeGenerationRng(
        GenerationDomain::SynthA, hashSynthPattern(pattern));
    const ModeConfig& cfg = config();
    
    // 1. Compute interpolation values (BPM & Flavor)
    float t = (bpm - 80.0f) / (170.0f - 80.0f);
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto prob100 = [](float p){ return int(p * 100.0f + 0.5f); };
