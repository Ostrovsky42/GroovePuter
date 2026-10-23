// DSP-A: deterministic, dry, real-voice references at the product sample rate.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include "src/audio/audio_config.h"
#include "src/dsp/swappable_synth_voice.h"

namespace {
bool audioScope = false;
size_t audioAllocations = 0, audioFrees = 0;
}
void* operator new(size_t n) {
  if (audioScope) ++audioAllocations;
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (audioScope && p) ++audioFrees;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, size_t) noexcept { ::operator delete(p); }

namespace {
constexpr int sr = kSampleRate;
struct Event { int frame; int note; int velocity; bool accent, slide, off; };
struct Case {
  std::string id, engine;
  int note=45, velocity=100, filter=-1, osc=-1, table=-1;
  float morph=-1, sub=-1;
  bool sequence=false, retrigger=false, morphRamp=false, stress=false;
};
std::unique_ptr<IMonoSynthVoice> voiceFor(const std::string& engine) {
  if (engine=="TB303") return std::make_unique<TB303Voice>(sr);
  if (engine=="SH101") return std::make_unique<Sh101SynthVoice>(sr);
  if (engine=="WAVEMORPH") return std::make_unique<WaveMorphSynthVoice>(sr);
  if (engine=="AY") return std::make_unique<AySynthVoice>(sr);
  if (engine=="SID") return std::make_unique<SidSynthVoice>(sr);
  if (engine=="SN76489") return std::make_unique<Sn76489SynthVoice>(sr);
  throw std::runtime_error("unknown engine");
}
void configure(IMonoSynthVoice& voice, const Case& c) {
  if (c.engine=="TB303") {
    auto& tb=static_cast<TB303Voice&>(voice);
    if (c.filter>=0) tb.setParameter(TB303ParamId::FilterType,c.filter);
    if (c.osc>=0) tb.setParameter(TB303ParamId::Oscillator,c.osc);
    if (c.stress) {
      tb.setParameter(TB303ParamId::Cutoff,2500);
      tb.setParameter(TB303ParamId::Resonance,.85f);
      tb.setParameter(TB303ParamId::EnvAmount,2000);
      tb.setParameter(TB303ParamId::EnvDecay,2200);
    }
  }
  if (c.engine=="WAVEMORPH") {
    if (c.table>=0) voice.setParameterNormalized(0,c.table/7.0f);
    if (c.morph>=0) voice.setParameterNormalized(1,c.morph);
    if (c.sub>=0) voice.setParameterNormalized(2,c.sub);
  }
}
std::vector<Event> eventsFor(const Case& c) {
  std::vector<Event> events;
  if (c.sequence) {
    const int notes[]={36,43,48,46,36,55,48,43};
    for (int i=0;i<8;++i) {
      events.push_back({i*sr/8,notes[i],i%2 ? 110:80,i%4==2,i%4==1,false});
      if (i==7 || (i+1)%4!=1) events.push_back({i*sr/8+sr*9/100,0,0,false,false,true});
    }
  } else if (c.retrigger) {
    for (int i=0;i<5;++i) events.push_back({i*(sr*187/1000)+53,c.note,c.velocity,false,false,false});
    events.push_back({sr,0,0,false,false,true});
  } else {
    events.push_back({0,c.note,c.velocity,c.stress,false,false});
    events.push_back({sr*3/5,0,0,false,false,true});
  }
  std::stable_sort(events.begin(),events.end(),[](const Event&a,const Event&b){return a.frame<b.frame;});
  return events;
}
void u16(std::ofstream& file,uint16_t v) { char b[]={char(v),char(v>>8)};file.write(b,2); }
void u32(std::ofstream& file,uint32_t v) { char b[]={char(v),char(v>>8),char(v>>16),char(v>>24)};file.write(b,4); }
void writeWav(const std::filesystem::path& path,const std::vector<float>& data) {
  std::ofstream f(path,std::ios::binary);
  f.write("RIFF",4);u32(f,36+data.size()*2);f.write("WAVEfmt ",8);u32(f,16);u16(f,1);u16(f,1);
  u32(f,sr);u32(f,sr*2);u16(f,2);u16(f,16);f.write("data",4);u32(f,data.size()*2);
  for(float value:data) u16(f,static_cast<uint16_t>(static_cast<int16_t>(std::lrint(std::clamp(value,-1.0f,1.0f)*32767.0f))));
  if(!f) throw std::runtime_error("WAV write failed");
}
void render(const Case& c,const std::filesystem::path& root,std::ofstream& csv) {
  auto voice=voiceFor(c.engine);configure(*voice,c);
  const auto events=eventsFor(c);
  std::vector<float> output(sr*2);
  size_t ei=0;int lastOff=0;
  double maxJump=0,peak=0,energy=0,tail=0;size_t clipped=0;
  const auto allocationsBefore=audioAllocations, freesBefore=audioFrees;
  const auto begin=std::chrono::steady_clock::now();
  audioScope=true;
  for(size_t i=0;i<output.size();++i) {
    bool noteOn=false;
    while(ei<events.size() && events[ei].frame==static_cast<int>(i)) {
      const auto& e=events[ei++];
      if(e.off) { voice->release();lastOff=static_cast<int>(i); }
      else {voice->startNote(440.0f*std::pow(2.0f,(e.note-69)/12.0f),e.accent,e.slide,static_cast<uint8_t>(e.velocity));noteOn=true;}
    }
    if(c.morphRamp && i<static_cast<size_t>(sr*3/5)) voice->setParameterNormalized(1,static_cast<float>(i)/(sr*3/5));
    const float value=voice->process();
    if(!std::isfinite(value)) { audioScope=false;throw std::runtime_error(c.id+": nonfinite sample"); }
    output[i]=value;
    if(noteOn && i) maxJump=std::max(maxJump,std::abs(double(value-output[i-1])));
    peak=std::max(peak,std::abs(double(value)));energy+=double(value)*value;
    if(std::abs(value)>=1.0f) ++clipped;
    if(i>=output.size()-512) tail=std::max(tail,std::abs(double(value)));
  }
  audioScope=false;
  const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
  if(audioAllocations!=allocationsBefore || audioFrees!=freesBefore) throw std::runtime_error(c.id+": allocation/free on audio path");
  writeWav(root/(c.id+".wav"),output);
  std::ofstream raw(root/(c.id+".f32"),std::ios::binary);raw.write(reinterpret_cast<const char*>(output.data()),output.size()*sizeof(float));
  if(!raw) throw std::runtime_error("float reference write failed");
  csv<<c.id<<','<<c.engine<<','<<sr<<','<<output.size()<<','<<peak<<','<<std::sqrt(energy/output.size())<<','<<clipped<<','<<tail<<','<<maxJump<<','<<lastOff<<','<<us<<','<<audioAllocations-allocationsBefore<<','<<audioFrees-freesBefore<<'\n';
  std::ofstream params(root/(c.id+".params.csv"));
  params<<"index,label,value,normalized\n";
  if(c.engine=="TB303") {
    auto& tb=static_cast<TB303Voice&>(*voice);
    for(int i=0;i<static_cast<int>(TB303ParamId::Count);++i) {const auto&p=tb.parameter(static_cast<TB303ParamId>(i));params<<i<<','<<p.label()<<','<<p.value()<<','<<p.normalized()<<'\n';}
  } else for(uint8_t i=0;i<voice->parameterCount();++i) {const auto&p=voice->getParameter(i);params<<int(i)<<','<<p.label()<<','<<p.value()<<','<<p.normalized()<<'\n';}
}
}
int main(int argc,char**argv) {
  if(argc!=2) {std::fprintf(stderr,"usage: dsp_a_render OUTPUT_DIR\n");return 2;}
  const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
  std::ofstream csv(root/"metrics.csv");csv.precision(10);
  csv<<"case,engine,sample_rate,frames,peak,rms,clipped_samples,tail_peak_last512,retrigger_jump,last_note_off_frame,host_microseconds,audio_allocations,audio_frees\n";
  try {
    for(const auto* engine:{"TB303","SH101","WAVEMORPH","AY","SID","SN76489"}) {
      for(int note:{36,48,60,72}) for(int velocity:{48,100}) { Case c; c.engine=engine;c.note=note;c.velocity=velocity;c.id=c.engine+"-n"+std::to_string(note)+"-v"+std::to_string(velocity);render(c,root,csv); }
      for(int mode=0;mode<2;++mode) {Case c;c.engine=engine;c.sequence=mode==0;c.retrigger=mode==1;c.id=c.engine+(mode==0 ? "-sequence":"-retrigger");render(c,root,csv);}
    }
    for(int filter=0;filter<7;++filter) for(int osc=0;osc<5;++osc) {
      Case c;c.engine="TB303";c.filter=filter;c.osc=osc;c.id="TB303-f"+std::to_string(filter)+"-o"+std::to_string(osc);render(c,root,csv);
    }
    for(int filter=0;filter<7;++filter) {Case c;c.engine="TB303";c.filter=filter;c.stress=true;c.id="TB303-f"+std::to_string(filter)+"-accent-extreme";render(c,root,csv);}
    for(int table=0;table<8;++table) for(int step=0;step<=4;++step) {Case c;c.engine="WAVEMORPH";c.table=table;c.morph=step*.25f;c.sub=0;c.id="WAVEMORPH-t"+std::to_string(table)+"-m"+std::to_string(step);render(c,root,csv);}
    for(int table=0;table<8;++table) {Case c;c.engine="WAVEMORPH";c.table=table;c.sub=0;c.morphRamp=true;c.id="WAVEMORPH-t"+std::to_string(table)+"-ramp";render(c,root,csv);}
  } catch(const std::exception&e) {std::fprintf(stderr,"DSP-A render: %s\n",e.what());return 1;}
  std::puts("DSP-A deterministic references: PASS (all audio-path allocation/free counters zero)");
}
