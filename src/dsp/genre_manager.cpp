#include "genre_manager.h"

#include "drum_genre_templates.h"
#include "miniacid_engine.h"

#include <cmath>

const GenerativeParams kGenerativePresets[kGenerativeModeCount] = {
    {8, 14, 36, 72, 0.40f, 0.50f, 0.8f, 0.0f, 0.1f, 85, 120,
     false, true, 0.25f, 0.10f, 0.15f, false, false, false, 0.6f,
     0.20f, false, 8},
    {10, 14, 48, 72, 0.12f, 0.25f, 0.70f, 0.08f, 0.01f, 90, 118,
     false, true, 0.10f, 0.03f, 0.05f, false, false, false, 0.4f,
     0.12f, false, 6},
    {4, 7, 24, 48, 0.05f, 0.50f, 0.35f, 0.0f, 0.0f, 100, 125,
     true, true, 0.70f, 0.0f, 0.03f, false, true, false, 0.25f,
     0.05f, false, 8},
    {6, 10, 36, 60, 0.0f, 0.70f, 0.3f, 0.0f, 0.0f, 105, 115,
     false, true, 0.30f, 0.05f, 0.10f, false, false, false, 0.5f,
     0.35f, false, 8},
    {12, 16, 36, 72, 0.20f, 0.80f, 0.5f, 0.0f, 0.0f, 110, 127,
     false, true, 0.20f, 0.05f, 0.20f, false, false, false, 0.7f,
     0.08f, false, 8},
    {4, 8, 24, 48, 0.05f, 0.15f, 0.55f, 0.20f, 0.15f, 80, 110,
     false, true, 0.60f, 0.12f, 0.05f, true, true, true, 0.25f,
     0.28f, true, 6},
    {5, 9, 36, 60, 0.05f, 0.25f, 0.60f, 0.18f, 0.25f, 75, 108,
     false, true, 0.35f, 0.18f, 0.10f, true, true, false, 0.20f,
     0.30f, true, 6},
    {7, 12, 36, 72, 0.10f, 0.35f, 0.45f, 0.28f, 0.12f, 90, 120,
     false, true, 0.20f, 0.08f, 0.12f, false, false, false, 0.35f,
     0.45f, true, 8},
    {8, 12, 48, 72, 0.02f, 0.15f, 0.38f, 0.0f, 0.0f, 96, 122,
     true, true, 0.40f, 0.02f, 0.06f, false, true, true, 0.12f,
     0.02f, false, 4},
};

const TextureParams kTexturePresets[kTextureModeCount] = {
    {{3, 5, 8, 85, 0}, 0, 0, false, 0, 0, 0, 0, 0},
    {{10, 15, 10, 68, 0}, -100, 0, true, 0.75f, 0.5f, 0.50f, 2, -2},
    {{15, 20, 12, 60, 0}, -150, -0.1f, true, 0.5f, 0.3f, 0.15f, 3, -4},
    {{5, 30, 20, 75, 0}, 100, 0.15f, true, 0.25f, 0.2f, 0.1f, 1, 3},
    {{18, 35, 22, 78, 1}, 120, 0.10f, true, 0.75f, 0.62f, 0.42f, 2, 4},
};

const GenrePreset kGenrePresets[8] = {
    {GenerativeMode::Acid, TextureMode::Clean, "Classic Acid"},
    {GenerativeMode::Outrun, TextureMode::Clean, "Outrun Lead"},
    {GenerativeMode::Darksynth, TextureMode::Clean, "Darksynth Bass"},
    {GenerativeMode::Outrun, TextureMode::Dub, "Synthwave"},
    {GenerativeMode::Electro, TextureMode::Industrial, "EBM"},
    {GenerativeMode::Rave, TextureMode::Clean, "Rave Acid"},
    {GenerativeMode::Darksynth, TextureMode::Industrial, "Hotline"},
    {GenerativeMode::Electro, TextureMode::Clean, "Detroit"},
};

namespace {

float clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

float lerpf(float a, float b, float amount) {
    return a + (b - a) * amount;
}

int lerpi(int a, int b, float amount) {
    return static_cast<int>(std::lround(
        static_cast<float>(a) + static_cast<float>(b - a) * amount));
}

int generativeIndex(GenerativeMode mode) {
    const int value = static_cast<int>(mode);
    return value >= 0 && value < kGenerativeModeCount ? value : 0;
}

int textureIndex(TextureMode mode) {
    const int value = static_cast<int>(mode);
    return value >= 0 && value < kTextureModeCount ? value : 0;
}

GenerativeMode sceneGenerativeMode(const GenreSettings& settings) {
    return static_cast<GenerativeMode>(
        settings.generativeMode < kGenerativeModeCount
            ? settings.generativeMode
            : 0);
}

TextureMode sceneTextureMode(const GenreSettings& settings) {
    return static_cast<TextureMode>(
        settings.textureMode < kTextureModeCount ? settings.textureMode : 0);
}

constexpr uint8_t kAllowedTextureMask[kGenerativeModeCount] = {
    0b11111,
    0b00111,
    0b11111,
    0b11011,
    0b11001,
    0b00111,
    0b00111,
    0b11111,
    0b10101,
};

struct RecipeOverride {
    int minNotes = -1;
    int maxNotes = -1;
    float swingAmount = -1.0f;
    float microTimingAmount = -1.0f;
    int velocityMin = -1;
    int velocityMax = -1;
    float rootNoteBias = -1.0f;
    float ghostProbability = -1.0f;
    float chromaticProbability = -1.0f;
    float fillProbability = -1.0f;
    int sparseKick = -1;
    int sparseHats = -1;
    int noAccents = -1;
    float drumSyncopation = -1.0f;
    int drumPreferOffbeat = -1;
    int drumVoiceCount = -1;
};

struct GenreRecipeDef {
    GenreRecipeId id = 0;
    const char* name = "";
    RecipeOverride params;
    bool hasDrumOverride = false;
    DrumGenreTemplate drum{};
};

const GenreRecipeDef kGenreRecipes[] = {
    {1, "UK Garage",
     {6, 10, 0.28f, 0.18f, 84, 116, 0.45f, 0.12f, 0.10f, 0.28f,
      0, 0, 0, 0.55f, 1, 8},
     true,
     {0x8121, 0x0808, 0xFFFF, 0x2222, 0.08f, 0.10f, 0.35f,
      104, 102, 82, false, true}},
    {2, "Drum&Bass",
     {7, 12, 0.08f, 0.10f, 96, 124, 0.40f, 0.06f, 0.10f, 0.35f,
      0, 0, 0, 0.65f, 1, 8},
     true,
     {0x8060, 0x0808, 0xFFFF, 0x2222, 0.05f, 0.08f, 0.25f,
      118, 110, 98, false, false}},
    {3, "Footwork",
     {8, 12, 0.00f, 0.22f, 90, 120, 0.35f, 0.08f, 0.14f, 0.30f,
      0, 0, 0, 0.80f, 1, 8},
     true,
     {0x9129, 0x0808, 0xFFFF, 0x1111, 0.10f, 0.10f, 0.42f,
      112, 108, 90, false, true}},
    {4, "Psytrance",
     {12, 16, 0.00f, 0.04f, 102, 126, 0.22f, 0.05f, 0.10f, 0.55f,
      0, 0, 0, 0.12f, 0, 8},
     true,
     {0x8888, 0x0808, 0xFFFF, 0x2222, 0.04f, 0.04f, 0.15f,
      122, 112, 102, false, true}},
    {5, "Dub Techno",
     {3, 6, 0.12f, 0.16f, 72, 108, 0.78f, 0.28f, 0.05f, 0.18f,
      1, 1, 1, 0.30f, 1, 6},
     true,
     {0x8080, 0x0808, 0x2222, 0x0202, 0.10f, 0.06f, 0.22f,
      92, 88, 70, true, false}},
    {6, "Chicago Jack",
     {8, 13, 0.02f, 0.04f, 76, 122, 0.58f, 0.04f, 0.08f, 0.35f,
      0, 0, 0, 0.20f, 0, 8},
     true,
     {0x8888, 0x0808, 0x2222, 0x0202, 0.04f, 0.04f, 0.18f,
      122, 102, 82, false, true}},
    {7, "Rolling Acid",
     {10, 16, 0.04f, 0.06f, 82, 124, 0.42f, 0.05f, 0.10f, 0.42f,
      0, 0, 0, 0.24f, 0, 8},
     true,
     {0x8888, 0x0808, 0xAAAA, 0x2222, 0.04f, 0.05f, 0.20f,
      124, 104, 84, false, true}},
    {8, "Classic 2-Step",
     {4, 8, 0.32f, 0.18f, 82, 116, 0.52f, 0.14f, 0.08f, 0.28f,
      0, 0, 0, 0.58f, 1, 8},
     true,
     {0x8121, 0x0808, 0xFFFF, 0x2222, 0.08f, 0.10f, 0.35f,
      104, 102, 82, false, true}},
    {9, "Dark Skippy",
     {4, 9, 0.34f, 0.20f, 80, 116, 0.48f, 0.16f, 0.10f, 0.32f,
      0, 0, 0, 0.66f, 1, 8},
     true,
     {0x8121, 0x0808, 0xAAAA, 0x2222, 0.10f, 0.12f, 0.40f,
      106, 102, 80, false, true}},
    {10, "Deep Chord",
     {3, 6, 0.10f, 0.10f, 72, 108, 0.80f, 0.24f, 0.04f, 0.16f,
      1, 1, 1, 0.28f, 1, 6},
     true,
     {0x8888, 0x0808, 0x2222, 0x0202, 0.08f, 0.06f, 0.20f,
      94, 88, 70, true, false}},
    {11, "Minimal Space",
     {2, 5, 0.02f, 0.08f, 68, 104, 0.84f, 0.20f, 0.03f, 0.12f,
      1, 1, 1, 0.22f, 1, 6},
     true,
     {0x8080, 0x0808, 0x2222, 0x0202, 0.06f, 0.05f, 0.16f,
      90, 84, 68, true, false}},
};

const GenreRecipeDef* findRecipe(GenreRecipeId id) {
    if (id == kBaseRecipeId) return nullptr;
    for (const GenreRecipeDef& recipe : kGenreRecipes) {
        if (recipe.id == id) return &recipe;
    }
    return nullptr;
}

void applyRecipeOverride(GenerativeParams& params,
                         const RecipeOverride& overrideValues) {
    if (overrideValues.minNotes >= 0) params.minNotes = overrideValues.minNotes;
    if (overrideValues.maxNotes >= 0) params.maxNotes = overrideValues.maxNotes;
    if (overrideValues.swingAmount >= 0.0f)
        params.swingAmount = overrideValues.swingAmount;
    if (overrideValues.microTimingAmount >= 0.0f)
        params.microTimingAmount = overrideValues.microTimingAmount;
    if (overrideValues.velocityMin >= 0)
        params.velocityMin = overrideValues.velocityMin;
    if (overrideValues.velocityMax >= 0)
        params.velocityMax = overrideValues.velocityMax;
    if (overrideValues.rootNoteBias >= 0.0f)
        params.rootNoteBias = overrideValues.rootNoteBias;
    if (overrideValues.ghostProbability >= 0.0f)
        params.ghostProbability = overrideValues.ghostProbability;
    if (overrideValues.chromaticProbability >= 0.0f)
        params.chromaticProbability = overrideValues.chromaticProbability;
    if (overrideValues.fillProbability >= 0.0f)
        params.fillProbability = overrideValues.fillProbability;
    if (overrideValues.sparseKick >= 0)
        params.sparseKick = overrideValues.sparseKick != 0;
    if (overrideValues.sparseHats >= 0)
        params.sparseHats = overrideValues.sparseHats != 0;
    if (overrideValues.noAccents >= 0)
        params.noAccents = overrideValues.noAccents != 0;
    if (overrideValues.drumSyncopation >= 0.0f)
        params.drumSyncopation = overrideValues.drumSyncopation;
    if (overrideValues.drumPreferOffbeat >= 0)
        params.drumPreferOffbeat = overrideValues.drumPreferOffbeat != 0;
    if (overrideValues.drumVoiceCount >= 0)
        params.drumVoiceCount = overrideValues.drumVoiceCount;
}

void clampGenerativeParams(GenerativeParams& params) {
    if (params.minNotes < 0) params.minNotes = 0;
    if (params.maxNotes < params.minNotes) params.maxNotes = params.minNotes;
    if (params.maxNotes > 16) params.maxNotes = 16;
    if (params.minNotes > 16) params.minNotes = 16;

    if (params.minOctave > params.maxOctave) {
        const int value = params.minOctave;
        params.minOctave = params.maxOctave;
        params.maxOctave = value;
    }

    params.slideProbability = clamp01(params.slideProbability);
    params.accentProbability = clamp01(params.accentProbability);
    params.gateLengthMultiplier =
        params.gateLengthMultiplier < 0.1f
            ? 0.1f
            : (params.gateLengthMultiplier > 1.0f
                   ? 1.0f
                   : params.gateLengthMultiplier);
    params.swingAmount =
        params.swingAmount < 0.0f
            ? 0.0f
            : (params.swingAmount > 0.66f ? 0.66f : params.swingAmount);
    params.microTimingAmount = clamp01(params.microTimingAmount);
    params.velocityMin =
        params.velocityMin < 1
            ? 1
            : (params.velocityMin > 127 ? 127 : params.velocityMin);
    params.velocityMax =
        params.velocityMax < params.velocityMin
            ? params.velocityMin
            : (params.velocityMax > 127 ? 127 : params.velocityMax);
    params.rootNoteBias = clamp01(params.rootNoteBias);
    params.ghostProbability = clamp01(params.ghostProbability);
    params.chromaticProbability = clamp01(params.chromaticProbability);
    params.fillProbability = clamp01(params.fillProbability);
    params.drumSyncopation = clamp01(params.drumSyncopation);
    params.drumVoiceCount =
        params.drumVoiceCount < 1
            ? 1
            : (params.drumVoiceCount > 8 ? 8 : params.drumVoiceCount);
}

}  // namespace

namespace GenreCatalog {

uint8_t recipeCount() {
    return static_cast<uint8_t>(
        1 + sizeof(kGenreRecipes) / sizeof(kGenreRecipes[0]));
}

const char* recipeName(GenreRecipeId id) {
    if (id == kBaseRecipeId) return "BASE";
    const GenreRecipeDef* recipe = findRecipe(id);
    return recipe ? recipe->name : "BASE";
}

const char* generativeModeName(GenerativeMode mode) {
    static const char* const names[kGenerativeModeCount] = {
        "Acid", "Minimal", "Techno", "Electro", "Rave",
        "Reggae", "TripHop", "Broken", "Chip",
    };
    return names[generativeIndex(mode)];
}

const char* textureModeName(TextureMode mode) {
    static const char* const names[kTextureModeCount] = {
        "Clean", "Dub", "LoFi", "Industrial", "Psychedelic",
    };
    return names[textureIndex(mode)];
}

bool isTextureAllowed(GenerativeMode genre, TextureMode texture) {
    const int genreValue = static_cast<int>(genre);
    const int textureValue = static_cast<int>(texture);
    if (genreValue < 0 || genreValue >= kGenerativeModeCount) return false;
    if (textureValue < 0 || textureValue >= kTextureModeCount) return false;
    return (kAllowedTextureMask[genreValue] & (1u << textureValue)) != 0;
}

TextureMode firstAllowedTexture(GenerativeMode genre) {
    for (int index = 0; index < kTextureModeCount; ++index) {
        const TextureMode mode = static_cast<TextureMode>(index);
        if (isTextureAllowed(genre, mode)) return mode;
    }
    return TextureMode::Clean;
}

TextureMode nextAllowedTexture(GenerativeMode genre,
                               TextureMode current,
                               int direction) {
    if (direction == 0) return current;
    int index = textureIndex(current);
    for (int count = 0; count < kTextureModeCount; ++count) {
        index = (index + direction + kTextureModeCount) % kTextureModeCount;
        const TextureMode mode = static_cast<TextureMode>(index);
        if (isTextureAllowed(genre, mode)) return mode;
    }
    return firstAllowedTexture(genre);
}

GrooveboxMode grooveboxModeForGenerative(GenerativeMode mode) {
    switch (mode) {
        case GenerativeMode::Acid: return GrooveboxMode::Acid;
        case GenerativeMode::Outrun: return GrooveboxMode::Minimal;
        case GenerativeMode::Darksynth: return GrooveboxMode::Electro;
        case GenerativeMode::Electro: return GrooveboxMode::Electro;
        case GenerativeMode::Rave: return GrooveboxMode::Acid;
        case GenerativeMode::Reggae: return GrooveboxMode::Dub;
        case GenerativeMode::TripHop: return GrooveboxMode::Dub;
        case GenerativeMode::Broken: return GrooveboxMode::Breaks;
        case GenerativeMode::Chip: return GrooveboxMode::Electro;
        default: return GrooveboxMode::Minimal;
    }
}

GrooveboxMode grooveboxModeForRecipe(GenreRecipeId id,
                                     GenerativeMode fallbackMode) {
    switch (id) {
        case 1: return GrooveboxMode::Breaks;
        case 2: return GrooveboxMode::Breaks;
        case 3: return GrooveboxMode::Breaks;
        case 4: return GrooveboxMode::Acid;
        case 5: return GrooveboxMode::Dub;
        case 6: return GrooveboxMode::Acid;
        case 7: return GrooveboxMode::Acid;
        case 8: return GrooveboxMode::Breaks;
        case 9: return GrooveboxMode::Breaks;
        case 10: return GrooveboxMode::Dub;
        case 11: return GrooveboxMode::Dub;
        case 0:
        default:
            return grooveboxModeForGenerative(fallbackMode);
    }
}

GenerativeParams compiledGenerativeParams(const GenreSettings& settings) {
    const GenerativeMode mode = sceneGenerativeMode(settings);
    GenerativeParams params = kGenerativePresets[generativeIndex(mode)];

    const GenreRecipeDef* baseRecipe = findRecipe(settings.recipe);
    if (baseRecipe) applyRecipeOverride(params, baseRecipe->params);

    const GenreRecipeDef* morphRecipe = findRecipe(settings.morphTarget);
    if (morphRecipe && settings.morphAmount > 0 &&
        settings.morphTarget != settings.recipe) {
        GenerativeParams target = kGenerativePresets[generativeIndex(mode)];
        applyRecipeOverride(target, morphRecipe->params);
        clampGenerativeParams(target);

        const float amount =
            static_cast<float>(settings.morphAmount) / 255.0f;
        params.minNotes = lerpi(params.minNotes, target.minNotes, amount);
        params.maxNotes = lerpi(params.maxNotes, target.maxNotes, amount);
        params.swingAmount =
            lerpf(params.swingAmount, target.swingAmount, amount);
        params.microTimingAmount =
            lerpf(params.microTimingAmount, target.microTimingAmount, amount);
        params.velocityMin =
            lerpi(params.velocityMin, target.velocityMin, amount);
        params.velocityMax =
            lerpi(params.velocityMax, target.velocityMax, amount);
        params.rootNoteBias =
            lerpf(params.rootNoteBias, target.rootNoteBias, amount);
        params.ghostProbability =
            lerpf(params.ghostProbability, target.ghostProbability, amount);
        params.chromaticProbability = lerpf(
            params.chromaticProbability, target.chromaticProbability, amount);
        params.fillProbability =
            lerpf(params.fillProbability, target.fillProbability, amount);
        params.drumSyncopation =
            lerpf(params.drumSyncopation, target.drumSyncopation, amount);
        params.drumVoiceCount =
            lerpi(params.drumVoiceCount, target.drumVoiceCount, amount);
        if (amount >= 0.5f) {
            params.sparseKick = target.sparseKick;
            params.sparseHats = target.sparseHats;
            params.noAccents = target.noAccents;
            params.drumPreferOffbeat = target.drumPreferOffbeat;
        }
    }

    clampGenerativeParams(params);
    return params;
}

const DrumGenreTemplate* drumTemplateOverride(
        const GenreSettings& settings) {
    const GenreRecipeDef* selected = findRecipe(settings.recipe);
    const DrumGenreTemplate* result =
        selected && selected->hasDrumOverride ? &selected->drum : nullptr;

    const GenreRecipeDef* morph = findRecipe(settings.morphTarget);
    if (morph && morph->hasDrumOverride && settings.morphAmount >= 128 &&
        settings.morphTarget != settings.recipe) {
        result = &morph->drum;
    }
    return result;
}

GenreBehavior behavior(const GenreSettings& settings) {
    static const GenreBehavior base[kGenerativeModeCount] = {
        {0xFFFF, 4, 1, true, true, true, false,
         {0.0f, 0.55f, 0.35f, 0.85f, 0.35f}},
        {0xFFFF, 6, 2, true, false, true, false,
         {0.0f, 0.72f, 0.18f, 0.58f, 0.30f}},
        {0xAAAA, 3, 1, true, false, false, false,
         {1.0f, 0.34f, 0.50f, 0.92f, 0.22f}},
        {0xAA55, 3, 3, true, true, false, false,
         {0.2f, 0.60f, 0.30f, 0.75f, 0.20f}},
        {0xFFFF, 6, 1, true, true, true, false,
         {0.0f, 0.78f, 0.32f, 0.80f, 0.50f}},
        {0xAAAA, 4, 0, true, false, false, true,
         {1.0f, 0.28f, 0.40f, 0.55f, 0.18f}},
        {0xF0F0, 4, 2, true, false, false, true,
         {0.2f, 0.45f, 0.25f, 0.55f, 0.30f}},
        {0xAA55, 3, 3, true, true, true, false,
         {0.0f, 0.62f, 0.32f, 0.70f, 0.25f}},
        {0xFFFF, 2, 0, true, false, false, true,
         {1.0f, 0.68f, 0.22f, 0.82f, 0.12f}},
    };

    GenreBehavior result =
        base[generativeIndex(sceneGenerativeMode(settings))];

    if (settings.recipe == 6) {
        result.stepMask = 0xFFFF;
        result.motifLength = 4;
        result.preferredScale = 1;
        result.useMotif = true;
        result.allowChromatic = true;
        result.forceOctaveJump = false;
        result.avoidClusters = false;
        result.timbre = {0.0f, 0.52f, 0.55f, 0.82f, 0.25f};
    } else if (settings.recipe == 7) {
        result.stepMask = 0xFFFF;
        result.motifLength = 6;
        result.preferredScale = 1;
        result.useMotif = true;
        result.allowChromatic = true;
        result.forceOctaveJump = false;
        result.avoidClusters = false;
        result.timbre = {0.0f, 0.60f, 0.62f, 0.88f, 0.32f};
    } else if (settings.recipe == 8 || settings.recipe == 9) {
        result.stepMask = 0xAA55;
        result.motifLength = 4;
        result.preferredScale = 3;
        result.useMotif = true;
        result.allowChromatic = false;
        result.forceOctaveJump = false;
        result.avoidClusters = true;
        result.timbre = {0.2f, 0.46f, 0.24f, 0.58f, 0.22f};
    } else if (settings.recipe == 10 || settings.recipe == 11) {
        result.stepMask = 0x8888;
        result.motifLength = 4;
        result.preferredScale = 3;
        result.useMotif = true;
        result.allowChromatic = false;
        result.forceOctaveJump = false;
        result.avoidClusters = true;
        result.timbre = {1.0f, 0.30f, 0.34f, 0.50f, 0.20f};
    }

    return result;
}

GrooveRecipe grooveRecipe(const GenreSettings& settings) {
    const GenerativeParams params = compiledGenerativeParams(settings);
    GrooveRecipe recipe;
    recipe.stepsPerBar = 16;
    recipe.swingPercent =
        50 + static_cast<uint8_t>(std::round(params.swingAmount * 100.0f));
    if (recipe.swingPercent < 50) recipe.swingPercent = 50;
    if (recipe.swingPercent > 75) recipe.swingPercent = 75;
    recipe.gateLengthRatio = params.gateLengthMultiplier;
    recipe.densityMin = static_cast<float>(params.minNotes) / 16.0f;
    recipe.densityMax = static_cast<float>(params.maxNotes) / 16.0f;
    recipe.velMin = static_cast<uint8_t>(params.velocityMin);
    recipe.velMax = static_cast<uint8_t>(params.velocityMax);
    recipe.swingMask = 0xFFFF;
    recipe.sparseKick = params.sparseKick;
    recipe.noAccents = params.noAccents;
    recipe.preferDownbeats = params.preferDownbeats;
    return recipe;
}

}  // namespace GenreCatalog

GenreSettings& GenreSceneView::settings() {
    return scenes_.currentScene().genre;
}

const GenreSettings& GenreSceneView::settings() const {
    return scenes_.currentScene().genre;
}

void GenreSceneView::setGenerativeMode(GenerativeMode mode) {
    settings().generativeMode =
        static_cast<uint8_t>(generativeIndex(mode));
}

void GenreSceneView::setTextureMode(TextureMode mode) {
    settings().textureMode = static_cast<uint8_t>(textureIndex(mode));
}

void GenreSceneView::setRecipe(GenreRecipeId value) {
    settings().recipe =
        value < GenreCatalog::recipeCount() ? value : kBaseRecipeId;
}

void GenreSceneView::setMorphTarget(GenreRecipeId value) {
    settings().morphTarget =
        value < GenreCatalog::recipeCount() ? value : kBaseRecipeId;
}

void GenreSceneView::setMorphAmount(uint8_t value) {
    settings().morphAmount = value;
}

void GenreSceneView::cycleGenerative(int direction) {
    int next = generativeIndex(generativeMode()) + direction;
    while (next < 0) next += kGenerativeModeCount;
    while (next >= kGenerativeModeCount) next -= kGenerativeModeCount;
    settings().generativeMode = static_cast<uint8_t>(next);
}

void GenreSceneView::cycleTexture(int direction) {
    int next = textureIndex(textureMode()) + direction;
    while (next < 0) next += kTextureModeCount;
    while (next >= kTextureModeCount) next -= kTextureModeCount;
    settings().textureMode = static_cast<uint8_t>(next);
}

void GenreSceneView::cycleRecipe(int direction) {
    const int count = static_cast<int>(GenreCatalog::recipeCount());
    if (count <= 0) return;
    int next = static_cast<int>(recipe()) + direction;
    while (next < 0) next += count;
    while (next >= count) next -= count;
    settings().recipe = static_cast<uint8_t>(next);
}

GenerativeMode GenreSceneView::generativeMode() const {
    return sceneGenerativeMode(settings());
}

TextureMode GenreSceneView::textureMode() const {
    return sceneTextureMode(settings());
}

GenreRecipeId GenreSceneView::recipe() const {
    return settings().recipe < GenreCatalog::recipeCount()
               ? settings().recipe
               : kBaseRecipeId;
}

GenreRecipeId GenreSceneView::morphTarget() const {
    return settings().morphTarget < GenreCatalog::recipeCount()
               ? settings().morphTarget
               : kBaseRecipeId;
}

uint8_t GenreSceneView::morphAmount() const {
    return settings().morphAmount;
}

GenerativeParams GenreSceneView::getCompiledGenerativeParams() const {
    return GenreCatalog::compiledGenerativeParams(settings());
}

GrooveRecipe GenreSceneView::getGrooveRecipe() const {
    return GenreCatalog::grooveRecipe(settings());
}

const DrumGenreTemplate* GenreSceneView::drumTemplateOverride() const {
    return GenreCatalog::drumTemplateOverride(settings());
}

const TextureParams& GenreSceneView::getTextureParams() const {
    return kTexturePresets[textureIndex(textureMode())];
}

GenreBehavior GenreSceneView::getBehavior() const {
    return GenreCatalog::behavior(settings());
}

void GenreSceneView::syncTextureBiasBaselineFromCurrentState() {
    const TextureParams& params = getTextureParams();
    lastAppliedCutoffBias_ =
        static_cast<int>(params.filterCutoffBias / 5.0f);
    lastAppliedResBias_ =
        static_cast<int>(params.filterResonanceBias * 40.0f);
}

void GenreSceneView::applyGenreTimbre(MiniAcid& engine) {
    const bool atlasAcidRecipe = recipe() == 6 || recipe() == 7;
    const bool atlasHybridRecipe = recipe() >= 8 && recipe() <= 11;
    if (atlasAcidRecipe) {
        engine.setSynthEngine(0, "TB303");
        engine.setSynthEngine(1, "TB303");
    } else if (atlasHybridRecipe) {
        engine.setSynthEngine(0, "TB303");
        engine.setSynthEngine(1, "OPL2");
    }

    const GenreBehavior b = getBehavior();
    const GenreTimbre& t = b.timbre;
    for (int v = 0; v < 2; ++v) {
        if (engine.currentSynthEngineName(v) != "TB303") continue;

        engine.set303ParameterNormalized(
            TB303ParamId::Oscillator, t.osc, v);

        float cut = t.cutoff;
        float reso = t.resonance;
        float env = t.envAmount;
        float decay = t.envDecay;

        if (v == 0) {
            cut = clamp01(cut);
            reso = clamp01(reso);
            env = clamp01(env);
            decay = clamp01(decay);

            if (cut < 0.18f) cut = 0.18f;
            if (cut > 0.62f) cut = 0.62f;
            if (env < 0.18f) env = 0.18f;
            if (env > 0.55f) env = 0.55f;
            if (decay < 0.10f) decay = 0.10f;
            if (decay > 0.45f) decay = 0.45f;
            if (reso < 0.0f) reso = 0.0f;
            if (reso > 0.85f) reso = 0.85f;
        } else {
            if (cut < 0.40f) cut = 0.40f;
            if (env < 0.20f) env = 0.20f;
            if (decay < 0.08f) decay = 0.08f;
            if (cut > 0.95f) cut = 0.95f;
            if (reso > 0.95f) reso = 0.95f;
        }

        engine.set303ParameterNormalized(
            TB303ParamId::Cutoff, clamp01(cut), v);
        engine.set303ParameterNormalized(
            TB303ParamId::Resonance, clamp01(reso), v);
        engine.set303ParameterNormalized(
            TB303ParamId::EnvAmount, clamp01(env), v);
        engine.set303ParameterNormalized(
            TB303ParamId::EnvDecay, clamp01(decay), v);
    }
}

void GenreSceneView::applyTexture(MiniAcid& engine) {
    const TextureParams& params = getTextureParams();
    const float amount =
        clamp01(settings().textureAmount / 100.0f);

    TapeState& tape = engine.sceneManager().currentScene().tape;
    TapeMacro macro = params.tapeMacro;
    macro.wow = static_cast<uint8_t>(macro.wow * amount);
    macro.age = static_cast<uint8_t>(macro.age * amount);
    macro.sat = static_cast<uint8_t>(macro.sat * amount);
    macro.crush = static_cast<uint8_t>(macro.crush * amount);
    constexpr int neutralTone = 85;
    macro.tone = static_cast<uint8_t>(
        neutralTone +
        static_cast<int>(
            (static_cast<int>(params.tapeMacro.tone) - neutralTone) *
            amount));
    tape.macro = macro;

    const bool tapeOn =
        textureMode() != TextureMode::Clean && amount > 0.01f;
    tape.fxEnabled = tapeOn;
    engine.sceneManager().currentScene().feel.tapeEnabled = tapeOn;

    for (int voice = 0; voice < 2; ++voice) {
        TempoDelay& delay = engine.tempoDelay(voice);
        const bool delayOn =
            params.delayEnabled && amount > 0.01f;
        delay.setEnabled(delayOn);
        if (delayOn) {
            delay.setBeats(params.delayBeats);
            delay.setFeedback(params.delayFeedback * amount);
            delay.setMix(params.delayMix * amount);
        }
    }

    const int newCutoffBias =
        static_cast<int>((params.filterCutoffBias * amount) / 5.0f);
    const int newResBias =
        static_cast<int>((params.filterResonanceBias * amount) * 40.0f);
    const int cutoffDelta =
        newCutoffBias - lastAppliedCutoffBias_;
    const int resDelta =
        newResBias - lastAppliedResBias_;

    if (cutoffDelta != 0) {
        for (int voice = 0; voice < 2; ++voice) {
            if (engine.currentSynthEngineName(voice) == "TB303") {
                engine.adjust303Parameter(
                    TB303ParamId::Cutoff, cutoffDelta, voice);
            }
        }
        lastAppliedCutoffBias_ = newCutoffBias;
    }

    if (resDelta != 0) {
        for (int voice = 0; voice < 2; ++voice) {
            if (engine.currentSynthEngineName(voice) == "TB303") {
                engine.adjust303Parameter(
                    TB303ParamId::Resonance, resDelta, voice);
            }
        }
        lastAppliedResBias_ = newResBias;
    }
}
