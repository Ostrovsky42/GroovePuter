#pragma once

#include <stdint.h>

#include "mini_dsp_params.h"
#include "tube_distortion.h"

enum class DrumParamId : uint8_t {
  MainVolume = 0,
  Count
};

enum DrumVoiceType : uint8_t {
  KICK = 0,
  SNARE,
  CLOSED_HAT,
  OPEN_HAT,
  MID_TOM,
  HIGH_TOM,
  RIM,
  CLAP,
  CYMBAL,
  VOICE_COUNT
};

class LoFiDrumFX {
public:
  LoFiDrumFX();
  float process(float input, DrumVoiceType voice);
  void setAmount(float amount);
  void setEnabled(bool enabled);

private:
  float bitcrush(float input, int bits);
  inline float fastTanh(float x);
  float vinyl();
  float drift();

  bool enabled_ = false;
  float amount_ = 0.0f;
  uint32_t noiseState_;
  float driftPhase_ = 0;
  
  struct OnePoleHP {
    float z1_ = 0;
    float process(float input, float cutoffHz, float sampleRate) {
      float alpha = cutoffHz / (sampleRate * 0.5f);
      float output = alpha * (input - z1_);
      z1_ = input;
      return output;
    }
  } hipass_;
};

class DrumSynthVoice {
public:
  virtual ~DrumSynthVoice() = default;

  virtual void reset() = 0;
  virtual void setSampleRate(float sampleRate) = 0;
  virtual void triggerKick(bool accent = false, uint8_t velocity = 100) = 0;
  virtual void triggerSnare(bool accent = false, uint8_t velocity = 100) = 0;
  virtual void triggerHat(bool accent = false, uint8_t velocity = 100) = 0;
  virtual void triggerOpenHat(bool accent = false, uint8_t velocity = 100) = 0;
  virtual void triggerMidTom(bool accent = false, uint8_t velocity = 100) = 0;
  virtual void triggerHighTom(bool accent = false, uint8_t velocity = 100) = 0;
  virtual void triggerRim(bool accent = false, uint8_t velocity = 100) = 0;
  virtual void triggerClap(bool accent = false, uint8_t velocity = 100) = 0;
  virtual void triggerCymbal(bool accent = false, uint8_t velocity = 100) = 0;

  virtual float processKick() = 0;
  virtual float processSnare() = 0;
  virtual float processHat() = 0;
  virtual float processOpenHat() = 0;
  virtual float processMidTom() = 0;
  virtual float processHighTom() = 0;
  virtual float processRim() = 0;
  virtual float processClap() = 0;
  virtual float processCymbal() = 0;

  virtual const Parameter& parameter(DrumParamId id) const = 0;
  virtual void setParameter(DrumParamId id, float value) = 0;

  virtual void setLoFiMode(bool enabled) = 0;
  virtual void setLoFiAmount(float amount) = 0;
};

class TR808DrumSynthVoice : public DrumSynthVoice {
public:
  explicit TR808DrumSynthVoice(float sampleRate);

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void triggerKick(bool accent = false, uint8_t velocity = 100) override;
  void triggerSnare(bool accent = false, uint8_t velocity = 100) override;
  void triggerHat(bool accent = false, uint8_t velocity = 100) override;
  void triggerOpenHat(bool accent = false, uint8_t velocity = 100) override;
  void triggerMidTom(bool accent = false, uint8_t velocity = 100) override;
  void triggerHighTom(bool accent = false, uint8_t velocity = 100) override;
  void triggerRim(bool accent = false, uint8_t velocity = 100) override;
  void triggerClap(bool accent = false, uint8_t velocity = 100) override;
  void triggerCymbal(bool accent = false, uint8_t velocity = 100) override;

  float processKick() override;
  float processSnare() override;
  float processHat() override;
  float processOpenHat() override;
  float processMidTom() override;
  float processHighTom() override;
  float processRim() override;
  float processClap() override;
  float processCymbal() override;

  const Parameter& parameter(DrumParamId id) const override;
  void setParameter(DrumParamId id, float value) override;

  void setLoFiMode(bool enabled) override { lofiEnabled = enabled; }
  void setLoFiAmount(float amount) override { lofi.setAmount(amount); }

private:
  bool lofiEnabled = false;
  LoFiDrumFX lofi;

private:
  struct Biquad {
    float a0;
    float a1;
    float a2;
    float b1;
    float b2;
    float z1;
    float z2;

    float process(float input) {
      float output = a0 * input + z1;
      z1 = a1 * input - b1 * output + z2;
      z2 = a2 * input - b2 * output;
      return output;
    }

    void reset() {
      z1 = 0.0f;
      z2 = 0.0f;
    }
  };

  float frand();
  float applyAccentDistortion(float input, bool accent);
  void updateClapFilters(float accentAmount);

  float kickPhase;
  float kickFreq;
  float kickEnvAmp;
  float kickEnvPitch;
  bool kickActive;
  float kickAccentGain;
  bool kickAccentDistortion;
  float kickAmpDecay;
  float kickBaseFreq;
  float kickSubPhase;
  float kickSubDecay;
  float kickClickAmp;
  uint32_t noiseState;

  float snareEnvAmp;
  float snareToneEnv;
  bool snareActive;
  float snareBp;
  float snareLp;
  float snareTonePhase;
  float snareTonePhase2;
  float snareAccentGain;
  float snareToneGain;
  bool snareAccentDistortion;

  float hatEnvAmp;
  float hatToneEnv;
  bool hatActive;
  float hatHp;
  float hatPrev;
  float hatPhaseA;
  float hatPhaseB;
  float hatAccentGain;
  float hatBrightness;
  bool hatAccentDistortion;

  float openHatEnvAmp;
  float openHatToneEnv;
  bool openHatActive;
  float openHatHp;
  float openHatPrev;
  float openHatPhaseA;
  float openHatPhaseB;
  float openHatAccentGain;
  float openHatBrightness;
  bool openHatAccentDistortion;

  float midTomPhase;
  float midTomEnv;
  bool midTomActive;
  float midTomAccentGain;
  bool midTomAccentDistortion;

  float highTomPhase;
  float highTomEnv;
  bool highTomActive;
  float highTomAccentGain;
  bool highTomAccentDistortion;

  float rimPhase;
  float rimEnv;
  bool rimActive;
  float rimAccentGain;
  bool rimAccentDistortion;

  float clapEnv;
  float clapTrans;
  float clapNoise;
  bool clapActive;
  float clapDelay;
  float clapTime;
  
  // Optimized Clap State (replaces expensive expf)
  float clapEnv1;
  float clapEnv2;
  float clapEnv3;
  int clapWait2; // samples until 2nd burst
  int clapWait3; // samples until 3rd burst
  float clapDecayCoef;

  float clapAccentAmount;
  float clapAccentGain;
  bool clapAccentDistortion;
  Biquad clapBandpass;
  Biquad clapLowpass;

  float cymbalEnv;
  float cymbalToneEnv;
  bool cymbalActive;
  float cymbalHp;
  float cymbalPrev;
  float cymbalPhaseA;
  float cymbalPhaseB;
  float cymbalAccentGain;
  float cymbalBrightness;
  bool cymbalAccentDistortion;

  float sampleRate;
  float invSampleRate;

  TubeDistortion accentDistortion;

  Parameter params[static_cast<int>(DrumParamId::Count)];
};

class TR909DrumSynthVoice : public DrumSynthVoice {
public:
  explicit TR909DrumSynthVoice(float sampleRate);

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void triggerKick(bool accent = false, uint8_t velocity = 100) override;
  void triggerSnare(bool accent = false, uint8_t velocity = 100) override;
  void triggerHat(bool accent = false, uint8_t velocity = 100) override;
  void triggerOpenHat(bool accent = false, uint8_t velocity = 100) override;
  void triggerMidTom(bool accent = false, uint8_t velocity = 100) override;
  void triggerHighTom(bool accent = false, uint8_t velocity = 100) override;
  void triggerRim(bool accent = false, uint8_t velocity = 100) override;
  void triggerClap(bool accent = false, uint8_t velocity = 100) override;
  void triggerCymbal(bool accent = false, uint8_t velocity = 100) override;

  float processKick() override;
  float processSnare() override;
  float processHat() override;
  float processOpenHat() override;
  float processMidTom() override;
  float processHighTom() override;
  float processRim() override;
  float processClap() override;
  float processCymbal() override;

  const Parameter& parameter(DrumParamId id) const override;
  void setParameter(DrumParamId id, float value) override;

  void setLoFiMode(bool enabled) override { lofiEnabled = enabled; }
  void setLoFiAmount(float amount) override { lofi.setAmount(amount); }

private:
  bool lofiEnabled = false;
  LoFiDrumFX lofi;

private:
  struct Biquad {
    float a0;
    float a1;
    float a2;
    float b1;
    float b2;
    float z1;
    float z2;

    float process(float input) {
      float output = a0 * input + z1;
      z1 = a1 * input - b1 * output + z2;
      z2 = a2 * input - b2 * output;
      return output;
    }

    void reset() {
      z1 = 0.0f;
      z2 = 0.0f;
    }
  };

  float frand();
  float applyAccentDistortion(float input, bool accent);
  void updateClapFilter();

  uint32_t noiseState_ = 54321;

  float kickPhase;
  float kickFreq;
  float kickEnvAmp;
  float kickEnvPitch;
  bool kickActive;
  float kickAccentGain;
  bool kickAccentDistortion;
  float kickAmpDecay;
  float kickBaseFreq;
  float kickClickEnv;

  float snareEnvAmp;
  float snareToneEnv;
  bool snareActive;
  float snareBp;
  float snareLp;
  float snareTonePhase;
  float snareTonePhase2;
  float snareAccentGain;
  float snareToneGain;
  bool snareAccentDistortion;
  float snareNoiseColor;

  float hatEnvAmp;
  float hatToneEnv;
  bool hatActive;
  float hatHp;
  float hatPrev;
  float hatPhaseA;
  float hatPhaseB;
  float hatAccentGain;
  float hatBrightness;
  bool hatAccentDistortion;

  float openHatEnvAmp;
  float openHatToneEnv;
  bool openHatActive;
  float openHatHp;
  float openHatPrev;
  float openHatPhaseA;
  float openHatPhaseB;
  float openHatAccentGain;
  float openHatBrightness;
  bool openHatAccentDistortion;

  float midTomPhase;
  float midTomEnv;
  bool midTomActive;
  float midTomAccentGain;
  bool midTomAccentDistortion;

  float highTomPhase;
  float highTomEnv;
  bool highTomActive;
  float highTomAccentGain;
  bool highTomAccentDistortion;

  float rimPhase;
  float rimEnv;
  bool rimActive;
  float rimAccentGain;
  bool rimAccentDistortion;

  float clapEnv;
  float clapTrans;
  float clapNoise;
  bool clapActive;
  float clapDelay;
  float clapTime;
  float clapTailEnv;
  float clapTailDecay;
  float clapAccentGain;
  bool clapAccentDistortion;
  Biquad clapBandpass;

  float cymbalEnv;
  float cymbalToneEnv;
  bool cymbalActive;
  float cymbalHp;
  float cymbalPrev;
  float cymbalPhaseA;
  float cymbalPhaseB;
  float cymbalAccentGain;
  float cymbalBrightness;
  bool cymbalAccentDistortion;

  float sampleRate;
  float invSampleRate;

  TubeDistortion accentDistortion;

  Parameter params[static_cast<int>(DrumParamId::Count)];
};

class TR606DrumSynthVoice : public DrumSynthVoice {
public:
  explicit TR606DrumSynthVoice(float sampleRate);

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void triggerKick(bool accent = false, uint8_t velocity = 100) override;
  void triggerSnare(bool accent = false, uint8_t velocity = 100) override;
  void triggerHat(bool accent = false, uint8_t velocity = 100) override;
  void triggerOpenHat(bool accent = false, uint8_t velocity = 100) override;
  void triggerMidTom(bool accent = false, uint8_t velocity = 100) override;
  void triggerHighTom(bool accent = false, uint8_t velocity = 100) override;
  void triggerRim(bool accent = false, uint8_t velocity = 100) override;
  void triggerClap(bool accent = false, uint8_t velocity = 100) override;
  void triggerCymbal(bool accent = false, uint8_t velocity = 100) override;

  float processKick() override;
  float processSnare() override;
  float processHat() override;
  float processOpenHat() override;
  float processMidTom() override;
  float processHighTom() override;
  float processRim() override;
  float processClap() override;
  float processCymbal() override;

  const Parameter& parameter(DrumParamId id) const override;
  void setParameter(DrumParamId id, float value) override;

  void setLoFiMode(bool enabled) override { lofiEnabled = enabled; }
  void setLoFiAmount(float amount) override { lofi.setAmount(amount); }

private:
  bool lofiEnabled = false;
  LoFiDrumFX lofi;

private:
  struct OnePole {
    float z;
    float a;
    float process(float input) { return z += a * (input - z); }
    void reset() { z = 0.0f; }
  };

  struct Biquad {
    float a0;
    float a1;
    float a2;
    float b1;
    float b2;
    float z1;
    float z2;

    float process(float input) {
      float output = a0 * input + z1;
      z1 = a1 * input - b1 * output + z2;
      z2 = a2 * input - b2 * output;
      return output;
    }

    void reset() {
      z1 = 0.0f;
      z2 = 0.0f;
    }
  };

  float frand();
  uint32_t noiseState_ = 98765;
  float decayCoeff(float timeSeconds) const;
  float onePoleCoeff(float cutoffHz) const;
  float square(float phase) const;
  void setAccent(bool accent);
  void updateMetalBank();
  void updateHatFilters(float accent);
  void updateCymbalFilter(float accent, Biquad& filter);

  float kickPhase;
  float kickAmpEnv;
  float kickFmEnv;
  bool kickActive;
  float kickAmpDecay;
  float kickFmDecay;

  float snareTonePhaseA;
  float snareTonePhaseB;
  float snareToneEnv;
  float snareNoiseEnv;
  bool snareActive;
  float snareToneDecay;
  float snareNoiseDecay;
  OnePole snareNoiseLp;
  float snareNoiseLpCoeff;

  float midTomPhase;
  float midTomAmpEnv;
  float midTomFmEnv;
  bool midTomActive;
  float midTomAmpDecay;
  float midTomFmDecay;

  float highTomPhase;
  float highTomAmpEnv;
  float highTomFmEnv;
  bool highTomActive;
  float highTomAmpDecay;
  float highTomFmDecay;

  float hatEnv;
  float openHatEnv;
  bool hatActive;
  bool openHatActive;
  float hatDecay;
  float openHatDecay;
  OnePole hatNoiseLp;
  OnePole hatMetalLp;
  float hatNoiseLpCoeff;
  float hatMetalLpCoeff;

  float cymbalEnv;
  bool cymbalActive;
  float cymbalDecay;
  Biquad cymbalBandpass;

  float accentEnv;
  float accentDecay;

  float sampleRate;
  float invSampleRate;

  float metalPhases[6];
  float metalSignal;
  const float metalFreqs[6] = {330.0f, 558.0f, 880.0f, 1320.0f, 1760.0f, 2640.0f};

  Parameter params[static_cast<int>(DrumParamId::Count)];
};

class CR78DrumSynthVoice : public DrumSynthVoice {
public:
  explicit CR78DrumSynthVoice(float sampleRate);

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void triggerKick(bool accent, uint8_t velocity) override;
  void triggerSnare(bool accent, uint8_t velocity) override;
  void triggerHat(bool accent, uint8_t velocity) override;
  void triggerOpenHat(bool accent, uint8_t velocity) override;
  void triggerMidTom(bool accent, uint8_t velocity) override;
  void triggerHighTom(bool accent, uint8_t velocity) override;
  void triggerRim(bool accent, uint8_t velocity) override;
  void triggerClap(bool accent, uint8_t velocity) override;
  void triggerCymbal(bool accent, uint8_t velocity) override;

  float processKick() override;
  float processSnare() override;
  float processHat() override;
  float processOpenHat() override;
  float processMidTom() override;
  float processHighTom() override;
  float processRim() override;
  float processClap() override;
  float processCymbal() override;

  const Parameter& parameter(DrumParamId id) const override;
  void setParameter(DrumParamId id, float value) override;

  void setLoFiMode(bool enabled) override { lofiEnabled = enabled; }
  void setLoFiAmount(float amount) override { lofi.setAmount(amount); }

private:
  bool lofiEnabled = false;
  LoFiDrumFX lofi;
  float sampleRate;
  uint32_t noiseState = 12345;

  // CR-78 specific state
  float kickEnv, kickPhase;
  float snareEnv, snareNoiseEnv;
  float hatEnv, hatMetalPhase[4];
  float tomEnv[2], tomPhase[2]; // Low/High
  float rimEnv, rimPhase;
  float clapEnv; // CR-78 doesn't really have a clap, mapping to Guiro or similar? KPR-77 has the clap.
                 // Actually CR-78 usually has "Tambourine" or "Guiro". We will synthesize a simple distinctive CR-78 clicky noise/guiro for Clap slot.
  float cymbalEnv, cymbalPhase;
  
  float decayCoef(float ms);
  float lcgFrand();
  
  Parameter params[static_cast<int>(DrumParamId::Count)];
};

class KPR77DrumSynthVoice : public DrumSynthVoice {
public:
  explicit KPR77DrumSynthVoice(float sampleRate);

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void triggerKick(bool accent, uint8_t velocity) override;
  void triggerSnare(bool accent, uint8_t velocity) override;
  void triggerHat(bool accent, uint8_t velocity) override;
  void triggerOpenHat(bool accent, uint8_t velocity) override;
  void triggerMidTom(bool accent, uint8_t velocity) override;
  void triggerHighTom(bool accent, uint8_t velocity) override;
  void triggerRim(bool accent, uint8_t velocity) override;
  void triggerClap(bool accent, uint8_t velocity) override;
  void triggerCymbal(bool accent, uint8_t velocity) override;

  float processKick() override;
  float processSnare() override;
  float processHat() override;
  float processOpenHat() override;
  float processMidTom() override;
  float processHighTom() override;
  float processRim() override;
  float processClap() override;
  float processCymbal() override;

  const Parameter& parameter(DrumParamId id) const override;
  void setParameter(DrumParamId id, float value) override;

  void setLoFiMode(bool enabled) override { lofiEnabled = enabled; }
  void setLoFiAmount(float amount) override { lofi.setAmount(amount); }

private:
  bool lofiEnabled = false;
  LoFiDrumFX lofi;
  float sampleRate;
  uint32_t noiseState = 67890;

  float kickEnv, kickPhase;
  float snareEnva, snareEnvb; // Body/Noise
  float hatEnv;
  float tomEnv[2], tomPhase[2];
  float clapEnv, clapPulseTimer; // KPR-77 Clap is distinctive
  int clapState; 
  float cymbalEnv;

  // Simple one-pole definitions if needed, or inline
  float decayCoef(float ms);
  float lcgFrand();

  Parameter params[static_cast<int>(DrumParamId::Count)];
};

class SP12DrumSynthVoice : public DrumSynthVoice {
public:
  explicit SP12DrumSynthVoice(float sampleRate);

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void triggerKick(bool accent, uint8_t velocity) override;
  void triggerSnare(bool accent, uint8_t velocity) override;
  void triggerHat(bool accent, uint8_t velocity) override;
  void triggerOpenHat(bool accent, uint8_t velocity) override;
  void triggerMidTom(bool accent, uint8_t velocity) override;
  void triggerHighTom(bool accent, uint8_t velocity) override;
  void triggerRim(bool accent, uint8_t velocity) override;
  void triggerClap(bool accent, uint8_t velocity) override;
  void triggerCymbal(bool accent, uint8_t velocity) override;

  float processKick() override;
  float processSnare() override;
  float processHat() override;
  float processOpenHat() override;
  float processMidTom() override;
  float processHighTom() override;
  float processRim() override;
  float processClap() override;
  float processCymbal() override;

  const Parameter& parameter(DrumParamId id) const override;
  void setParameter(DrumParamId id, float value) override;

  void setLoFiMode(bool enabled) override { lofiEnabled = enabled; }
  void setLoFiAmount(float amount) override { lofi.setAmount(amount); }

private:
  bool lofiEnabled = false;
  LoFiDrumFX lofi;
  float sampleRate;

  // PCM State
  struct VG {
    const int8_t* curData;
    int curLen;
    int curPos = -1; // -1 = inactive
    float phase = 0.0f;
    float increment = 1.0f;
    float volume = 1.0f; 
    float reconLP = 0.0f;
  };
  VG voices[9]; // Map to DrumVoiceType

  // ROM data storage (static would be better, but member for now to avoid global pollution if desired, or static members)
  // We'll declare buildROM in cpp
  static void buildROM(); 
  static bool romBuilt;

  float processPCM(int voiceIdx);
  float quantize12(float sample); // SP-12 12-bit DAC emulation

  Parameter params[static_cast<int>(DrumParamId::Count)];
};
