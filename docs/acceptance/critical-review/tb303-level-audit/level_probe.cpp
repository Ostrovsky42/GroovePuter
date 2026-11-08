#include <cmath>
#include <cstdio>
#include <memory>
#include "src/dsp/mini_tb303.h"
#include "src/dsp/felucca_analog_voice.h"
#include "src/dsp/sound_quality_presets.h"
static void measure(const char* name, int preset, int note) {
 std::unique_ptr<IMonoSynthVoice> voice;
 if (name[0]=='T') {
  auto tb=std::make_unique<TB303Voice>(22050);
  if(preset>=0) SoundQualityPresets::apply(*tb,static_cast<SoundQualityPresets::Tb303>(preset));
  voice=std::move(tb);
 } else {
  voice=std::make_unique<FeluccaAnalogVoice>(22050);
  voice->setParameterNormalized(0,float(preset)/12.f);
 }
 voice->startNote(440.f*std::pow(2.f,(note-69)/12.f),false,false,100);
 double energy=0;float peak=0,tail=0;int count=0;
 for(int i=0;i<44100;++i) {
  if(i==13230) voice->release();
  float v=voice->process();
  if(!std::isfinite(v)) {std::fprintf(stderr,"nonfinite\n");std::exit(1);}
  if(i>=3308&&i<9922){energy+=double(v)*v;++count;peak=std::fmax(peak,std::fabs(v));}
  if(i>=43588)tail=std::fmax(tail,std::fabs(v));
 }
 double rms=std::sqrt(energy/count);
 std::printf("%s,%d,%d,100,%.9g,%.3f,%.9g,%.9g\n",name,preset,note,rms,20*std::log10(std::fmax(rms,1e-12)),peak,tail);
}
int main(){
 std::puts("engine,preset,note,velocity,sustain_rms,sustain_dbfs,peak,tail_last512");
 for(int note:{36,48,60}) {
  for(int preset=-1;preset<3;++preset)measure("TB303",preset,note);
  for(int preset=0;preset<13;++preset)measure("DUAL OSC",preset,note);
 }
}
