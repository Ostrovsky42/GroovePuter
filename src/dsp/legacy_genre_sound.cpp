#include "mode_manager.h"
#include "miniacid_engine.h"

bool GrooveboxModeManager::applyLegacyGenreSoundPreset(
        GenerativeMode genre, int presetIndex, int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex > 1) return false;
    if (engine_.currentSynthEngineName(voiceIndex) != "TB303") return false;

    const TB303ModePreset* presets = nullptr;
    int count = 0;

    switch (genre) {
        case GenerativeMode::Acid:
            presets = kAcidPresets;
            count = static_cast<int>(sizeof(kAcidPresets) / sizeof(kAcidPresets[0]));
            break;
        case GenerativeMode::Techno:
            // Techno's old Detroit/Electro lineage owns this TB303 family.
            // Do not change currentMode_: it is also an input to generation RNG.
            presets = kElectroPresets;
            count = static_cast<int>(sizeof(kElectroPresets) / sizeof(kElectroPresets[0]));
            break;
        default:
            return false;
    }

    if (presetIndex < 0 || presetIndex >= count) return false;

    const TB303ModePreset& preset = presets[presetIndex];
    engine_.set303Parameter(TB303ParamId::Cutoff, preset.cutoff, voiceIndex);
    engine_.set303Parameter(TB303ParamId::Resonance, preset.resonance, voiceIndex);
    engine_.set303Parameter(TB303ParamId::EnvAmount, preset.envAmount, voiceIndex);

    constexpr float kDecayMinMs = 20.0f;
    constexpr float kDecayMaxMs = 2200.0f;
    const float decayMs = kDecayMinMs + preset.decay * (kDecayMaxMs - kDecayMinMs);
    engine_.set303Parameter(TB303ParamId::EnvDecay, decayMs, voiceIndex);
    engine_.set303DistortionEnabled(voiceIndex, preset.distortion);
    engine_.set303DelayEnabled(voiceIndex, preset.delay);
    return true;
}
