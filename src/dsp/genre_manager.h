#ifndef GENRE_MANAGER_H
#define GENRE_MANAGER_H

#include <stdint.h>

#include "src/dsp/mini_dsp_params.h"
#include "src/dsp/tape_defs.h"

// Persisted values remain byte-compatible with existing Scene documents.
enum class GenerativeMode : uint8_t {
    Acid = 0,
    Outrun = 1,
    Darksynth = 2,
    Electro = 3,
    Rave = 4,
    Reggae = 5,
    TripHop = 6,
    Broken = 7,
    Chip = 8
};

enum class TextureMode : uint8_t {
    Clean = 0,
    Dub = 1,
    LoFi = 2,
    Industrial = 3,
    Psychedelic = 4
};

static constexpr int kGenerativeModeCount = 9;
static constexpr int kTextureModeCount = 5;
using GenreRecipeId = uint8_t;
static constexpr GenreRecipeId kBaseRecipeId = 0;

struct GenerativeParams {
    int minNotes = 4;
    int maxNotes = 8;
    int minOctave = 36;
    int maxOctave = 60;

    float slideProbability = 0.10f;
    float accentProbability = 0.25f;
    float gateLengthMultiplier = 0.50f;

    float swingAmount = 0.0f;
    float microTimingAmount = 0.0f;

    int velocityMin = 80;
    int velocityMax = 110;

    bool preferDownbeats = true;
    bool allowRepeats = true;
    float rootNoteBias = 0.40f;
    float ghostProbability = 0.05f;
    float chromaticProbability = 0.0f;

    bool sparseKick = false;
    bool sparseHats = false;
    bool noAccents = false;
    float fillProbability = 0.15f;

    float drumSyncopation = 0.0f;
    bool drumPreferOffbeat = false;
    int drumVoiceCount = 8;
};

// === GROOVE RECIPE (legacy compact adapter; full generation uses GenerativeParams)
struct GrooveRecipe {
    uint8_t stepsPerBar = 16;
    uint8_t swingPercent = 50;
    float gateLengthRatio = 0.5f;
    float densityMin = 0.25f;
    float densityMax = 0.75f;
    uint8_t velMin = 60;
    uint8_t velMax = 127;
    uint16_t swingMask = 0;
    bool sparseKick = false;
    bool noAccents = false;
    bool preferDownbeats = true;
};

struct TextureParams {
    TapeMacro tapeMacro;
    float filterCutoffBias;
    float filterResonanceBias;
    bool delayEnabled;
    float delayBeats;
    float delayFeedback;
    float delayMix;
    float bassBoostDB;
    float trebleBoostDB;
};

struct GenreTimbre {
    float osc;
    float cutoff;
    float resonance;
    float envAmount;
    float envDecay;
};

struct GenreBehavior {
    uint16_t stepMask;
    uint8_t motifLength;
    uint8_t preferredScale;
    bool useMotif;
    bool allowChromatic;
    bool forceOctaveJump;
    bool avoidClusters;
    GenreTimbre timbre;
};

extern const GenerativeParams kGenerativePresets[kGenerativeModeCount];
extern const TextureParams kTexturePresets[kTextureModeCount];

class MiniAcid;
class SceneManager;
struct GenreSettings;
struct DrumGenreTemplate;

namespace GenreCatalog {

uint8_t recipeCount();
const char* recipeName(GenreRecipeId id);
const char* generativeModeName(GenerativeMode mode);
const char* textureModeName(TextureMode mode);

GrooveboxMode grooveboxModeForRecipe(GenreRecipeId id,
                                     GenerativeMode fallbackMode);
GrooveboxMode grooveboxModeForGenerative(GenerativeMode mode);

bool isTextureAllowed(GenerativeMode genre, TextureMode texture);
TextureMode firstAllowedTexture(GenerativeMode genre);
TextureMode nextAllowedTexture(GenerativeMode genre,
                               TextureMode current,
                               int direction = 1);

GenerativeParams compiledGenerativeParams(const GenreSettings& settings);
const DrumGenreTemplate* drumTemplateOverride(const GenreSettings& settings);
GenreBehavior behavior(const GenreSettings& settings);
GrooveRecipe grooveRecipe(const GenreSettings& settings);

}  // namespace GenreCatalog

// Non-owning runtime adapter. Persisted genre state lives only in Scene::genre.
// The two integers below track already-applied filter deltas; they are transient
// DSP bookkeeping and are never a second copy of genre settings.
class GenreSceneView {
public:
    explicit GenreSceneView(SceneManager& scenes) : scenes_(scenes) {}

    void setGenerativeMode(GenerativeMode mode);
    void setTextureMode(TextureMode mode);
    void setRecipe(GenreRecipeId recipe);
    void setMorphTarget(GenreRecipeId target);
    void setMorphAmount(uint8_t amount);

    void cycleGenerative(int direction = 1);
    void cycleTexture(int direction = 1);
    void cycleRecipe(int direction = 1);

    GenerativeMode generativeMode() const;
    TextureMode textureMode() const;
    GenreRecipeId recipe() const;
    GenreRecipeId morphTarget() const;
    uint8_t morphAmount() const;

    GenerativeParams getGenerativeParams() const {
        return getCompiledGenerativeParams();
    }
    GenerativeParams getCompiledGenerativeParams() const;
    GrooveRecipe getGrooveRecipe() const;
    const DrumGenreTemplate* drumTemplateOverride() const;
    const TextureParams& getTextureParams() const;
    GenreBehavior getBehavior() const;

    // Pending manager-owned state was removed. Current callers retain this
    // no-op boundary until their bar callback is simplified separately.
    bool commitPendingRecipe() { return false; }

    void applyTexture(MiniAcid& engine);
    void applyGenreTimbre(MiniAcid& engine);

    void resetTextureBiasTracking() {
        lastAppliedCutoffBias_ = 0;
        lastAppliedResBias_ = 0;
    }
    void syncTextureBiasBaselineFromCurrentState();

    static const char* generativeModeName(GenerativeMode mode) {
        return GenreCatalog::generativeModeName(mode);
    }
    static const char* textureModeName(TextureMode mode) {
        return GenreCatalog::textureModeName(mode);
    }
    static const char* recipeName(GenreRecipeId id) {
        return GenreCatalog::recipeName(id);
    }
    static uint8_t recipeCount() {
        return GenreCatalog::recipeCount();
    }
    static GrooveboxMode grooveboxModeForRecipe(
            GenreRecipeId id, GenerativeMode fallbackMode) {
        return GenreCatalog::grooveboxModeForRecipe(id, fallbackMode);
    }
    static GrooveboxMode grooveboxModeForGenerative(GenerativeMode mode) {
        return GenreCatalog::grooveboxModeForGenerative(mode);
    }
    static bool isTextureAllowed(GenerativeMode genre, TextureMode texture) {
        return GenreCatalog::isTextureAllowed(genre, texture);
    }
    static TextureMode firstAllowedTexture(GenerativeMode genre) {
        return GenreCatalog::firstAllowedTexture(genre);
    }
    static TextureMode nextAllowedTexture(
            GenerativeMode genre, TextureMode current, int direction = 1) {
        return GenreCatalog::nextAllowedTexture(genre, current, direction);
    }

private:
    GenreSettings& settings();
    const GenreSettings& settings() const;

    SceneManager& scenes_;
    int lastAppliedCutoffBias_ = 0;
    int lastAppliedResBias_ = 0;
};

// Transitional source compatibility for production call sites. This is a type
// alias to a non-owning Scene view; there is no GenreManager class or state owner.
using GenreManager = GenreSceneView;

struct GenrePreset {
    GenerativeMode generative;
    TextureMode texture;
    const char* name;
};

extern const GenrePreset kGenrePresets[8];
static constexpr int kGenrePresetCount = 8;

#endif  // GENRE_MANAGER_H
