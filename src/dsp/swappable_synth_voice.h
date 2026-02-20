#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "mono_synth_voice.h"
#include "mini_tb303.h"
#include "sid_synth_voice.h"
#include "ay_synth_voice.h"
#include "opl2_synth_voice.h"

enum class SynthEngineType : uint8_t {
  TB303 = 0,
  SID = 1,
  AY = 2,
  OPL2 = 3,
};

struct SynthVoiceState {
  SynthEngineType engineType = SynthEngineType::TB303;
  std::array<float, 16> params{};
  uint8_t paramCount = 0;
};

class SwappableSynthVoice : public IMonoSynthVoice {
public:
  explicit SwappableSynthVoice(float sampleRate, SynthEngineType initialType = SynthEngineType::TB303);
  ~SwappableSynthVoice() override = default;

  void setEngineType(SynthEngineType type);
  void setEngineName(const std::string& name);
  SynthEngineType engineType() const { return type_; }

  SynthVoiceState getState() const;
  void setState(const SynthVoiceState& state);

  IMonoSynthVoice* activeVoice() { return current_; }
  const IMonoSynthVoice* activeVoice() const { return current_; }

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) override;
  void release() override;
  float process() override;
  uint8_t parameterCount() const override;
  void setParameterNormalized(uint8_t index, float norm) override;
  float getParameterNormalized(uint8_t index) const override;
  const Parameter& getParameter(uint8_t index) const override;
  const char* getEngineName() const override;
  void setMode(GrooveboxMode mode) override;
  void setLoFiAmount(float amount) override;

private:
  static std::unique_ptr<IMonoSynthVoice> createVoice(SynthEngineType type, float sampleRate);
  static const char* toEngineName(SynthEngineType type);
  static int engineIndex(SynthEngineType type) {
    switch (type) {
      case SynthEngineType::SID: return 1;
      case SynthEngineType::AY: return 2;
      case SynthEngineType::OPL2: return 3;
      case SynthEngineType::TB303:
      default: return 0;
    }
  }

  float sampleRate_ = 44100.0f;
  SynthEngineType type_ = SynthEngineType::TB303;
  SynthEngineType pendingType_ = SynthEngineType::TB303;

  std::unique_ptr<IMonoSynthVoice> engines_[4];
  IMonoSynthVoice* current_ = nullptr;
  IMonoSynthVoice* next_ = nullptr;

  bool switching_ = false;
  uint32_t xfadeTotal_ = 0;
  uint32_t xfadePos_ = 0;

  bool noteHeld_ = false;
  float lastFreqHz_ = 0.0f;
  bool lastAccent_ = false;
  bool lastSlide_ = false;
  uint8_t lastVelocity_ = 100;

  GrooveboxMode mode_ = GrooveboxMode::Acid;
  float loFiAmount_ = 0.0f;
};
