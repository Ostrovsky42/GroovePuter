#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "src/dsp/mini_tb303.h"
#include "src/dsp/tube_distortion.h"
int main() {
 std::puts("filter,note,cutoff,resonance,env_amount,dry_rms,dst_rms,dst_gain_db,dry_peak,dst_peak");
 for(int f=0;f<7;++f)for(int note:{36,48,60})for(float cutoff:{200.f,400.f,800.f,1600.f,4000.f})for(float res:{0.f,.3f,.6f,.9f})for(float env:{0.f,400.f}) {
  TB303Voice voice(22050);voice.setParameter(TB303ParamId::FilterType,f);
  voice.setParameter(TB303ParamId::Cutoff,cutoff);voice.setParameter(TB303ParamId::Resonance,res);voice.setParameter(TB303ParamId::EnvAmount,env);
  TubeDistortion dst;dst.setDrive(8);dst.setEnabled(true);
  voice.startNote(440.f*std::pow(2.f,(note-69)/12.f),false,false,100);
  double dry=0,wet=0;float dp=0,wp=0;int n=0;
  for(int i=0;i<22050;++i) {
   if(i==13230)voice.release();
   const float d=voice.process()*.5f,w=dst.process(d);
   if(!std::isfinite(d)||!std::isfinite(w))return 2;
   if(i>=3308&&i<9922){dry+=double(d)*d;wet+=double(w)*w;dp=std::fmax(dp,std::fabs(d));wp=std::fmax(wp,std::fabs(w));++n;}
  }
  dry=std::sqrt(dry/n);wet=std::sqrt(wet/n);
  std::printf("%d,%d,%.0f,%.2f,%.0f,%.10g,%.10g,%.4f,%.10g,%.10g\n",f,note,cutoff,res,env,dry,wet,20*std::log10(std::fmax(wet,1e-15)/std::fmax(dry,1e-15)),dp,wp);
 }
}
