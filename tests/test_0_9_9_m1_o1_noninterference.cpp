#include <cstdio>
#include "scenes.h"
#include "src/generation/migration/strong_rhythm_migration.h"
using namespace GroovePuterRhythm;
void one(unsigned m,unsigned a){GenreSettings s{};s.generativeMode=m;s.recipe=0;s.rhythmSelectionMode=(uint8_t)RhythmSelectionMode::Auto;s.rhythmArchetypeId=kNoArchetypeId;StrongRhythmMigrationContext c{};c.patternAddress=a;c.level=RealizationLevel::P2Variation;c.feelProfile=FeelProfileId::Straight;c.tonalMaterializationEnabled=true;c.scaleTypeValue=kScaleDorian;DrumPatternSet d{};SynthPattern x{},y{};auto r=migrateStrongRhythmMaterial(s,c,d,x,y);std::printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",m,a,(unsigned)r.status,(unsigned)r.archetype,(unsigned)r.bassRhythmId,(unsigned)r.chordRhythmId,(unsigned)r.melodicRhythmId,(unsigned)r.motifShapeId,(unsigned)r.progressionId,r.phraseBars);for(const auto&v:d.voices)for(const auto&e:v.steps)std::printf("%u%u%u%u%u%u%u",e.hit,e.accent,e.velocity,e.timing,e.fx,e.fxParam,e.probability);for(const auto&p:{&x,&y})for(const auto&e:p->steps)std::printf("%d%u%u%u%u%d%u%u%u",e.note,e.slide,e.accent,e.ghost,e.velocity,e.timing,e.fx,e.fxParam,e.probability);std::puts("");}
int main(){one(3,7);one(3,19);one(0,0);}
