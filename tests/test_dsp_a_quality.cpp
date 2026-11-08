#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "src/dsp/mini_tb303.h"
#include "src/dsp/wave_morph_synth_voice.h"
#include "src/dsp/sound_quality_presets.h"
#include "src/dsp/swappable_synth_voice.h"

constexpr int sr = 22050;
void stableFilter() {
  for(float cutoff : {80.0f, 800.0f, 2500.0f, 6500.0f, 8000.0f}) {
    for(float resonance : {0.0f, .4f, .85f}) {
      ChamberlinFilter filter(sr);
      double lateEnergy = 0;
      float peak = 0;
      for(int i=0;i<sr;++i) {
        const float out=filter.process(i==0 ? 1.0f:0.0f,cutoff,resonance);
        assert(std::isfinite(out));
        peak=std::fmax(peak,std::fabs(out));
        if(i>=sr/2) lateEnergy+=out*out;
      }
      const double lateRms=std::sqrt(lateEnergy/(sr-sr/2));
      if(lateRms>=.0001 || peak>=8) std::fprintf(stderr,"filter cutoff=%g resonance=%g lateRms=%g peak=%g\n",cutoff,resonance,lateRms,peak);
      assert(lateRms < .0001 && peak < 8);
    }
  }
}
void automatedFilter() {
  ChamberlinFilter filter(sr);
  double tail=0;
  for(int i=0;i<3*sr;++i) {
    const float cutoff=i<2*sr ? 80.0f+(i%2048)*(7920.0f/2047) : 800.0f;
    const float input=i<2*sr ? .25f*std::sin(i*6.28318530718f*110/sr) : 0;
    const float out=filter.process(input,cutoff,.85f);
    assert(std::isfinite(out) && std::fabs(out)<8);
    if(i>=5*sr/2) tail+=out*out;
  }
  assert(std::sqrt(tail/(sr/2))<.0001);
}

void retriggerTb() {
  TB303Voice continuous(sr), retriggered(sr);
  continuous.startNote(110,false,false,100);retriggered.startNote(110,false,false,100);
  float before=0;
  for(int i=0;i<sr/2;++i) {before=continuous.process();retriggered.process();}
  const float expected=continuous.process();
  retriggered.startNote(110,false,false,100);
  const float actual=retriggered.process();
  std::printf("TB303 retrigger jump=%g continuous=%g\n",std::fabs(actual-before),std::fabs(expected-before));
  assert(std::fabs(actual-expected)<.02f);
  retriggered.release();
  for(int i=0;i<sr;++i) retriggered.process();
  assert(!retriggered.isVoiceActive() && retriggered.process()==0);
}
void retriggerWave() {
  WaveMorphSynthVoice continuous(sr), retriggered(sr);
  for(auto* voice : {&continuous,&retriggered}) {
    voice->setParameterNormalized(0,0);voice->setParameterNormalized(1,0);voice->setParameterNormalized(2,0);
    voice->startNote(110,false,false,127);
  }
  float before=0;
  for(int i=0;i<2*sr+53;++i) {before=continuous.process();retriggered.process();}
  const float expected=continuous.process();
  retriggered.startNote(110,false,false,127);
  const float actual=retriggered.process();
  std::printf("WAVEMORPH retrigger jump=%g continuous=%g\n",std::fabs(actual-before),std::fabs(expected-before));
  assert(std::fabs(actual-expected)<.02f);
  retriggered.release();
  for(int i=0;i<sr;++i) retriggered.process();
  assert(retriggered.process()==0);
}
void explicitPresets() {
  for (int i=0;i<3;++i) {
    TB303Voice tb(sr);
    tb.setParameter(TB303ParamId::MainVolume,.37f);
    assert(SoundQualityPresets::apply(tb,static_cast<SoundQualityPresets::Tb303>(i)));
    assert(tb.parameterValue(TB303ParamId::MainVolume)==.37f);
    assert(tb.parameterCount()==4);
    WaveMorphSynthVoice wave(sr);
    assert(SoundQualityPresets::apply(wave,static_cast<SoundQualityPresets::WaveMorph>(i)));
    assert(wave.parameterCount()==6);
    tb.startNote(110,false,false,100);wave.startNote(110,false,false,100);
    double tbEnergy=0,waveEnergy=0;
    for(int n=0;n<sr;++n) {
      float a=tb.process(),b=wave.process();assert(std::isfinite(a)&&std::isfinite(b));
      tbEnergy+=a*a;waveEnergy+=b*b;
    }
    assert(tbEnergy>1 && waveEnergy>1);
    tb.release();wave.release();
    for(int n=0;n<sr;++n){tb.process();wave.process();}
    assert(tb.process()==0 && wave.process()==0);
  }
  TB303Voice untouched(sr);
  assert(untouched.parameterValue(TB303ParamId::Cutoff)==800);
  assert(untouched.parameterValue(TB303ParamId::Resonance)==0);
  assert(!SoundQualityPresets::apply(untouched,SoundQualityPresets::Tb303::Count));
  assert(untouched.parameterValue(TB303ParamId::Cutoff)==800);
}

void persistedTbControls() {
  SwappableSynthVoice source(sr,SynthEngineType::TB303);
  auto* tb=static_cast<TB303Voice*>(source.activeVoice());assert(tb);
  for(uint8_t i=0;i<4;++i) source.setParameterNormalized(i,.11f*(i+1));
  tb->setParameter(TB303ParamId::Oscillator,3);
  tb->setParameter(TB303ParamId::FilterType,4);
  tb->setParameter(TB303ParamId::MainVolume,.37f);
  const auto state=source.getState();
  assert(state.paramCount==6 && source.parameterCount()==4);
  SwappableSynthVoice restored(sr,SynthEngineType::WAVEMORPH);
  restored.setState(state);
  tb=static_cast<TB303Voice*>(restored.activeVoice());assert(tb);
  assert(tb->oscillatorIndex()==3);
  assert(tb->parameter(TB303ParamId::FilterType).optionIndex()==4);
  assert(tb->parameterValue(TB303ParamId::MainVolume)==.8f); // internal trim is not a saved user control
  for(uint8_t i=0;i<4;++i) assert(restored.getParameterNormalized(i)==state.params[i]);
  // Old four-parameter projects retain the prior default interpretation.
  auto legacy=state;legacy.paramCount=4;
  restored.setState(legacy);tb=static_cast<TB303Voice*>(restored.activeVoice());assert(tb);
  assert(tb->oscillatorIndex()==0 && tb->parameter(TB303ParamId::FilterType).optionIndex()==0);
  assert(tb->parameterValue(TB303ParamId::MainVolume)==.8f);
  for(uint8_t i=0;i<4;++i) assert(restored.getParameterNormalized(i)==legacy.params[i]);
}

int main(int argc,char**argv) {
  if(argc!=2) return 2;
  if(!std::strcmp(argv[1],"filter")) stableFilter();
  else if(!std::strcmp(argv[1],"tb-retrigger")) retriggerTb();
  else if(!std::strcmp(argv[1],"wave-retrigger")) retriggerWave();
  else if(!std::strcmp(argv[1],"presets")) explicitPresets();
  else if(!std::strcmp(argv[1],"automation")) automatedFilter();
  else if(!std::strcmp(argv[1],"state")) persistedTbControls();
  else return 2;
}
