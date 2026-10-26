#include <bits/stdc++.h>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
SerialMock Serial; SDMock SD;
int main(){
 for(int f:{0,1,2})for(int note:{36,48,60})for(float resonance:{0.f,.6f})for(bool dst:{false,true}){
  MiniAcid engine(22050,nullptr);engine.init();engine.setSongMode(false);engine.setGrooveboxMode(GrooveboxMode::Acid);
  engine.mute303=false;engine.mute303_2=true;engine.setDeviceMasterVolume(.6f);engine.setTrackVolume(VoiceId::SynthA,1.f);
  engine.set303Parameter(TB303ParamId::FilterType,f,0);engine.set303Parameter(TB303ParamId::Cutoff,800,0);engine.set303Parameter(TB303ParamId::Resonance,resonance,0);engine.set303Parameter(TB303ParamId::EnvAmount,400,0);
  engine.set303DistortionEnabled(0,dst);engine.delay303.setEnabled(false);
  engine.synthVoices_[0]->startNote(440.f*std::pow(2.f,(note-69)/12.f),false,false,100);
  double energy=0;int count=0,peak=0;int16_t block[256];
  for(int frame=0;frame<22016;frame+=256){
   if(frame>=13312)engine.synthVoices_[0]->release();
   engine.generateAudioBuffer(block,256);
   for(int i=0;i<256;++i)if(frame+i>=3308&&frame+i<9922){double v=block[i]/32768.;energy+=v*v;++count;peak=std::max(peak,std::abs(int(block[i])));}
  }
  double rms=std::sqrt(energy/count);
  std::printf("MIX_LEVEL filter=%d note=%d res=%.2f dst=%d rms=%.9g dbfs=%.3f peak_pcm=%d\n",f,note,resonance,dst,rms,20*std::log10(std::max(rms,1e-15)),peak);
 }
}
