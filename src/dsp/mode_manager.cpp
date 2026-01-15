#include "mode_manager.h"
#include "miniacid_engine.h"
#include <algorithm>

void GrooveboxModeManager::setMode(GrooveboxMode mode) {
    currentMode_ = mode;
    engine_.setGrooveboxMode(mode); // Sync to engine
}

void GrooveboxModeManager::toggle() {
    setMode((currentMode_ == GrooveboxMode::Acid) ? GrooveboxMode::Minimal : GrooveboxMode::Acid);
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
    int octave = note / 12;
    int pitch = note % 12;
    
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

void GrooveboxModeManager::generatePattern(SynthPattern& pattern) const {
    const ModeConfig& cfg = config();
    
    // Clear pattern
    for (int i = 0; i < SynthPattern::kSteps; ++i) {
        pattern.steps[i].note = -1; // -1 means no note
        pattern.steps[i].accent = false;
        pattern.steps[i].slide = false;
        pattern.steps[i].velocity = 100;
        pattern.steps[i].timing = 0;
        pattern.steps[i].ghost = false;
    }
    
    // Pick a scale for this pattern
    const Scale& scale = (currentMode_ == GrooveboxMode::Acid) ? kScales[0] : kScales[rand() % 4];
    int rootNote = (currentMode_ == GrooveboxMode::Acid) ? 36 : 24; // C2 or C1
    int strategy = rand() % 3; // 0: Random Walk, 1: Arp/Rhythm, 2: Sequential
    
    float swingAmount = (currentMode_ == GrooveboxMode::Acid) ? 0.0f : 0.20f;  // Subtle shuffle for minimal
    float ghostProb = (currentMode_ == GrooveboxMode::Acid) ? 0.15f : 0.45f;   // More ghosts in minimal

    if (currentMode_ == GrooveboxMode::Acid) {
        // ACID STRATEGY
        int currentScaleIdx = rand() % scale.count;
        for (int i = 0; i < 16; i++) {
            bool shouldHit = (rand() % 100) < (cfg.pattern.minNotes * 100 / 16 + 25);
            
            if (shouldHit) {
                // Determine next note
                if (strategy == 0) { // Random walk in scale
                    currentScaleIdx = (currentScaleIdx + (rand() % 3) - 1 + scale.count) % scale.count;
                } else if (strategy == 1) { // Arp-like (root/3rd/5th focus)
                    int choices[] = {0, 0, 2, 4}; // Root, Root, 3rd, 5th in pentatonic
                    currentScaleIdx = choices[rand() % 4];
                } else { // High energy random
                    currentScaleIdx = rand() % scale.count;
                }

                int note = rootNote + scale.intervals[currentScaleIdx];
                
                // Chromatic passing tones (Acid character)
                if ((rand() % 100) < (cfg.pattern.chromaticProbability * 100)) {
                    note += (rand() % 3) - 1;  // +1, 0, -1 semitone
                }
                
                // Octave jumps (Classic Acid)
                if (rand() % 100 < 30) note += 12;
                if (rand() % 100 < 10) note += 24;
                if (rand() % 100 < 5)  note -= 12;

                pattern.steps[i].note = note;
                pattern.steps[i].velocity = cfg.pattern.velocityMin + (rand() % (cfg.pattern.velocityMax - cfg.pattern.velocityMin));
                
                if ((rand() % 100) < (cfg.pattern.slideProbability * 100)) pattern.steps[i].slide = true;
                if ((rand() % 100) < (cfg.pattern.accentProbability * 100)) {
                    pattern.steps[i].accent = true;
                    pattern.steps[i].velocity = 127;
                    if (rand() % 100 < 40) pattern.steps[i].slide = true; // Accent + Slide = Acid fun
                }
            } else if ((rand() % 100) < (ghostProb * 100)) {
                // Ghost notes (low velocity)
                pattern.steps[i].note = rootNote;
                pattern.steps[i].ghost = true;
                pattern.steps[i].velocity = 25 + (rand() % 20);
                if (rand() % 100 < 20) pattern.steps[i].slide = true;
            }
        }
    } else {
        // MINIMAL STRATEGY: Hypnotic, bass-heavy
        int baseNote = 24 + (rand() % 12); // Transpose the whole pattern
        int lastNoteIdx = 0;
        
        for (int i = 0; i < 16; i++) {
            // Rhythmic anchor: 1, 4, 7, 10, 13 (typical minimal syncopation)
            bool isAnchor = (i % 4 == 0) || (i == 3) || (i == 6) || (i == 10);
            float hitProb = isAnchor ? 70.0f : 15.0f;
            
            if ((rand() % 100) < hitProb) {
                // Focus on root (hypnotic repetition)
                int scaleIdx;
                int rootBiasPercent = (int)(cfg.pattern.rootNoteBias * 100);
                if (rand() % 100 < rootBiasPercent) scaleIdx = 0; // Root note (80% in minimal)
                else if (rand() % 100 < 30) scaleIdx = (scale.count > 4) ? 4 : 2; // 30% fifth-ish
                else scaleIdx = rand() % scale.count; // 10% other
                
                int note = baseNote + scale.intervals[scaleIdx];
                
                // Deep octave drops
                if (rand() % 100 < 20) note -= 12;
                
                pattern.steps[i].note = note;
                pattern.steps[i].velocity = cfg.pattern.velocityMin + (rand() % (cfg.pattern.velocityMax - cfg.pattern.velocityMin));
                
                // Minimal accents are precise
                if (i % 8 == 0 && rand() % 100 < 60) {
                    pattern.steps[i].accent = true;
                    pattern.steps[i].velocity = 127;
                }
                
                if (rand() % 100 < 8) pattern.steps[i].slide = true;
            } else if ((rand() % 100) < (ghostProb * 100)) {
                // More active ghosts in minimal for texture
                pattern.steps[i].note = baseNote;
                pattern.steps[i].ghost = true;
                pattern.steps[i].velocity = 10 + (rand() % 15);  // Quieter ghosts
                // Microtiming for "shuffle"
                pattern.steps[i].timing = (int8_t)((rand() % 13) - 6); 
            }
        }
    }

    // Apply Swing
    if (swingAmount > 0.01f) {
        int8_t swingTicks = (int8_t)(swingAmount * 36.0f); // More pronounced swing
        for (int i = 0; i < 16; i++) {
            if (i % 2 == 1 && pattern.steps[i].note >= 0) {
                pattern.steps[i].timing += swingTicks;
            }
        }
    }
}

void GrooveboxModeManager::generateDrumPattern(DrumPatternSet& patternSet) const {
    const ModeConfig& cfg = config();
    
    // Clear
    for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
        for (int i = 0; i < DrumPattern::kSteps; ++i) {
            patternSet.voices[v].steps[i].hit = false;
            patternSet.voices[v].steps[i].accent = false;
            patternSet.voices[v].steps[i].velocity = 100;
            patternSet.voices[v].steps[i].timing = 0;
        }
    }
    
    float swingAmount = (currentMode_ == GrooveboxMode::Acid) ? 0.08f : 0.15f;  // Subtle swing for both
    bool humanize = true;

    // KICK
    if (cfg.drums.sparseKick) {
        patternSet.voices[0].steps[0].hit = true;
        patternSet.voices[0].steps[0].velocity = 120;
        patternSet.voices[0].steps[8].hit = true;
        patternSet.voices[0].steps[8].velocity = 110;
        if (rand() % 100 < 30) {
            patternSet.voices[0].steps[10].hit = true;
            patternSet.voices[0].steps[10].velocity = 100;
        }
    } else {
        for (int i = 0; i < 16; i += 4) {
            patternSet.voices[0].steps[i].hit = true;
            patternSet.voices[0].steps[i].velocity = 115 - (i / 4) * 3;
        }
        // Occasional double kick
        if (rand() % 100 < 40) {
            patternSet.voices[0].steps[14].hit = true;
            patternSet.voices[0].steps[14].velocity = 90;
        }
    }
    
    // SNARE
    patternSet.voices[1].steps[4].hit = true;
    patternSet.voices[1].steps[4].velocity = 115;
    patternSet.voices[1].steps[12].hit = true;
    patternSet.voices[1].steps[12].velocity = 110;

    // Ghost Snare (shuffle feel)
    if (rand() % 100 < 50) {
        int pos = (rand() % 100 < 50) ? 3 : 11;
        patternSet.voices[1].steps[pos].hit = true;
        patternSet.voices[1].steps[pos].velocity = 30 + (rand() % 20);
        patternSet.voices[1].steps[pos].timing = (int8_t)((rand() % 10) - 5);
    }
    
    // HATS
    if (cfg.drums.sparseHats) {
        for (int i = 0; i < 16; i += 4) {
            patternSet.voices[2].steps[(i + 2) % 16].hit = true; // Off-beats
            patternSet.voices[2].steps[(i + 2) % 16].velocity = 100 + (rand() % 20);
        }
    } else {
        for (int i = 0; i < 16; i++) {
            bool accentBeat = (i % 2 == 0);
            if (accentBeat || (rand() % 100 < 40)) {
                patternSet.voices[2].steps[i].hit = true;
                patternSet.voices[2].steps[i].velocity = accentBeat ? (100 + (rand() % 20)) : (40 + (rand() % 30));
            }
        }
    }
    
    // OPEN HAT (Classic off-beat)
    for (int i = 0; i < 16; i += 4) {
        if (rand() % 100 < (currentMode_ == GrooveboxMode::Acid ? 60 : 30)) {
            int pos = i + 2;
            patternSet.voices[3].steps[pos].hit = true;
            patternSet.voices[3].steps[pos].velocity = 95;
            // OH chokes CH (conventionally handled by engine, but good for pattern logic)
            patternSet.voices[2].steps[pos].hit = false;
        }
    }
    
    // FILLS / PERC
    if ((rand() % 100) < (cfg.drums.fillProbability * 100)) {
        int count = 1 + (rand() % 3);
        int voice = 4 + (rand() % 4); // MT, HT, Rim, Clap
        for (int i = 0; i < count; i++) {
            int pos = 12 + (rand() % 4);
            patternSet.voices[voice].steps[pos & 15].hit = true;
            patternSet.voices[voice].steps[pos & 15].velocity = 80 + (i * 10);
            patternSet.voices[voice].steps[pos & 15].accent = (rand() % 100 < 30);
        }
    }
    
    // Humanize & Swing
    for (int i = 0; i < 16; i++) {
        int8_t swingTicks = (i % 2 == 1) ? (int8_t)(swingAmount * 36.0f) : 0;
        for (int v = 0; v < DrumPatternSet::kVoices; v++) {
            if (patternSet.voices[v].steps[i].hit) {
                if (humanize) {
                    int var = (rand() % 16) - 8;
                    int newVel = (int)patternSet.voices[v].steps[i].velocity + var;
                    patternSet.voices[v].steps[i].velocity = (uint8_t)std::max(1, std::min(127, newVel));
                    
                    // Micro-timing humanization
                    patternSet.voices[v].steps[i].timing += (int8_t)((rand() % 5) - 2);
                }
                patternSet.voices[v].steps[i].timing += swingTicks;
            }
        }
    }

    // ACCENTS
    if (!cfg.drums.noAccents) {
        for (int i = 0; i < 16; i += 4) { // Focus accents on beats
            if (rand() % 100 < 40) {
               int v = rand() % 2; // Mostly Kick or Snare
               if (patternSet.voices[v].steps[i].hit) {
                   patternSet.voices[v].steps[i].accent = true;
                   patternSet.voices[v].steps[i].velocity = 127;
               }
            }
        }
    }
}
