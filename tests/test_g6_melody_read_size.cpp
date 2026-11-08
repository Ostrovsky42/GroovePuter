#include <bits/stdc++.h>
#include "src/state/melody_promotion.h"
SerialMock Serial; SDMock SD;
int main(){
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("gp-g6-read-bound-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root);SD.setRoot(root);
 constexpr size_t maximum=MelodyStore::kHeaderBytesV2+PhraseRuntime::kMaxSynthEvents*MelodyStore::kEventBytes;
 MelodyPromotion::SdFileSystem reader;
 for(size_t length : {maximum+1,size_t(65536),size_t(0),MelodyStore::kHeaderBytesV1-1}){
  const auto path=root/"corrupt.gpml";
  {std::ofstream file(path,std::ios::binary);if(length){file.seekp(length-1);file.put('x');}}
  std::vector<uint8_t> out{1,2,3};const auto capacity=out.capacity();
  const bool loaded=reader.read("/corrupt.gpml",out);
  printf("G6_SD_READ size=%zu accepted=%d capacity=%zu\n",length,loaded,out.capacity());
  fflush(stdout);assert(!loaded);assert(out==std::vector<uint8_t>({1,2,3}));assert(out.capacity()==capacity);
 }
 PhraseRuntime::RuntimeSynthEventBuffer melody{};melody.lengthTicks=8*PhraseRuntime::kTicksPerBar;melody.count=128;
 for(int i=0;i<128;++i){auto& e=melody.events[i];e.startTick=i*24;e.durationSubticks=128;e.note=60;e.velocity=100;e.probability=100;}
 std::vector<uint8_t> encoded;assert(MelodyStore::encode(melody,encoded));assert(encoded.size()==maximum);
 assert(reader.write("/valid.gpml",encoded.data(),encoded.size()));
 std::vector<uint8_t> loaded;assert(reader.read("/valid.gpml",loaded));
 PhraseRuntime::RuntimeSynthEventBuffer decoded{};assert(MelodyStore::decode(loaded.data(),loaded.size(),decoded));assert(decoded.count==128);
 // The smaller v1 header remains accepted; downgrade the v2 encoding.
 encoded.erase(encoded.begin()+16,encoded.begin()+20);encoded[4]=1;encoded[6]=16;
 assert(reader.write("/legacy.gpml",encoded.data(),encoded.size()));
 assert(reader.read("/legacy.gpml",loaded));assert(MelodyStore::decode(loaded.data(),loaded.size(),decoded));
 PhraseRuntime::RuntimeSynthEventBuffer empty{};assert(MelodyStore::encode(empty,encoded));
 encoded.erase(encoded.begin()+16,encoded.begin()+20);encoded[4]=1;encoded[6]=16;
 assert(encoded.size()==MelodyStore::kHeaderBytesV1);
 assert(reader.write("/empty-v1.gpml",encoded.data(),encoded.size()));
 assert(reader.read("/empty-v1.gpml",loaded));assert(MelodyStore::decode(loaded.data(),loaded.size(),decoded));assert(decoded.count==0);
 puts("G6 bounded SD Melody reads: PASS (max v2 and legacy v1)");
}
