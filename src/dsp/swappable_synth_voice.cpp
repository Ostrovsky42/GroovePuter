#include "swappable_synth_voice.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace {
inline float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

inline SynthEngineType parseEngineName(const std::string& name) {
  std::string upper = name;
  for (char& c : upper) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  if (upper.find("SID") != std::string::npos) return SynthEngineType::SID;
  return SynthEngineType::TB303;
}
} // namespace

SwappableSynthVoice::SwappableSynthVoice(float sampleRate, SynthEngineType initialType)
    : sampleRate_(sampleRate),
      type_(initialType),
      pendingType_(initialType) {
  if (sampleRate_ <= 0.0f) sampleRate_ = 44100.0f;

  engines_[engineIndex(SynthEngineType::TB303)] =
      createVoice(SynthEngineType::TB303, sampleRate_);
  engines_[engineIndex(SynthEngineType::SID)] =
      createVoice(SynthEngineType::SID, sampleRate_);

  current_ = engines_[engineIndex(type_)].get();
  for (auto& engine : engines_) {
    if (!engine) continue;
    engine->setMode(mode_);
    engine->setLoFiAmount(loFiAmount_);
  }
}

std::unique_ptr<IMonoSynthVoice> SwappableSynthVoice::createVoice(SynthEngineType type, float sampleRate) {
  if (sampleRate <= 0.0f) sampleRate = 44100.0f;
  switch (type) {
    case SynthEngineType::SID:
      return std::make_unique<SidSynthVoice>(sampleRate);
    case SynthEngineType::TB303:
    default:
      return std::make_unique<TB303Voice>(sampleRate);
  }
}

const char* SwappableSynthVoice::toEngineName(SynthEngineType type) {
  switch (type) {
    case SynthEngineType::SID: return "SID";
    case SynthEngineType::TB303:
    default: return "TB303";
  }
}

void SwappableSynthVoice::setEngineName(const std::string& name) {
  setEngineType(parseEngineName(name));
}

void SwappableSynthVoice::setEngineType(SynthEngineType type) {
  if (!current_) return;
  if (type == type_ && !switching_) return;

  pendingType_ = type;
  next_ = engines_[engineIndex(pendingType_)].get();
  if (!next_) return;

  // Deterministic target state for repeatable transitions.
  next_->reset();
  next_->setMode(mode_);
  next_->setLoFiAmount(loFiAmount_);

  if (noteHeld_ && lastFreqHz_ > 0.0f) {
    next_->startNote(lastFreqHz_, lastAccent_, lastSlide_, lastVelocity_);
  }

  const float xfadeMs = 10.0f;
  uint32_t samples = static_cast<uint32_t>(std::max(16.0f, sampleRate_ * xfadeMs * 0.001f));
  xfadeTotal_ = (samples == 0) ? 16 : samples;
  xfadePos_ = 0;
  switching_ = true;
}

SynthVoiceState SwappableSynthVoice::getState() const {
  SynthVoiceState state;
  state.engineType = switching_ ? pendingType_ : type_;
  const IMonoSynthVoice* voice = (switching_ && next_) ? next_ : current_;
  if (!voice) return state;

  const uint8_t count = std::min<uint8_t>(voice->parameterCount(),
                                          static_cast<uint8_t>(state.params.size()));
  state.paramCount = count;
  for (uint8_t i = 0; i < count; ++i) {
    state.params[i] = clamp01(voice->getParameterNormalized(i));
  }
  return state;
}

void SwappableSynthVoice::setState(const SynthVoiceState& state) {
  switching_ = false;
  xfadeTotal_ = 0;
  xfadePos_ = 0;
  next_ = nullptr;

  type_ = state.engineType;
  pendingType_ = state.engineType;
  current_ = engines_[engineIndex(type_)].get();
  if (!current_) return;

  noteHeld_ = false;
  lastFreqHz_ = 0.0f;
  lastAccent_ = false;
  lastSlide_ = false;
  lastVelocity_ = 100;

  current_->reset();
  current_->setMode(mode_);
  current_->setLoFiAmount(loFiAmount_);

  const uint8_t count = std::min<uint8_t>(state.paramCount, current_->parameterCount());
  for (uint8_t i = 0; i < count; ++i) {
    current_->setParameterNormalized(i, clamp01(state.params[i]));
  }
}

void SwappableSynthVoice::reset() {
  noteHeld_ = false;
  lastFreqHz_ = 0.0f;
  lastAccent_ = false;
  lastSlide_ = false;
  lastVelocity_ = 100;

  switching_ = false;
  xfadeTotal_ = 0;
  xfadePos_ = 0;
  next_ = nullptr;

  if (current_) current_->reset();
}

void SwappableSynthVoice::setSampleRate(float sampleRate) {
  if (sampleRate <= 0.0f) sampleRate = 44100.0f;
  sampleRate_ = sampleRate;
  for (auto& engine : engines_) {
    if (engine) engine->setSampleRate(sampleRate_);
  }
}

void SwappableSynthVoice::startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity) {
  if (freqHz <= 0.0f) return;

  noteHeld_ = true;
  lastFreqHz_ = freqHz;
  lastAccent_ = accent;
  lastSlide_ = slideFlag;
  lastVelocity_ = velocity;

  if (current_) current_->startNote(freqHz, accent, slideFlag, velocity);
  if (switching_ && next_) next_->startNote(freqHz, accent, slideFlag, velocity);
}

void SwappableSynthVoice::release() {
  noteHeld_ = false;
  if (current_) current_->release();
  if (switching_ && next_) next_->release();
}

float SwappableSynthVoice::process() {
  const float a = current_ ? current_->process() : 0.0f;
  if (!switching_ || !next_) return a;

  const float b = next_->process();
  const float t = (xfadeTotal_ > 0)
                      ? (static_cast<float>(xfadePos_) / static_cast<float>(xfadeTotal_))
                      : 1.0f;
  const float mix = clamp01(t);
  constexpr float kHalfPi = 1.57079632679f;
  const float gainA = std::cos((1.0f - mix) * kHalfPi);
  const float gainB = std::cos((mix) * kHalfPi);
  const float out = a * gainB + b * gainA;

  if (xfadePos_ < xfadeTotal_) ++xfadePos_;
  if (xfadePos_ >= xfadeTotal_) {
    current_ = next_;
    next_ = nullptr;
    switching_ = false;
    type_ = pendingType_;
    xfadeTotal_ = 0;
    xfadePos_ = 0;
  }

  return out;
}

uint8_t SwappableSynthVoice::parameterCount() const {
  const IMonoSynthVoice* voice = (switching_ && next_) ? next_ : current_;
  if (!voice) return 0;
  return voice->parameterCount();
}

void SwappableSynthVoice::setParameterNormalized(uint8_t index, float norm) {
  IMonoSynthVoice* voice = (switching_ && next_) ? next_ : current_;
  if (!voice) return;
  voice->setParameterNormalized(index, clamp01(norm));
}

float SwappableSynthVoice::getParameterNormalized(uint8_t index) const {
  const IMonoSynthVoice* voice = (switching_ && next_) ? next_ : current_;
  if (!voice) return 0.0f;
  return voice->getParameterNormalized(index);
}

const Parameter& SwappableSynthVoice::getParameter(uint8_t index) const {
  static Parameter dummy("dummy", "", 0.0f, 1.0f, 0.0f, 1.0f);
  const IMonoSynthVoice* voice = (switching_ && next_) ? next_ : current_;
  if (!voice) return dummy;
  return voice->getParameter(index);
}

const char* SwappableSynthVoice::getEngineName() const {
  return toEngineName(switching_ ? pendingType_ : type_);
}

void SwappableSynthVoice::setMode(GrooveboxMode mode) {
  mode_ = mode;
  for (auto& engine : engines_) {
    if (engine) engine->setMode(mode_);
  }
}

void SwappableSynthVoice::setLoFiAmount(float amount) {
  loFiAmount_ = amount;
  for (auto& engine : engines_) {
    if (engine) engine->setLoFiAmount(loFiAmount_);
  }
}
