#include "drum_sampler_track.h"

DrumSamplerTrack::DrumSamplerTrack() {
    // Initialize pads with defaults if needed
}

void DrumSamplerTrack::triggerPad(int padIndex, float velocity, ISampleStore& store, bool forceReverse) {
    if (padIndex < 0 || padIndex >= kNumPads) return;
    
    SamplerPad& p = pads_[padIndex];
    if (p.id.value == 0) return;

    // 1. Choke Group Logic
    if (p.chokeGroup > 0) {
        for (int i = 0; i < kNumPads; ++i) {
            if (pads_[i].chokeGroup == p.chokeGroup) {
                // Stop any voices that were triggered by pads in this choke group.
                // We use the pad index 'i' as the voice tag.
                pool_.stopByTag(i);
            }
        }
    }

    // 2. Setup Params
    SamplerVoice::Params vp;
    vp.id = p.id;
    vp.pitch = p.pitch;
    vp.gain = p.volume * velocity;
    vp.startFrame = p.startFrame;
    vp.endFrame = p.endFrame;
    vp.reverse = p.reverse || forceReverse;
    vp.loop = p.loop;

    // 3. Trigger in pool
    pool_.trigger(vp, store, padIndex);
}

void DrumSamplerTrack::stopPad(int padIndex) {
    if (padIndex < 0 || padIndex >= kNumPads) return;
    pool_.stopByTag(padIndex);
}

void DrumSamplerTrack::process(float* output, uint32_t numFrames, ISampleStore& store) {
    pool_.process(output, numFrames, store);
}
