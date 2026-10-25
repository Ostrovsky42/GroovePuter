#include "src/phrase/runtime_pattern_event_bank.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
int main() {
 std::mt19937 random(20261025);
 PhraseRuntime::RuntimePatternEventBank candidate{};
 const float gates[]={-.5f,0.f,.1f,.5f,1.f,std::numeric_limits<float>::quiet_NaN()};
 unsigned checked=0;
 for(unsigned i=0;i<256;++i){
  SynthPattern pattern{};
  for(auto& step:pattern.steps){
   step.note=static_cast<int8_t>(random()&255u);
   step.timing=static_cast<int8_t>(random()&255u);
   step.slide=(random()&1u)!=0;
  }
  for(uint8_t synth=0;synth<2;++synth) for(float gate:gates) {
   PhraseRuntime::PatternProjectionSettings settings{};
   settings.synthIndex=synth;settings.swingPercent=50+(random()%26);
   settings.swingEnabled=(random()&1u)!=0;settings.gateLengthRatio=gate;
   assert(candidate.refresh(synth,0,0,pattern,settings)==PhraseRuntime::PatternBankRefreshStatus::Ready);
   assert(candidate.select(synth,0,0).count<=SynthPattern::kSteps);
   ++checked;
  }
 }
 assert(candidate.publishPageIdentity(0));
 printf("G6 runtime projection input corpus: %u Ready; no loaded-input failure reproduced\n",checked);
}
