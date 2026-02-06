Tape v1: "Instrument-first Tape" Implementation Plan
Transform GroovePuter's existing tape functionality into a performance instrument with 5 macro controls, 4 modes, 6 presets, and 3 performance actions—all with a cassette-aesthetic UI.

User Review Required
IMPORTANT

RAM constraint: The existing TapeLooper uses up to 344KB (8s @ 22kHz mono 16-bit).
On Cardputer (ESP32-S3 with 2–8MB PSRAM), this should be fine, but we'll keep the 8s limit.

NOTE

DSP chain order: Dry Mix → TapeLooper (record/play) → TapeFX (coloring) → Output.
This allows TapeFX to be tweaked live without re-recording.

Proposed Changes
Core Data Structures
[MODIFY] 

scenes.h
Expand 

TapeState
 (lines 144–149) to support full v1 spec:

// NEW: Tape mode enum
enum class TapeMode : uint8_t { 
    Stop = 0, 
    Rec = 1, 
    Dub = 2, 
    Play = 3 
};
// NEW: Tape preset enum
enum class TapePreset : uint8_t {
    Clean = 0,
    Warm = 1,
    Dust = 2,
    VHS = 3,
    Broken = 4,
    AcidBath = 5,
    Count = 6
};
// NEW: Macro parameters (0–100 range for UI, mapped to DSP)
struct TapeMacro {
    uint8_t wow = 12;      // 0..100
    uint8_t age = 20;      // 0..100
    uint8_t sat = 35;      // 0..100
    uint8_t tone = 60;     // 0..100 (0=dark, 100=bright)
    uint8_t crush = 0;     // 0=off, 1=8bit, 2=6bit, 3=4bit
};
// REPLACE existing TapeState
struct TapeState {
    // Mode & control
    TapeMode mode = TapeMode::Stop;
    TapePreset preset = TapePreset::Warm;
    uint8_t speed = 1;       // 0=0.5x, 1=1.0x, 2=2.0x
    bool fxEnabled = true;   // Master on/off
    
    // Macro controls
    TapeMacro macro;
    
    // Looper volume
    float looperVolume = 1.0f;
};
DSP Processing
[MODIFY] 

tape_fx.h
Expand TapeFX to support all 5 macros with DSP mapping:

class TapeFX {
public:
    TapeFX();
    // Macro setters (0..100 mapped internally)
    void setWow(uint8_t value);      // → depth 0.0–0.012, flutter ratio
    void setAge(uint8_t value);      // → noise 0–0.08, LPF rolloff
    void setSaturation(uint8_t value); // → drive 1.0–8.0
    void setTone(uint8_t value);     // → LPF cutoff 1.2k–9k Hz
    void setCrush(uint8_t level);    // 0=off, 1=8bit, 2=6bit, 3=4bit
    
    // Apply all macros at once
    void applyMacro(const TapeMacro& macro);
    
    float process(float input);
private:
    // Delay line for wow/flutter
    static constexpr uint32_t kDelaySize = 1024;
    float buffer_[kDelaySize];
    uint32_t writePos_ = 0;
    // LFO state
    float wowSin_ = 0, wowCos_ = 1.0f;
    float wowStepSin_, wowStepCos_;
    float flutterSin_ = 0, flutterCos_ = 1.0f;
    float flutterStepSin_, flutterStepCos_;
    // DSP parameters (mapped from macros)
    float wowDepth_ = 0;        // 0..0.012
    float flutterRatio_ = 0;    // 0..1
    float noiseAmount_ = 0;     // 0..0.08
    float drive_ = 1.0f;        // 1..8
    float lpfCutoff_ = 0.9f;    // normalized 0..1
    uint8_t crushBits_ = 16;    // 16/8/6/4
    uint8_t crushDownsample_ = 1; // 1/2/4/6
    // LPF state (one-pole)
    float lpfZ1_ = 0;
    
    // Crush state
    uint8_t crushCounter_ = 0;
    float crushHold_ = 0;
    
    void updateLFO();
    inline float fastTanh(float x);
};
Macro → DSP Mapping (in 

tape_fx.cpp
):

Macro	DSP Parameter	Formula
WOW (0–100)	wowDepth	value * 0.00012f (0–0.012)
WOW (50–100)	flutterRatio	max(0, (value - 50) * 0.02f) (0–1)
AGE (0–100)	noiseAmount	value * 0.0008f (0–0.08)
SAT (0–100)	drive	1.0f + value * 0.07f (1–8)
TONE (0–100)	lpfCutoff	0.15f + value * 0.0075f (0.15–0.9 normalized)
CRUSH	bits/downsample	0→16/1, 1→8/2, 2→6/4, 3→4/6
[MODIFY] 

tape_looper.h
Add mode machine, speed, stutter:

class TapeLooper {
public:
    static constexpr uint32_t kMaxSeconds = 8;
    static constexpr uint32_t kMaxSamples = kMaxSeconds * kSampleRate;
    static constexpr uint32_t kStutterFrames = 512; // ~23ms @ 22kHz
    TapeLooper();
    ~TapeLooper();
    bool init(uint32_t maxSeconds);
    void clear();
    // Mode control
    void setMode(TapeMode mode);
    TapeMode mode() const { return mode_; }
    
    // Speed control
    void setSpeed(uint8_t speed); // 0=0.5x, 1=1.0x, 2=2.0x
    uint8_t speed() const { return speed_; }
    
    // Stutter (hold to activate)
    void setStutter(bool active);
    bool stutterActive() const { return stutterActive_; }
    
    // Eject (reset to clean state)
    void eject();
    // Getters for UI
    float playheadProgress() const; // 0.0–1.0
    float loopLengthSeconds() const;
    bool hasLoop() const { return length_ > 0; }
    void setVolume(float v) { volume_ = v; }
    void process(float input, float* loopPart);
private:
    int16_t* buffer_ = nullptr;
    uint32_t maxSamples_ = 0;
    uint32_t length_ = 0;
    float playhead_ = 0;  // float for interpolated speed
    
    TapeMode mode_ = TapeMode::Stop;
    uint8_t speed_ = 1;
    float speedMultiplier_ = 1.0f;
    
    bool stutterActive_ = false;
    uint32_t stutterStart_ = 0;
    
    float volume_ = 1.0f;
    bool firstRecord_ = false;
    float readInterpolated(float pos);
};
UI Layer
[MODIFY] 

tape_page.h
Complete redesign for v1 layout:

class TapePage : public IPage {
public:
    TapePage(IGfx& gfx, GroovePuter& mini_acid, AudioGuard& audio_guard,
             CassetteSkin* skin = nullptr);
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override;
    void setBoundaries(const Rect& rect) override;
private:
    enum class FocusItem { 
        Wow, Age, Sat, Tone, Crush, 
        Mode, Speed, Preset,
        Count 
    };
    void drawMacroSliders();
    void drawModeIndicator();
    void drawPresetName();
    void drawReelAnimation();
    void adjustParameter(int direction, bool fine);
    void cyclePreset();
    void cycleMode();
    void setSpeed(int speed);
    void toggleStutter(bool active);
    void eject();
    IGfx& gfx_;
    GroovePuter& mini_acid_;
    AudioGuard& audio_guard_;
    CassetteSkin* skin_;
    
    FocusItem focus_ = FocusItem::Wow;
    bool stutterHeld_ = false;
    
    std::string title_ = "TAPE";
};
UI Layout (240×135 display):

┌──────────────────────────────────────┐
│ TAPE     100BPM [●] P:WARM   PLAY   │ ← header
├──────────────────────────────────────┤
│                                      │
│  ┌──┐  ┌──┐         [▶ PLAY]        │ ← reels + mode
│  │▓▓│  │▓▓│         [1.0x]          │
│  └──┘  └──┘                         │
│  ▬▬▬●▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬             │ ← tape progress
│                                      │
│ WOW   ●─────────────────── 12       │ ← macro sliders
│ AGE   ───●─────────────── 20        │
│ SAT   ─────────●──────── 35         │
│ TONE  ─────────────●─── 60          │
│ CRUSH ──────────────── off          │
│                                      │
├──────────────────────────────────────┤
│ S1 S2 BD SD CH OH MT HT│ P R F 1 2 3│ ← mutes + hints
└──────────────────────────────────────┘
Hotkey Bindings (TapePage)
Key	Action	Notes
↑/↓	Select parameter	Cycle through WOW→AGE→SAT→TONE→CRUSH
←/→	Adjust value	±5 normal, ±1 with shift
P	Next preset	CLEAN→WARM→DUST→VHS→BROKEN→ACID_BATH
R	Cycle mode	STOP→REC→DUB→PLAY
F	Toggle FX	Master on/off
1/2/3	Set speed	0.5x / 1.0x / 2.0x
Enter	Stutter (hold)	Freeze playhead in small window
Del	Eject	Reset to clean state, fx off
Space	Play/Stop toggle	Standard transport
Preset Tables
[NEW] 

tape_presets.h
#pragma once
#include "../../scenes.h"
inline constexpr TapeMacro kTapePresets[6] = {
    // CLEAN:      subtle warmth
    { .wow = 5,  .age = 0,  .sat = 10, .tone = 80, .crush = 0 },
    // WARM:       classic tape
    { .wow = 12, .age = 20, .sat = 35, .tone = 60, .crush = 0 },
    // DUST:       aged cassette
    { .wow = 15, .age = 55, .sat = 30, .tone = 45, .crush = 0 },
    // VHS:        video tape vibe
    { .wow = 25, .age = 65, .sat = 40, .tone = 35, .crush = 1 },
    // BROKEN:     destroyed tape
    { .wow = 70, .age = 80, .sat = 55, .tone = 30, .crush = 2 },
    // ACID_BATH:  303 character
    { .wow = 35, .age = 35, .sat = 80, .tone = 55, .crush = 1 },
};
inline const char* tapePresetName(TapePreset preset) {
    static constexpr const char* names[] = {
        "CLEAN", "WARM", "DUST", "VHS", "BROKEN", "ACIDBTH"
    };
    return names[static_cast<int>(preset)];
}
Engine Integration
[MODIFY] 

grooveputer_engine.cpp
Update 

generateAudioBuffer
 to use TapeState:

void GroovePuter::generateAudioBuffer(int16_t *buffer, size_t numSamples) {
    // ... existing synth/drum rendering ...
+   // Apply current tape macro to TapeFX
+   tapeFX.applyMacro(sceneManager_.currentScene().tape.macro);
+   tapeLooper.setMode(sceneManager_.currentScene().tape.mode);
+   tapeLooper.setSpeed(sceneManager_.currentScene().tape.speed);
    for (size_t i = 0; i < numSamples; ++i) {
        // ... existing sample generation ...
        // Process through Looper
        float loopSample = 0.0f;
        tapeLooper.process(sample, &loopSample);
        sample += loopSample;
-       // Process through Tape FX  
-       sample = tapeFX.process(sample);
+       // Process through Tape FX (if enabled)
+       if (sceneManager_.currentScene().tape.fxEnabled) {
+           sample = tapeFX.process(sample);
+       }
        // ... rest of processing ...
    }
}
JSON Serialization
[MODIFY] 

scenes.h
 (writeSceneJson)
Update tape JSON serialization (around line 625):

-   if (!writeLiteral(",\"tape\":{\"wow\":")) return false;
-   if (!writeFloat(scene_->tape.wow)) return false;
-   if (!writeLiteral(",\"flt\":")) return false;
-   if (!writeFloat(scene_->tape.flutter)) return false;
-   if (!writeLiteral(",\"sat\":")) return false;
-   if (!writeFloat(scene_->tape.saturation)) return false;
-   if (!writeLiteral(",\"vol\":")) return false;
-   if (!writeFloat(scene_->tape.looperVolume)) return false;
+   if (!writeLiteral(",\"tape\":{\"mode\":")) return false;
+   if (!writeInt(static_cast<int>(scene_->tape.mode))) return false;
+   if (!writeLiteral(",\"preset\":")) return false;
+   if (!writeInt(static_cast<int>(scene_->tape.preset))) return false;
+   if (!writeLiteral(",\"speed\":")) return false;
+   if (!writeInt(scene_->tape.speed)) return false;
+   if (!writeLiteral(",\"fxEnabled\":")) return false;
+   if (!writeBool(scene_->tape.fxEnabled)) return false;
+   if (!writeLiteral(",\"wow\":")) return false;
+   if (!writeInt(scene_->tape.macro.wow)) return false;
+   if (!writeLiteral(",\"age\":")) return false;
+   if (!writeInt(scene_->tape.macro.age)) return false;
+   if (!writeLiteral(",\"sat\":")) return false;
+   if (!writeInt(scene_->tape.macro.sat)) return false;
+   if (!writeLiteral(",\"tone\":")) return false;
+   if (!writeInt(scene_->tape.macro.tone)) return false;
+   if (!writeLiteral(",\"crush\":")) return false;
+   if (!writeInt(scene_->tape.macro.crush)) return false;
+   if (!writeLiteral(",\"vol\":")) return false;
+   if (!writeFloat(scene_->tape.looperVolume)) return false;
Verification Plan
Automated Tests
Desktop SDL build:

cd platform_sdl && make clean && make -j4
Run with preset cycling test:

./grooveputer_sdl
# Navigate to TAPE page
# Press P to cycle presets, verify audio changes
Cardputer build:

cd .. && ./arduino-cli compile --fqbn esp32:esp32:m5stack_cardputer .
Manual Verification
 Verify all 5 macro sliders adjust sound audibly
 Verify mode cycling (STOP→REC→DUB→PLAY) works
 Verify REC records 8s of audio
 Verify DUB overdubs on existing loop
 Verify speed 0.5x/1x/2x affects pitch+time
 Verify stutter (Enter hold) creates tape-stuck effect
 Verify eject (Del) resets to clean state
 Verify presets load correct macro values
 Verify scene save/load persists tape state