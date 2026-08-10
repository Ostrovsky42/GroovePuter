#ifndef GENRE_MANAGER_H
#define GENRE_MANAGER_H

#include <stdint.h>

#include "src/dsp/mini_dsp_params.h"

// Persisted values 0..8 remain byte-compatible with existing Scene documents.
// New modes are append-only. Latin remains HARDWARE_PENDING until a production
// rhythm topology has a recorded repository verdict.
enum class GenerativeMode : uint8_t {
    Acid = 0,
    Outrun = 1,
    Darksynth = 2,
    Electro = 3,
    Rave = 4,
    Reggae = 5,
    TripHop = 6,
    Broken = 7,
    Chip = 8,
    House = 9,
    Techno = 10,
    HipHop = 11,
    FunkSoul = 12,
    UkGarage = 13,
    DrumAndBass = 14,
    LoFi = 15
};

static constexpr int kGenerativeModeCount = 16;
using GenreRecipeId = uint8_t;
static constexpr GenreRecipeId kBaseRecipeId = 0;

// Semantic Lo-Fi / Boom-Bap variants. IDs are append-only relative to the
// Stage 13 recipe table; existing recipe IDs 0..11 retain their meaning.
static constexpr GenreRecipeId kClassicChillRecipeId = 12;
static constexpr GenreRecipeId kDrunkenGrooveRecipeId = 13;
static constexpr GenreRecipeId kLoFiHouseRecipeId = 14;
static constexpr GenreRecipeId kMinimalSleepRecipeId = 15;
static constexpr GenreRecipeId kGoldenEraRecipeId = 16;
static constexpr GenreRecipeId kDustyJazzRecipeId = 17;

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

class SceneManager;
struct GenreSettings;
struct DrumGenreTemplate;

namespace GenreCatalog {

uint8_t recipeCount();
const char* recipeName(GenreRecipeId id);
const char* generativeModeName(GenerativeMode mode);

GrooveboxMode grooveboxModeForRecipe(GenreRecipeId id,
                                     GenerativeMode fallbackMode);
GrooveboxMode grooveboxModeForGenerative(GenerativeMode mode);

GenerativeParams compiledGenerativeParams(const GenreSettings& settings);
const DrumGenreTemplate* drumTemplateOverride(const GenreSettings& settings);
GenreBehavior behavior(const GenreSettings& settings);
GrooveRecipe grooveRecipe(const GenreSettings& settings);

}  // namespace GenreCatalog

class GenreSceneView {
public:
    explicit GenreSceneView(SceneManager& scenes) : scenes_(scenes) {}

    void setGenerativeMode(GenerativeMode mode);
    void setRecipe(GenreRecipeId recipe);
    void setMorphTarget(GenreRecipeId target);
    void setMorphAmount(uint8_t amount);

    void cycleGenerative(int direction = 1);
    void cycleRecipe(int direction = 1);

    GenerativeMode generativeMode() const;
    GenreRecipeId recipe() const;
    GenreRecipeId morphTarget() const;
    uint8_t morphAmount() const;

    GenerativeParams getGenerativeParams() const {
        return getCompiledGenerativeParams();
    }
    GenerativeParams getCompiledGenerativeParams() const;
    GrooveRecipe getGrooveRecipe() const;
    const DrumGenreTemplate* drumTemplateOverride() const;
    GenreBehavior getBehavior() const;

    bool commitPendingRecipe() { return false; }

    static const char* generativeModeName(GenerativeMode mode) {
        return GenreCatalog::generativeModeName(mode);
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

private:
    GenreSettings& settings();
    const GenreSettings& settings() const;

    SceneManager& scenes_;
};

using GenreManager = GenreSceneView;

struct GenrePreset {
    GenerativeMode generative;
    const char* name;
};

extern const GenrePreset kGenrePresets[8];
static constexpr int kGenrePresetCount = 8;

#endif  // GENRE_MANAGER_H
