import json,hashlib,sys
from pathlib import Path
source=Path(sys.argv[1]);destination=Path(sys.argv[2]);destination.mkdir(exist_ok=True)
def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(",",":")).encode()).hexdigest()
results=[]
for p in sorted(source.glob("g*-s*/g2-case.json")):
 j=json.loads(p.read_text());song=j["songs"][0];bars=[]
 for index,refs in enumerate(song["positions"][1:song["length"]],1):
  events={}
  for role,banks in (("a","synthABanks"),("b","synthBBanks"),("drums","drumBanks")):
   slot=refs[role];events[role]=None if slot<0 else j[banks][slot//8][slot%8]
  bars.append({"song_row":index,"refs":refs,"events":events,"event_sha256":digest(events)})
 sections={name:digest([bar["events"] for bar in bars[start:start+4]]) for name,start in (("take",0),("develop",4),("break",8),("return",12)) if len(bars)>=start+4}
 record={"case":p.parent.name,"source_project_sha256":hashlib.sha256(p.read_bytes()).hexdigest(),"bars":bars,"section_hashes":sections}
 (destination/(p.parent.name+".json")).write_text(json.dumps(record,sort_keys=True,indent=2)+"\n")
 if len(bars)==16:assert sections["take"]==sections["return"]
 results.append({"case":p.parent.name,"bars":len(bars),"section_hashes":sections,"develop_differs":sections.get("develop")!=sections["take"] if "develop" in sections else None,"break_differs":sections.get("break")!=sections["take"] if "break" in sections else None})
(destination/"summary.json").write_text(json.dumps(results,indent=2)+"\n")
print(json.dumps(results,indent=2))
