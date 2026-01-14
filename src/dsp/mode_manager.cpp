#include "mode_manager.h"
#include "miniacid_engine.h"

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

void GrooveboxModeManager::generatePattern(SynthPattern& pattern) const {
    const ModeConfig& cfg = config();
    
    // Clear pattern
    for (int i = 0; i < SynthPattern::kSteps; ++i) {
        pattern.steps[i].note = -1;
        pattern.steps[i].accent = false;
        pattern.steps[i].slide = false;
    }
    
    int numNotes = cfg.pattern.minNotes + (rand() % (cfg.pattern.maxNotes - cfg.pattern.minNotes + 1));
    
    if (currentMode_ == GrooveboxMode::Acid) {
        // Acid: fill random positions
        for (int i = 0; i < numNotes; i++) {
            int step = rand() % 16;
            int note = cfg.pattern.minOctave + (rand() % (cfg.pattern.maxOctave - cfg.pattern.minOctave + 1));
            
            pattern.steps[step].note = note;
            if ((rand() % 100) < (cfg.pattern.slideProbability * 100)) pattern.steps[step].slide = true;
            if ((rand() % 100) < (cfg.pattern.accentProbability * 100)) pattern.steps[step].accent = true;
        }
    } else {
        // Minimal: structured positions
        const int minimalPositions[] = {0, 4, 8, 10, 12, 14};
        int posCount = sizeof(minimalPositions) / sizeof(int);
        for (int i = 0; i < numNotes && i < posCount; i++) {
            int step = minimalPositions[i];
            int note = cfg.pattern.minOctave + (rand() % (cfg.pattern.maxOctave - cfg.pattern.minOctave + 1));
            
            pattern.steps[step].note = note;
            // Downbeats accent
            if (step == 0 || step == 8) {
                if ((rand() % 100) < (cfg.pattern.accentProbability * 100)) pattern.steps[step].accent = true;
            }
            if ((rand() % 100) < (cfg.pattern.slideProbability * 100)) pattern.steps[step].slide = true;
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
        }
    }
    
    // Voices: 0=Kick, 1=Snare, 2=CH, 3=OH, 4=MT, 5=HT, 6=Rim, 7=Clap
    
    // KICK
    if (cfg.drums.sparseKick) {
        patternSet.voices[0].steps[0].hit = true;
        patternSet.voices[0].steps[8].hit = true;
    } else {
        for (int i = 0; i < 16; i += 4) patternSet.voices[0].steps[i].hit = true;
    }
    
    // SNARE
    patternSet.voices[1].steps[4].hit = true;
    patternSet.voices[1].steps[12].hit = true;
    
    // HATS
    if (cfg.drums.sparseHats) {
        for (int i = 0; i < 16; i += 4) {
            if ((rand() % 100) < 70) patternSet.voices[2].steps[i].hit = true;
        }
    } else {
        for (int i = 0; i < 16; i += 2) patternSet.voices[2].steps[i].hit = true;
    }
    
    // OPEN HAT
    int openPos = 6 + (rand() % 4) * 2;
    patternSet.voices[3].steps[openPos & 15].hit = true;
    
    // FILLS
    if ((rand() % 100) < (cfg.drums.fillProbability * 100)) {
        int fillPos = rand() % 16;
        int voice = 4 + (rand() % 4);
        patternSet.voices[voice].steps[fillPos].hit = true;
    }
    
    // ACCENTS
    if (!cfg.drums.noAccents) {
        for (int i = 0; i < 16; i++) {
            if ((rand() % 100) < 20) {
               // Accent random voice at this step
               int v = rand() % DrumPatternSet::kVoices;
               if (patternSet.voices[v].steps[i].hit) patternSet.voices[v].steps[i].accent = true;
            }
        }
    }
}
