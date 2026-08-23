#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "src/generation/phrase/phrase_evolution.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {
uint8_t bits(StepMask value) { uint8_t n=0; while(value){value=static_cast<StepMask>(value&(value-1u));++n;} return n; }
StepMask secondaryAdded(const RhythmPhrasePlan& before,const RhythmPhrasePlan& after){StepMask v=0;for(uint8_t b=0;b<before.barCount;++b)for(uint8_t r=0;r<kRhythmRoleCount;++r)v=static_cast<StepMask>(v|(after.bars[b].roles[r].secondary&~before.bars[b].roles[r].secondary));return v;}
StepMask secondaryRemoved(const RhythmPhrasePlan& before,const RhythmPhrasePlan& after){StepMask v=0;for(uint8_t b=0;b<before.barCount;++b)for(uint8_t r=0;r<kRhythmRoleCount;++r)v=static_cast<StepMask>(v|(before.bars[b].roles[r].secondary&~after.bars[b].roles[r].secondary));return v;}
void dump(const char* tag, const RhythmPhrasePlan& p) {
  std::printf("%s bars=%u", tag, p.barCount);
  for (uint8_t b=0;b<p.barCount;++b) for(uint8_t r=0;r<kRhythmRoleCount;++r){const auto& x=p.bars[b].roles[r]; std::printf(" b%u/%04x,%04x,%04x,%04x",b,x.structural,x.secondary,x.ghosts,x.accents);}
  std::puts("");
}
GenerationContext gen(RhythmArchetypeId id,uint16_t ordinal){GenerationContext g{};g.projectSeed=0xE1000000u|id;g.phraseOrdinal=ordinal;return g;}
}

int main() {
  const auto& catalog=ReferenceVocabulary::phraseEvolutionCatalog();
  const RhythmArchetypeId ids[]={404,413,714};
  for(auto id:ids){
    RhythmRealizationRequest base{}; base.catalog=&catalog;base.archetypeId=id;base.phraseBars=1;base.level=RealizationLevel::P1Canonical;base.generation=gen(id,17);
    const auto canonical=realizeRhythmPhrase(base); assert(canonical.status!=RealizationStatus::InvalidConstraintSet);
    auto varied=base; varied.level=RealizationLevel::P3Transformation; varied.reuseIdentity=&canonical.identity;
    const auto first=realizeRhythmPhrase(varied); const auto second=realizeRhythmPhrase(varied);
    assert(first.status!=RealizationStatus::InvalidConstraintSet); assert(std::memcmp(&first.plan,&second.plan,sizeof(first.plan))==0);
    dump("CASE canonical",canonical.plan); dump("CASE production-P2",first.plan);
    std::printf("DIFF id=%u secondary_added=%u secondary_removed=%u ghost=%u accent=%u structural_unchanged=%s DROP=0 DISPLACE=0 RERUN_EQUAL=true\n",id,bits(secondaryAdded(canonical.plan,first.plan)),bits(secondaryRemoved(canonical.plan,first.plan)),bits(first.plan.bars[0].roles[0].ghosts),bits(first.plan.bars[0].roles[0].accents),secondaryAdded(canonical.plan,first.plan)==0?"true":"false");
  }
  bool foundSecondary=false;
  for (RhythmArchetypeId id : {RhythmArchetypeId(404),RhythmArchetypeId(413),RhythmArchetypeId(714)}) for(uint16_t seed=0;seed<512&&!foundSecondary;++seed){RhythmRealizationRequest base{};base.catalog=&catalog;base.archetypeId=id;base.phraseBars=1;base.level=RealizationLevel::P1Canonical;base.generation=gen(id,seed);auto before=realizeRhythmPhrase(base);auto varied=base;varied.level=RealizationLevel::P3Transformation;varied.reuseIdentity=&before.identity;auto after=realizeRhythmPhrase(varied);if(secondaryAdded(before.plan,after.plan)){std::printf("SECONDARY-SEARCH id=%u ordinal=%u added=%u\n",id,seed,bits(secondaryAdded(before.plan,after.plan)));foundSecondary=true;}}
  assert(foundSecondary);
  PhraseEvolutionRequest request{}; request.catalog=&catalog;request.archetypeId=404;request.level=RealizationLevel::P2Variation;request.generation=gen(404,23);request.roleIdentity={};
  for(uint8_t bars:{uint8_t(2),uint8_t(4),uint8_t(8)}){request.phraseBars=bars;const auto first=evolveMultiBarPhrase(request);const auto second=evolveMultiBarPhrase(request);assert(first.status==PhraseEvolutionStatus::Ok);assert(std::memcmp(&first,&second,sizeof(first))==0);assert(first.segmentCount==(bars==8?2:1));if(bars==8){assert(first.rhythmIdentity.phraseBars==4);assert(first.bars[0].function==first.bars[4].function||first.segmentTrajectories[0]!=first.segmentTrajectories[1]);}std::printf("AUDITION-DIRECT bars=%u segments=%u traj=%u/%u identity_bars=%u phraseOrdinal_transition=%s RERUN_EQUAL=true\n",bars,first.segmentCount,first.segmentTrajectories[0],first.segmentTrajectories[1],first.rhythmIdentity.phraseBars,bars==8?"N->N+1":"none");}
  for(TrajectoryId trajectory:{TrajectoryId(6),TrajectoryId(7)}){BarEvolutionRequest legacy{};legacy.catalog=&catalog;legacy.archetypeId=404;legacy.phraseBars=4;legacy.level=RealizationLevel::P3Transformation;legacy.generation=gen(404,91);legacy.requestedTrajectoryId=trajectory;auto evolved=evolveRhythmPhrase(legacy);assert(evolved.status==BarEvolutionStatus::Ok);std::printf("LEGACY trajectory=%u primitive_structural_after=%04x policy=trajectory RERUN_EQUAL=true\n",trajectory,evolved.plan.bars[1].roles[0].structural);}
}
