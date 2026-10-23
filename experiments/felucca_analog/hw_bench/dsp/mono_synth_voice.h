#ifndef MONO_SYNTH_VOICE_H
#define MONO_SYNTH_VOICE_H

#pragma once

#include <stdint.h>
#include <string>
#include "mini_dsp_params.h"

// enum class GrooveboxMode removed as it is included from mini_dsp_params.h

// Common parameter interface for all swappable synth engines.
// This allows the UI to generically adjust "Parameter 0", "Parameter 1" 
// without knowing if the underlying engine is a TB303 or a SID chip.
class IMonoSynthVoice {
public:
    virtual ~IMonoSynthVoice() = default;

    virtual void reset() = 0;
    virtual void setSampleRate(float sampleRate) = 0;
    
    // Core synthesis trigger
    virtual void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) = 0;
    virtual void release() = 0;
    
    // Process next sample
    virtual float process() = 0;

    // Parameter abstraction
    virtual uint8_t parameterCount() const = 0;
    virtual void setParameterNormalized(uint8_t index, float norm) = 0;
    virtual float getParameterNormalized(uint8_t index) const = 0;
    virtual const Parameter& getParameter(uint8_t index) const = 0;
    
    // Name of the engine (e.g., "TB303", "SID", "FM")
    virtual const char* getEngineName() const = 0;

    // Engine-specific global mode/lofi state
    virtual void setMode(GrooveboxMode mode) = 0;
    virtual void setLoFiAmount(float amount) = 0;
};

#endif // MONO_SYNTH_VOICE_H
