import re, sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
text=(root/'src/generation/composition/generation_profile.cpp').read_text()
block=text[text.index('constexpr ProfileDefinition kProfiles[]'):text.index('const ProfileDefinition* definitionFor')]
pairs=re.findall(r'profile\(GenerativeMode::(\w+),\s*([^,]+),',block)
assert len(pairs)==33, len(pairs)
out=Path(sys.argv[1]); out.parent.mkdir(parents=True,exist_ok=True)
out.write_text('#pragma once\nstruct Pair { unsigned mode; unsigned recipe; };\nconstexpr Pair kPairs[] = {'+','.join('{'+str(['Acid','Outrun','Darksynth','Electro','Rave','Reggae','TripHop','Broken','Chip','House','Techno','HipHop','FunkSoul','UkGarage','DrumAndBass','LoFi'].index(m))+','+({'kBaseRecipeId':'0','kClassicChillRecipeId':'12','kDrunkenGrooveRecipeId':'13','kLoFiHouseRecipeId':'14','kMinimalSleepRecipeId':'15','kGoldenEraRecipeId':'16','kDustyJazzRecipeId':'17'}.get(r.strip(),r.strip()))+'}' for m,r in pairs)+'};\n')
