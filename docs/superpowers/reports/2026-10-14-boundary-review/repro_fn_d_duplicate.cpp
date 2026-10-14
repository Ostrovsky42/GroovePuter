#include <cassert>
#include <cstdio>
#include <vector>
#include "src/input/cardputer_input_edges.h"
struct K { bool alt=false,ctrl=false,shift=false,fn=false; std::vector<unsigned char> hid_keys; std::vector<char> word; };
int main(){ K old{},now{}; now.fn=true; now.hid_keys={7}; now.word={'d'}; unsigned n=0; unsigned mask=0;
 if(GroovePuterInput::shouldDispatchHid(now,old,true,7)) ++n;
 char c='d'; if(GroovePuterInput::shouldDispatchWord(now,old,true,c) && !GroovePuterInput::wordLetterAlreadyDispatched(c,mask) && !now.ctrl && !now.alt) ++n;
 std::printf("Fn+D dispatch count=%u\n",n); std::fflush(stdout); assert(n==1);
}
