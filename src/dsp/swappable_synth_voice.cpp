#include "swappable_synth_voice.h"
#include <cmath>
#include <ctype.h>

namespace {
inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

inline uint32_t nextRevision(uint32_t revision) {
    ++revision;
    if (revision == 0) ++revision;
    return revision;
}
} // namespace

SynthEngineType SwappableSynthVoice::parseEngineName(const std::string& name) {
  std::string upper = name;
  for (char& c : upper) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  if (upper.find("SID") != std::string::npos) return SynthEngineType::SID;
  if (upper.find("OPL2") != std::string::npos || upper.find("FM") != std::string::npos ||
      upper.find("YM3812") != std::string::npos) return SynthEngineType::OPL2;
  if (upper.find("AY") != std::string::npos || upper.find("YM2149") != std::string::npos ||
      upper.find("PSG") != std::string::npos) return SynthEngineType::AY;
  return SynthEngineType::TB303;
}

SwappableSynthVoice::SwappableSynthVoice(float sampleRate, SynthEngineType initialType)
    : sampleRate_(sampleRate), type_(initialType), pendingType_(initialType) {
    if (sampleRate_ <= 0.0f) sampleRate_ = 44100.0f;
    current_ = createVoice(type_, sampleRate_);
    if (current_) {
        current_->setMode(mode_);
        current_->setLoFiAmount(loFi_);
    }

    initializeControlStateFromVoice_();
    audioBoundaryRegistered_ =
        GroovePuterAudio::AudioControlBoundaryRegistry::instance()
            .registerClient(this,
                            &SwappableSynthVoice::applyPendingControlCallback_,
                            &SwappableSynthVoice::captureStructuralControlCallback_);
}

SwappableSynthVoice::~SwappableSynthVoice() {
    if (audioBoundaryRegistered_) {
        GroovePuterAudio::AudioControlBoundaryRegistry::instance()
            .unregisterClient(this);
    }
}

std::unique_ptr<IMonoSynthVoice> SwappableSynthVoice::createVoice(SynthEngineType type, float sampleRate) {
    switch (type) {
        case SynthEngineType::SID:   return std::make_unique<SidSynthVoice>(sampleRate);
        case SynthEngineType::AY:    return std::make_unique<AySynthVoice>(sampleRate);
        case SynthEngineType::OPL2:  return std::make_unique<Opl2SynthVoice>(sampleRate);
        case SynthEngineType::TB303:
        default:                     return std::make_unique<TB303Voice>(sampleRate);
    }
}

IMonoSynthVoice* SwappableSynthVoice::controlTargetVoice_() {
    return (switching_ && next_) ? next_.get() : current_.get();
}

const IMonoSynthVoice* SwappableSynthVoice::controlTargetVoice_() const {
    return (switching_ && next_) ? next_.get() : current_.get();
}

void SwappableSynthVoice::initializeControlStateFromVoice_() {
    controlShadow_ = SynthParameterControlSnapshot{};
    const IMonoSynthVoice* voice = controlTargetVoice_();
    if (voice) {
        const uint8_t count = std::min<uint8_t>(
            voice->parameterCount(), kMaxControlParams);
        controlShadow_.paramCount = count;
        for (uint8_t i = 0; i < count; ++i) {
            controlParameters_[i] = voice->getParameter(i);
            const float norm = clamp01(voice->getParameterNormalized(i));
            controlParameters_[i].setNormalized(norm);
            controlShadow_.normalized[i] = norm;
        }
    }
    controlShadow_.revision = 1;
    controlSnapshots_.initialize(controlShadow_);
    appliedControlRevision_.store(controlShadow_.revision,
                                  std::memory_order_release);
}

void SwappableSynthVoice::publishControlShadow_() {
    controlShadow_.revision = nextRevision(controlShadow_.revision);
    controlSnapshots_.publish(controlShadow_);
}

void SwappableSynthVoice::captureControlStateAfterStructuralMutation_() {
    const IMonoSynthVoice* voice = controlTargetVoice_();
    if (!voice) return;

    const uint8_t count = std::min<uint8_t>(
        voice->parameterCount(), kMaxControlParams);
    controlShadow_.paramCount = count;
    for (uint8_t i = 0; i < count; ++i) {
        controlParameters_[i] = voice->getParameter(i);
        const float norm = clamp01(voice->getParameterNormalized(i));
        controlParameters_[i].setNormalized(norm);
        controlShadow_.normalized[i] = norm;
    }
    for (uint8_t i = count; i < kMaxControlParams; ++i) {
        controlParameters_[i] = Parameter{};
        controlShadow_.normalized[i] = 0.0f;
    }

    publishControlShadow_();
    // The direct structural setters already changed the active DSP object.
    appliedControlRevision_.store(controlShadow_.revision,
                                  std::memory_order_release);
}

void SwappableSynthVoice::applyPendingControlSnapshotAtAudioBoundary_() {
    const SynthParameterControlSnapshot snapshot = controlSnapshots_.read();
    const uint32_t applied =
        appliedControlRevision_.load(std::memory_order_acquire);
    if (snapshot.revision == applied) return;

    IMonoSynthVoice* voice = controlTargetVoice_();
    if (!voice) {
        appliedControlRevision_.store(snapshot.revision,
                                      std::memory_order_release);
        return;
    }

    const uint8_t count = std::min<uint8_t>(
        std::min<uint8_t>(snapshot.paramCount, voice->parameterCount()),
        kMaxControlParams);
    for (uint8_t i = 0; i < count; ++i) {
        voice->setParameterNormalized(i, clamp01(snapshot.normalized[i]));
    }

    appliedControlRevision_.store(snapshot.revision,
                                  std::memory_order_release);
}

void SwappableSynthVoice::applyPendingControlCallback_(void* context) {
    if (!context) return;
    static_cast<SwappableSynthVoice*>(context)
        ->applyPendingControlSnapshotAtAudioBoundary_();
}

void SwappableSynthVoice::captureStructuralControlCallback_(void* context) {
    if (!context) return;
    static_cast<SwappableSynthVoice*>(context)
        ->captureControlStateAfterStructuralMutation_();
}

void SwappableSynthVoice::setEngineType(SynthEngineType type) {
    if (type == type_ && !switching_) return;

    pendingType_ = type;
    next_ = createVoice(pendingType_, sampleRate_);
    if (!next_) return;

    next_->setMode(mode_);
    next_->setLoFiAmount(loFi_);

    if (noteHeld_) {
        next_->startNote(lastFreqHz_, lastAccent_, lastSlide_, lastVelocity_);
    }

    const float xfadeMs = 10.0f;
    const uint32_t samples = static_cast<uint32_t>(std::max(16.0f, (sampleRate_ * xfadeMs) / 1000.0f));
    xfadeTotal_ = samples;
    xfadePos_ = 0;
    switching_ = true;

    // Engine replacement is structural. Its freshly-created parameter model
    // becomes the new control baseline; routine knob changes after this point
    // target next_ while the old voice fades out.
    captureControlStateAfterStructuralMutation_();
}

void SwappableSynthVoice::setEngineName(const std::string& name) {
    setEngineType(parseEngineName(name));
}

SynthVoiceState SwappableSynthVoice::getState() const {
    SynthVoiceState st;
    st.engineType = type_;
    if (!current_) return st;

    const uint8_t n = std::min<uint8_t>(current_->parameterCount(), static_cast<uint8_t>(st.params.size()));
    st.paramCount = n;
    for (uint8_t i = 0; i < n; ++i) {
        st.params[i] = current_->getParameterNormalized(i);
    }
    return st;
}

void SwappableSynthVoice::setState(const SynthVoiceState& st) {
    switching_ = false;
    xfadeTotal_ = 0;
    xfadePos_ = 0;
    
    type_ = st.engineType;
    pendingType_ = st.engineType;
    current_ = createVoice(type_, sampleRate_);
    next_.reset();

    if (!current_) return;

    current_->setMode(mode_);
    current_->setLoFiAmount(loFi_);

    const uint8_t n = std::min<uint8_t>(st.paramCount, current_->parameterCount());
    for (uint8_t i = 0; i < n; ++i) {
        current_->setParameterNormalized(i, clamp01(st.params[i]));
    }
    captureControlStateAfterStructuralMutation_();
}

void SwappableSynthVoice::reset() {
    noteHeld_ = false;
    lastFreqHz_ = 0.0f;
    lastAccent_ = false;
    lastSlide_ = false;
    lastVelocity_ = 0;

    switching_ = false;
    xfadeTotal_ = 0;
    xfadePos_ = 0;
    next_.reset();

    if (current_) current_->reset();
}

void SwappableSynthVoice::setSampleRate(float sampleRate) {
    sampleRate_ = sampleRate;
    if (current_) current_->setSampleRate(sampleRate_);
    if (next_) next_->setSampleRate(sampleRate_);
}

void SwappableSynthVoice::startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity) {
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

    const float t = (xfadeTotal_ > 0) ? (static_cast<float>(xfadePos_) / static_cast<float>(xfadeTotal_)) : 1.0f;
    const float mix = clamp01(t);
    constexpr float kHalfPi = 1.57079632679f;
    const float gainA = std::cos(mix * kHalfPi);
    const float gainB = std::cos((1.0f - mix) * kHalfPi);
    const float out = a * gainA + b * gainB;

    if (xfadePos_ < xfadeTotal_) ++xfadePos_;
    if (xfadePos_ >= xfadeTotal_) {
        current_ = std::move(next_);
        next_.reset();
        type_ = pendingType_;
        switching_ = false;
        xfadeTotal_ = 0;
        xfadePos_ = 0;
    }

    return out;
}

uint8_t SwappableSynthVoice::parameterCount() const {
    return controlShadow_.paramCount;
}

void SwappableSynthVoice::setParameterNormalized(uint8_t index, float norm) {
    IMonoSynthVoice* voice = controlTargetVoice_();
    if (!voice) return;

    const uint8_t count = std::min<uint8_t>(voice->parameterCount(),
                                            kMaxControlParams);
    if (index >= count) return;

    norm = clamp01(norm);
    controlShadow_.paramCount = count;
    controlParameters_[index].setNormalized(norm);
    controlShadow_.normalized[index] = controlParameters_[index].normalized();
    publishControlShadow_();

    auto& runtime = GroovePuterAudio::AudioControlBoundaryRegistry::instance();
    if (runtime.shouldQueueRoutineControls()) {
        // The audio task will apply the complete coherent snapshot at the next
        // block boundary. No renderer pause and no DSP object mutation here.
        return;
    }

    // Boot or guarded structural mutation: no concurrent renderer owns the
    // target, so direct application remains the correct and cheaper path.
    voice->setParameterNormalized(index, controlShadow_.normalized[index]);
    appliedControlRevision_.store(controlShadow_.revision,
                                  std::memory_order_release);
}

float SwappableSynthVoice::getParameterNormalized(uint8_t index) const {
    if (index >= controlShadow_.paramCount || index >= kMaxControlParams) {
        return 0.0f;
    }
    return controlParameters_[index].normalized();
}

const Parameter& SwappableSynthVoice::getParameter(uint8_t index) const {
    static Parameter dummy("dummy", "", 0.0f, 1.0f, 0.0f, 1.0f);
    if (index >= controlShadow_.paramCount || index >= kMaxControlParams) {
        return dummy;
    }
    return controlParameters_[index];
}

void SwappableSynthVoice::setMode(GrooveboxMode mode) {
    mode_ = mode;
    if (current_) current_->setMode(mode_);
    if (next_) next_->setMode(mode_);
}

void SwappableSynthVoice::setLoFiAmount(float amount) {
    loFi_ = amount;
    if (current_) current_->setLoFiAmount(loFi_);
    if (next_) next_->setLoFiAmount(loFi_);
}

const char* SwappableSynthVoice::getEngineName() const {
    const IMonoSynthVoice* voice = (switching_ && next_) ? next_.get() : current_.get();
    return voice ? voice->getEngineName() : "none";
}
