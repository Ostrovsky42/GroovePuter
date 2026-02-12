#include "advanced_pattern_generator.h"
#include "drum_genre_templates.h"
#include <vector>
#include <algorithm>
#include <cmath>

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

// ============================================================================
// AdvancedPatternGenerator Implementation (Synths)
// ============================================================================

void AdvancedPatternGenerator::generatePattern(SynthPattern& pattern, const GeneratorParams& params) {
    // Clear existing steps
    for(int i=0; i<SynthPattern::kSteps; ++i) {
        pattern.steps[i].note = -1;
        pattern.steps[i].slide = false;
        pattern.steps[i].accent = false;
        pattern.steps[i].velocity = 100;
        pattern.steps[i].timing = 0;
        pattern.steps[i].ghost = false;
        pattern.steps[i].probability = 100;
    }
    
    const int minNotes = std::max(0, std::min(SynthPattern::kSteps, params.minNotes));
    const int maxNotes = std::max(minNotes, std::min(SynthPattern::kSteps, params.maxNotes));
    const int noteSpan = maxNotes - minNotes + 1;
    int numNotes = minNotes + (noteSpan > 1 ? (rand() % noteSpan) : 0);
    std::vector<int> positions = generatePositions(numNotes, params);
    
    for (int step : positions) {
        pattern.steps[step].note = generateNote(step, params);
        pattern.steps[step].velocity = generateVelocity(step, params);
        pattern.steps[step].timing = generateTiming(step, params);
        
        if ((rand() % 100) < (params.ghostNoteProbability * 100)) {
            pattern.steps[step].ghost = true;
            pattern.steps[step].velocity = 40; 
        }
        
        if ((rand() % 100) < 30) pattern.steps[step].accent = true;
        if ((rand() % 100) < 15) pattern.steps[step].slide = true;
    }
    
    applySwing(pattern, params.swingAmount);
}

std::vector<int> AdvancedPatternGenerator::generatePositions(int count, const GeneratorParams& params) {
    std::vector<int> positions;
    if (params.preferDownbeats) {
        const int goodPositions[] = {0, 4, 8, 12, 2, 6, 10, 14};
        const int offbeats[] = {1, 3, 5, 7, 9, 11, 13, 15};
        
        for (int i = 0; i < count; i++) {
            if ((rand() % 100) < 75 && positions.size() < 8) {
                positions.push_back(goodPositions[rand() % 8]);
            } else {
                positions.push_back(offbeats[rand() % 8]);
            }
        }
    } else {
        for (int i = 0; i < count; i++) positions.push_back(rand() % 16);
    }
    std::sort(positions.begin(), positions.end());
    positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
    return positions;
}

int AdvancedPatternGenerator::generateNote(int step, const GeneratorParams& params) {
    (void)step;
    const int minOct = std::min(params.minOctave, params.maxOctave);
    const int maxOct = std::max(params.minOctave, params.maxOctave);
    const int octSpan = maxOct - minOct + 1;
    int note = minOct + (octSpan > 1 ? (rand() % octSpan) : 0);
    if (params.scaleQuantize) {
        note = quantizeToScale(note, params.scaleRoot, params.scale);
    }
    return note;
}

uint8_t AdvancedPatternGenerator::generateVelocity(int step, const GeneratorParams& params) {
    int val = 100 + ((rand() % 41) - 20) * params.velocityRange;
    if (isDownbeat(step)) val += 15;
    return (uint8_t)std::max(40, std::min(127, val));
}

int8_t AdvancedPatternGenerator::generateTiming(int step, const GeneratorParams& params) {
    if (params.microTimingAmount == 0) return 0;
    int maxOffset = 24 * params.microTimingAmount;
    return (rand() % (maxOffset * 2 + 1)) - maxOffset;
}

void AdvancedPatternGenerator::applySwing(SynthPattern& pattern, float amount) {
    if (amount <= 0.01f) return;
    int swingTicks = (int)(amount * 48.0f); // Assuming 96 PPQN, 16th = 24 ticks. 50% swing = 12 ticks.
    for (int step = 1; step < SynthPattern::kSteps; step += 2) {
        if (pattern.steps[step].note >= 0) {
            pattern.steps[step].timing = (int8_t)std::min(127, (int)pattern.steps[step].timing + swingTicks);
        }
    }
}

int AdvancedPatternGenerator::quantizeToScale(int note, int root, ScaleType scale) {
    static const int intervals[][7] = {
        {0, 2, 3, 5, 7, 8, 10}, // MINOR
        {0, 2, 4, 5, 7, 9, 11}, // MAJOR
        {0, 2, 3, 5, 7, 9, 10}, // DORIAN
        {0, 1, 3, 5, 7, 8, 10}, // PHRYGIAN
        {0, 2, 4, 6, 7, 9, 11}, // LYDIAN
        {0, 2, 4, 5, 7, 9, 10}, // MIXOLYDIAN
        {0, 1, 3, 5, 6, 8, 10}  // LOCRIAN
    };
    
    const int* pIntervals = intervals[static_cast<int>(scale % 7)];
    int octave = note / 12;
    int semitone = note % 12;
    int closest = 0;
    int minDist = 12;
    for (int i = 0; i < 7; i++) {
        int scaleTone = (root + pIntervals[i]) % 12;
        int dist = std::abs(semitone - scaleTone);
        if (dist < minDist) { minDist = dist; closest = scaleTone; }
    }
    return octave * 12 + closest;
}

bool AdvancedPatternGenerator::isDownbeat(int step) { return (step % 4 == 0); }


// ============================================================================
// DrumPatternGenerator Implementation
// ============================================================================

namespace {

static inline bool stepInMask(uint16_t mask, int step) {
    return (mask & (1u << (15 - step))) != 0;
}

static inline uint8_t clampVelocity(int value) {
    if (value < 1) return 1;
    if (value > 127) return 127;
    return static_cast<uint8_t>(value);
}

static inline uint8_t baseToGenreRange(uint8_t base, const GenerativeParams& params) {
    const int minV = std::max(1, std::min(127, params.velocityMin));
    const int maxV = std::max(minV, std::min(127, params.velocityMax));
    const int span = maxV - minV;
    return static_cast<uint8_t>(minV + ((int)base * span) / 127);
}

static inline int8_t randomTimingOffset(float microTimingAmount) {
    if (microTimingAmount <= 0.01f) return 0;
    const int range = std::max(1, (int)std::round(microTimingAmount * 7.0f));
    return static_cast<int8_t>((rand() % (range * 2 + 1)) - range);
}

} // namespace

void DrumPatternGenerator::generateDrumPattern(DrumPatternSet& patternSet,
                                               const GenerativeParams& params,
                                               GenerativeMode mode,
                                               const DrumGenreTemplate* templateOverride) {
    for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
        for (int s = 0; s < DrumPattern::kSteps; ++s) {
            patternSet.voices[v].steps[s] = DrumStep();
        }
    }

    int modeIdx = static_cast<int>(mode);
    if (modeIdx < 0 || modeIdx >= kGenerativeModeCount) {
        modeIdx = 0;
    }
    const DrumGenreTemplate& tmpl = templateOverride ? *templateOverride : kDrumTemplates[modeIdx];

    const int activeVoices = std::max(1, std::min(DrumPatternSet::kVoices, params.drumVoiceCount));
    const uint8_t kickMainVel = baseToGenreRange(tmpl.kickVelBase, params);
    const uint8_t snareMainVel = baseToGenreRange(tmpl.snareVelBase, params);
    const uint8_t hatMainVel = baseToGenreRange(tmpl.hatVelBase, params);

    auto canUseVoice = [activeVoices](int voice) { return voice >= 0 && voice < activeVoices; };
    auto resolveVoice = [&](int preferred, int fallbackA, int fallbackB) {
        if (canUseVoice(preferred)) return preferred;
        if (canUseVoice(fallbackA)) return fallbackA;
        if (canUseVoice(fallbackB)) return fallbackB;
        if (canUseVoice(1)) return 1;
        if (canUseVoice(0)) return 0;
        return -1;
    };
    auto placeHit = [&](int voice, int step, bool accent, uint8_t velocity, uint8_t probability) {
        if (!canUseVoice(voice) || step < 0 || step >= DrumPattern::kSteps) return;
        DrumStep& st = patternSet.voices[voice].steps[step];
        st.hit = 1;
        st.accent = params.noAccents ? 0 : (accent ? 1 : 0);
        st.velocity = velocity;
        st.timing = randomTimingOffset(params.microTimingAmount);
        st.probability = probability;
    };

    // Voice map: 0 kick, 1 snare, 2 closed hat, 3 open hat, 6 rim, 7 clap.
    const int preferredMainSnare = tmpl.useClap ? 7 : (tmpl.useRim ? 6 : 1);
    const int preferredGhostSnare = tmpl.useClap ? 1 : (tmpl.useRim ? 1 : preferredMainSnare);
    const int mainSnareVoice = resolveVoice(preferredMainSnare, 1, 7);
    const int ghostSnareVoice = resolveVoice(preferredGhostSnare, mainSnareVoice, 6);

    for (int step = 0; step < DrumPattern::kSteps; ++step) {
        if (stepInMask(tmpl.kickMask, step)) {
            if (!params.sparseKick || (rand() % 100) < 90) {
                const bool accent = (step % 4 == 0);
                const uint8_t vel = clampVelocity((int)kickMainVel + (accent ? 8 : 0));
                placeHit(0, step, accent, vel, 100);
            }
            continue;
        }

        float ghostChance = tmpl.kickGhostProb;
        if (params.drumSyncopation > 0.01f) ghostChance *= (0.6f + params.drumSyncopation);
        if (params.sparseKick) ghostChance *= 0.45f;
        if ((rand() % 1000) < (int)(ghostChance * 1000.0f)) {
            if ((step % 2) == 1 || params.drumPreferOffbeat) {
                placeHit(0, step, false, clampVelocity((int)kickMainVel - 24), 55);
            }
        }
    }

    for (int step = 0; step < DrumPattern::kSteps; ++step) {
        if (stepInMask(tmpl.snareMask, step)) {
            placeHit(mainSnareVoice, step, true, clampVelocity((int)snareMainVel + 6), 100);
            continue;
        }
        if ((rand() % 1000) < (int)(tmpl.snareGhostProb * 1000.0f)) {
            if ((step % 2) == 1 || params.drumPreferOffbeat) {
                placeHit(ghostSnareVoice, step, false, clampVelocity((int)snareMainVel - 30), 45);
            }
        }
    }

    for (int step = 0; step < DrumPattern::kSteps; ++step) {
        const bool inOpen = stepInMask(tmpl.openHatMask, step);
        const bool inClosed = stepInMask(tmpl.hatMask, step);
        if (inOpen) {
            if (!params.sparseHats || (rand() % 100) < 80) {
                placeHit(3, step, true, clampVelocity((int)hatMainVel + 10), 90);
            }
            continue;
        }
        if (!inClosed) continue;
        if (params.sparseHats && (rand() % 100) < 40) continue;

        int vel = hatMainVel;
        if (tmpl.hatVariation > 0.01f) {
            const int spread = (int)std::round(18.0f * tmpl.hatVariation);
            vel += (rand() % (spread * 2 + 1)) - spread;
        }
        if (params.drumPreferOffbeat && (step % 2) == 0 && (rand() % 100) < 30) continue;
        placeHit(2, step, false, clampVelocity(vel), 100);
    }

    if ((rand() % 1000) < (int)(params.fillProbability * 600.0f)) {
        const int fillStart = 12 + (rand() % 2);
        for (int step = fillStart; step < DrumPattern::kSteps; ++step) {
            if ((rand() % 100) < 55) {
                int voice = 4 + (rand() % 2); // Mid/high tom.
                if (!canUseVoice(voice)) voice = resolveVoice(mainSnareVoice, 1, 0);
                placeHit(voice, step, false, clampVelocity((int)snareMainVel - 8 + (step - fillStart) * 6), 85);
            }
        }
    }

    if (params.swingAmount > 0.01f) {
        applyDrumSwing(patternSet, params.swingAmount);
    }
}

void DrumPatternGenerator::applyDrumSwing(DrumPatternSet& ps, float amount) {
    int swingTicks = (int)(amount * 48.0f);
    for (int step = 1; step < DrumPattern::kSteps; step += 2) {
        for (int v = 0; v < DrumPatternSet::kVoices; v++) {
            if (ps.voices[v].steps[step].hit) {
                ps.voices[v].steps[step].timing = (int8_t)std::min(127, (int)ps.voices[v].steps[step].timing + swingTicks);
            }
        }
    }
}
