#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,hashlib,io,json,re,zipfile
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

EXPECTED_SHA256='5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd'
@dataclass(frozen=True)
class Spec:
    atlas_id:str; runtime_id:int; filename:str
SPECS=(
 Spec('REC_ACID_CHICAGO_JACK',6,'rec_acid_chicago_jack.generated.h'),
 Spec('REC_ACID_ROLLING',7,'rec_acid_rolling.generated.h'),
 Spec('REC_UKG_CLASSIC_2STEP',8,'rec_ukg_classic_2step.generated.h'),
 Spec('REC_UKG_DARK_SKIPPY',9,'rec_ukg_dark_skippy.generated.h'),
 Spec('REC_DUB_DEEP_CHORD',10,'rec_dub_deep_chord.generated.h'),
 Spec('REC_DUB_MINIMAL_SPACE',11,'rec_dub_minimal_space.generated.h'),
)
TARGETS={'KICK':0,'SNARE':1,'HAT1':2,'HAT2':3,'PERC1':4,'PERC2':5,'RIM':6,'CLAP':7,'SYNTH1':8,'SYNTH2':9,'DX':9}
PRIORITY={'SYNTH2':20,'DX':10}
ROOTS={'C':0,'C#':1,'DB':1,'D':2,'D#':3,'EB':3,'E':4,'F':5,'F#':6,'GB':6,'G':7,'G#':8,'AB':8,'A':9,'A#':10,'BB':10,'B':11}
ACTIVE,ACCENT,SLIDE,SUSTAIN=1,2,4,8

def rows(z,root,rel):return list(csv.DictReader(io.StringIO(z.read(root+rel).decode('utf-8-sig'))))
def integer(v,default=0):
 v=(v or '').strip();return int(v) if v else default
def boolean(v):return (v or '').strip().lower()=='true'
def clamp(v,l,h):return max(l,min(h,v))
def identifier(v):return re.sub(r'[^A-Za-z0-9]+','_',v).strip('_')
def quoted(v):return json.dumps(v,ensure_ascii=True)
def chord_note(e):
 root=(e.get('chord_root') or '').strip()
 if not root:
  m=re.match(r'^([A-Ga-g])([#b]?)',(e.get('pitch') or '').strip());root=m.group(1).upper()+m.group(2) if m else ''
 pc=ROOTS.get(root.upper());return None if pc is None else 48+pc

def compile_event(e,track):
 target=TARGETS.get(track)
 if target is None:return None
 step=integer(e.get('step_index'))-1
 if not 0<=step<16:raise ValueError(f"{e['event_id']}: invalid step")
 timing=clamp(integer(e.get('substep_offset'))+integer(e.get('microtiming_ticks')),-23,23)
 velocity=clamp(integer(e.get('velocity'),100),1,127)
 probability=clamp(integer(e.get('probability_percent'),100),0,100)
 art=(e.get('articulation') or '').strip().upper();flags=ACTIVE
 if boolean(e.get('accent')) or art=='ACCENT':flags|=ACCENT
 if boolean(e.get('glide_from_previous')) or art=='SLIDE':flags|=SLIDE
 if art=='SUSTAIN' or integer(e.get('note_length_steps'),1)>1:flags|=SUSTAIN
 if target>=8:
  raw=(e.get('midi_note') or '').strip();note=clamp(int(raw),0,127) if raw else (chord_note(e) if track=='SYNTH2' else None)
  if note is None:return None
 else:note=-1
 return (target,step,note,velocity,timing,probability,flags,PRIORITY.get(track,0))

def render_types():return '''#pragma once

#include <cstddef>
#include <cstdint>

// Generated from SEQTRAK Pattern Atlas schema 2.6.0.
// Do not edit manually.

namespace AtlasGenerated {

enum EventFlags : uint8_t {
  kActive = 1u << 0,
  kAccent = 1u << 1,
  kSlide = 1u << 2,
  kSustain = 1u << 3,
};

struct Event {
  uint8_t target;
  uint8_t step;
  int8_t note;
  uint8_t velocity;
  int8_t timing;
  uint8_t probability;
  uint8_t flags;
};

struct Pattern {
  const char* atlasPatternId;
  const char* slotId;
  const char* slotFunction;
  const Event* events;
  uint16_t eventCount;
};

struct Recipe {
  uint8_t runtimeRecipeId;
  const char* atlasRecipeId;
  const char* displayName;
  uint16_t bpm;
  uint8_t swingPercent;
  const Pattern* patterns;
  uint8_t patternCount;
};

}  // namespace AtlasGenerated
'''

def render_recipe(spec,recipe,links,data):
 lines=['#pragma once','','#include "atlas_runtime_types.generated.h"','','namespace AtlasGenerated {',''];arr=[]
 for link in links:
  pid=link['pattern_id'];name='kEvents_'+identifier(pid);arr.append((link,name));lines.append(f'inline constexpr Event {name}[] = {{')
  for e in data[pid]:lines.append(f'  {{{e[0]}, {e[1]}, {e[2]}, {e[3]}, {e[4]}, {e[5]}, {e[6]}}},')
  lines+=['};','']
 pname='kPatterns_'+identifier(spec.atlas_id);lines.append(f'inline constexpr Pattern {pname}[] = {{')
 for link,name in arr:
  lines.append(f'  {{{quoted(link["pattern_id"])}, {quoted(link["slot_id"])}, {quoted(link["slot_function"])}, {name}, static_cast<uint16_t>(sizeof({name}) / sizeof({name}[0]))}},')
 display=recipe['display_name'].replace(' SEQTRAK recipe','')
 lines+=['};','',f'inline constexpr Recipe kRecipe_{identifier(spec.atlas_id)} = {{{spec.runtime_id}, {quoted(spec.atlas_id)}, {quoted(display)}, {integer(recipe["default_bpm"])}, {integer(recipe["swing_percent"])}, {pname}, static_cast<uint8_t>(sizeof({pname}) / sizeof({pname}[0]))}};','', '}  // namespace AtlasGenerated','']
 return '\n'.join(lines)

def render_index(stats):
 includes='\n'.join(f'#include "{s.filename}"' for s in SPECS)
 recipes='\n'.join(f'  kRecipe_{identifier(s.atlas_id)},' for s in SPECS)
 return f'''#pragma once

#include "atlas_runtime_types.generated.h"
{includes}

namespace AtlasGenerated {{

inline constexpr Recipe kRecipes[] = {{
{recipes}
}};

inline constexpr size_t kRecipeCount = sizeof(kRecipes) / sizeof(kRecipes[0]);
inline constexpr uint16_t kIgnoredSamplerEvents = {stats['ignored_sampler_events']};
inline constexpr uint16_t kIgnoredUnsupportedTracks = {stats['ignored_unsupported_tracks']};
inline constexpr uint16_t kIgnoredUnrepresentablePitchEvents = {stats['ignored_unrepresentable_pitch_events']};

}}  // namespace AtlasGenerated
'''

def compile_all(zip_path,out):
 digest=hashlib.sha256(zip_path.read_bytes()).hexdigest()
 if digest!=EXPECTED_SHA256:raise ValueError(f'unexpected Atlas archive SHA-256: {digest}')
 with zipfile.ZipFile(zip_path) as z:
  roots={n.split('/',1)[0] for n in z.namelist() if '/' in n}
  if len(roots)!=1:raise ValueError('Atlas ZIP must contain one root')
  root=next(iter(roots))+'/'
  summary=json.loads(z.read(root+'reports/validation_summary.json'))
  if summary.get('schema_version')!='2.6.0' or summary.get('failures')!=0:raise ValueError('Atlas validation gate failed')
  recipes=rows(z,root,'core/recipes.csv');caps=rows(z,root,'runtime/recipe_application_capabilities.csv');links=rows(z,root,'core/recipe_patterns.csv');patterns=rows(z,root,'core/patterns.csv');tracks=rows(z,root,'core/pattern_tracks.csv');events=rows(z,root,'core/pattern_events.csv')
 rmap={r['recipe_id']:r for r in recipes};cmap={r['recipe_id']:r for r in caps};pmap={r['pattern_id']:r for r in patterns}
 tmap=defaultdict(list);emap=defaultdict(list)
 for r in tracks:tmap[r['pattern_id']].append(r)
 for r in events:emap[(r['pattern_id'],r['track_id'])].append(r)
 stats={'ignored_sampler_events':0,'ignored_unsupported_tracks':0,'ignored_unrepresentable_pitch_events':0,'recipes':[]}
 out.mkdir(parents=True,exist_ok=True);(out/'atlas_runtime_types.generated.h').write_text(render_types(),encoding='utf-8')
 for spec in SPECS:
  recipe=rmap[spec.atlas_id];cap=cmap[spec.atlas_id]
  if not recipe['publication_status'].startswith('PUBLISHED'):raise ValueError(f'{spec.atlas_id}: recipe is not published')
  if not boolean(cap['can_apply_pattern_to_internal_project']):raise ValueError(f'{spec.atlas_id}: not runtime applicable')
  rlinks=sorted((r for r in links if r['recipe_id']==spec.atlas_id),key=lambda r:integer(r['slot_order']))
  if [r['slot_id'] for r in rlinks]!=['P1','P2','P3']:raise ValueError(f'{spec.atlas_id}: must contain P1/P2/P3')
  compiled={};src_count=runtime_count=sampler_count=unrepr=unsupported=0
  for link in rlinks:
   pid=link['pattern_id'];p=pmap[pid]
   if integer(p['bars'])!=1 or integer(p['steps_per_bar'])!=16:raise ValueError(f'unsupported pattern shape: {pid}')
   if not p['publication_status'].startswith('PUBLISHED'):raise ValueError(f'pattern is not published: {pid}')
   merged={}
   for tr in sorted(tmap[pid],key=lambda r:integer(r['track_order'])):
    track=tr['track_id'];tev=emap[(pid,track)];src_count+=len(tev)
    if track=='SAMPLER':sampler_count+=len(tev);continue
    if track not in TARGETS:unsupported+=len(tev);continue
    for raw in tev:
     ev=compile_event(raw,track)
     if ev is None:unrepr+=1;continue
     key=(ev[0],ev[1]);prev=merged.get(key)
     if prev is None or ev[7]>=prev[7]:merged[key]=ev
   compiled[pid]=sorted(merged.values(),key=lambda e:(e[0],e[1]));runtime_count+=len(compiled[pid])
  (out/spec.filename).write_text(render_recipe(spec,recipe,rlinks,compiled),encoding='utf-8')
  stats['ignored_sampler_events']+=sampler_count;stats['ignored_unsupported_tracks']+=unsupported;stats['ignored_unrepresentable_pitch_events']+=unrepr
  stats['recipes'].append({'atlas_recipe_id':spec.atlas_id,'runtime_recipe_id':spec.runtime_id,'display_name':recipe['display_name'].replace(' SEQTRAK recipe',''),'bpm':integer(recipe['default_bpm']),'swing_percent':integer(recipe['swing_percent']),'source_event_count':src_count,'runtime_event_count':runtime_count,'ignored_sampler_event_count':sampler_count,'ignored_unsupported_track_events':unsupported,'ignored_unrepresentable_pitch_events':unrepr,'slots':[{'slot_id':l['slot_id'],'slot_function':l['slot_function'],'pattern_id':l['pattern_id']} for l in rlinks]})
 (out/'atlas_runtime.generated.h').write_text(render_index(stats),encoding='utf-8')
 return stats

def main():
 ap=argparse.ArgumentParser();ap.add_argument('atlas_zip',type=Path);ap.add_argument('output_dir',type=Path);a=ap.parse_args();stats=compile_all(a.atlas_zip,a.output_dir);print(json.dumps(stats,indent=2))
if __name__=='__main__':main()
