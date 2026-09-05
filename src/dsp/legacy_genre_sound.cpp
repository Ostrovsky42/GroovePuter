#include "mode_manager.h"
#include "miniacid_engine.h"

namespace {

struct LegacyGenreTimbre {
    float osc;
    float cutoff;
    float resonance;
    float envAmount;
    float envDecay;
};

// Exact base timbre values from the last GenreSceneView::applyGenreTimbre()
// implementation before physical synth ownership was removed from GenreManager.
constexpr LegacyGenreTimbre kLegacyAcidTimbre{
    0.0f, 0.55f, 0.35f, 0.85f, 0.35f};
constexpr LegacyGenreTimbre kLegacyDetroitTechnoTimbre{
    0.2f, 0.60f, 0.30f, 0.75f, 0.20f};

float clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

}  // namespace

bool GrooveboxModeManager::applyLegacyGenreTimbre(
        GenerativeMode genre, int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex > 1) return false;
    if (engine_.currentSynthEngineName(voiceIndex) != "TB303") return false;

    const LegacyGenreTimbre* timbre = nullptr;
    switch (genre) {
        case GenerativeMode::Acid:
            timbre = &kLegacyAcidTimbre;
            break;
        case GenerativeMode::Techno:
            // Techno was appended after the old Genre timbre projector was
            // removed. Its legacy target is the old Electro/Detroit behavior.
            timbre = &kLegacyDetroitTechnoTimbre;
            break;
        default:
            return false;
    }

    engine_.set303ParameterNormalized(
        TB303ParamId::Oscillator, timbre->osc, voiceIndex);

    float cutoff = timbre->cutoff;
    float resonance = timbre->resonance;
    float envAmount = timbre->envAmount;
    float envDecay = timbre->envDecay;

    // Preserve the historical per-voice clamps exactly, but keep the operation
    // explicit and sound-only instead of restoring GenreManager ownership.
    if (voiceIndex == 0) {
        cutoff = clamp01(cutoff);
        resonance = clamp01(resonance);
        envAmount = clamp01(envAmount);
        envDecay = clamp01(envDecay);

        if (cutoff < 0.18f) cutoff = 0.18f;
        if (cutoff > 0.62f) cutoff = 0.62f;
        if (envAmount < 0.18f) envAmount = 0.18f;
        if (envAmount > 0.55f) envAmount = 0.55f;
        if (envDecay < 0.10f) envDecay = 0.10f;
        if (envDecay > 0.45f) envDecay = 0.45f;
        if (resonance < 0.0f) resonance = 0.0f;
        if (resonance > 0.85f) resonance = 0.85f;
    } else {
        if (cutoff < 0.40f) cutoff = 0.40f;
        if (envAmount < 0.20f) envAmount = 0.20f;
        if (envDecay < 0.08f) envDecay = 0.08f;
        if (cutoff > 0.95f) cutoff = 0.95f;
        if (resonance > 0.95f) resonance = 0.95f;
    }

    engine_.set303ParameterNormalized(
        TB303ParamId::Cutoff, clamp01(cutoff), voiceIndex);
    engine_.set303ParameterNormalized(
        TB303ParamId::Resonance, clamp01(resonance), voiceIndex);
    engine_.set303ParameterNormalized(
        TB303ParamId::EnvAmount, clamp01(envAmount), voiceIndex);
    engine_.set303ParameterNormalized(
        TB303ParamId::EnvDecay, clamp01(envDecay), voiceIndex);
    return true;
}
