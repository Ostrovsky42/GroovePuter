#include <bits/stdc++.h>
#include "src/audio/pattern_paging.cpp"
SerialMock Serial; SDMock SD;
namespace {
void writePageFile(const std::string& path,const Scene& scene,int version,uint32_t generation=1){
 uint8_t kinds[kMaterialSlotCount]{};GroovePuterMaterial::MaterialId ids[kMaterialSlotCount]{};
 collectMaterialKinds(scene,kinds);collectMaterialIds(scene,ids);
 std::vector<uint8_t> payload;
 auto append=[&](const void* data,size_t n){const auto* b=static_cast<const uint8_t*>(data);payload.insert(payload.end(),b,b+n);};
 append(scene.synthABanks,sizeof(scene.synthABanks));append(scene.synthBBanks,sizeof(scene.synthBBanks));append(scene.drumBanks,sizeof(scene.drumBanks));
 if(version>=4) append(kinds,sizeof(kinds));if(version>=5) append(ids,sizeof(ids));
 auto h=makeHeader(scene,generation);h.version=version;
 h.headerSize=version==3?sizeof(PageFileHeaderV3):version==4?sizeof(PageFileHeaderV4):version==5?sizeof(PageFileHeaderV5):sizeof(PageFileHeaderV6);
 h.payloadSize=payload.size();h.payloadCrc32=finalizeCrc(crc32Update(kCrcInitial,payload.data(),payload.size()));
 SD.remove(path.c_str());
 File file=SD.open(path.c_str(),FILE_WRITE);assert(file);assert(writeAll(file,&h,h.headerSize));assert(writeAll(file,payload.data(),payload.size()));file.close();
}
}
int main(){
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("gp-g6-page-meta-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root);SD.setRoot(root);assert(PatternPagingService::setProjectName("metadata"));
 Scene source{};source.synthABanks[0].patterns[0].steps[0].note=42;
 source.materialSlots[0][3].kind=GroovePuterMaterial::MaterialKind::Melody;source.materialSlots[0][3].id.value=101;
 for(int version=3;version<=6;++version){
  writePageFile(pagePathFor("metadata",2),source,version);
  auto& scratch=sceneTransactionScratch();scratch.synthABanks[0].patterns[0].steps[0].note=77;
  std::vector<uint8_t> before(sizeof(Scene));std::memcpy(before.data(),&scratch,sizeof(Scene));const int page=PatternPagingService::activePageIndex();
  PatternPagingService::PageMaterialDescriptors meta{};assert(PatternPagingService::readPageMaterialDescriptors(2,meta));
  assert(meta[3].kind==(version>=4?GroovePuterMaterial::MaterialKind::Melody:GroovePuterMaterial::MaterialKind::Pattern));
  assert(meta[3].id.value==(version>=5?101u:0u));
  assert(std::memcmp(before.data(),&scratch,sizeof(Scene))==0);assert(PatternPagingService::activePageIndex()==page);
  Scene loaded{};assert(PatternPagingService::loadPage(2,loaded,false));
  for(int v=0;v<Scene::kMaterialVoices;++v) for(int i=0;i<Scene::kMaterialSlotsPerVoice;++i){
   const auto& expected=loaded.materialSlots[v][i];const auto& actual=meta[v*Scene::kMaterialSlotsPerVoice+i];assert(actual.kind==expected.kind && actual.id==expected.id);
  }
  printf("G6 metadata v%d: same descriptors, scratch/index untouched: PASS\n",version);
 }
 const auto pathA=slotPathFor("metadata",2,GroovePuterMaterial::PublicationSlot::SlotA);
 const auto pathB=slotPathFor("metadata",2,GroovePuterMaterial::PublicationSlot::SlotB);
 writePageFile(pathB,source,6,2);
 int readsA=0;
 const bool loaded=readPageCandidate(2,[&](const std::string& path,uint32_t* generation){
  if(path==pathA){++readsA;if(generation)*generation=1;return readsA==1;}
  return false;
 });
 printf("G6 fallback reread failure: loaded=%d readsA=%d\n",loaded,readsA);fflush(stdout);
 assert(!loaded && readsA==2);
 Scene newer=source;newer.materialSlots[0][3].id.value=202;
 writePageFile(pathA,source,6,1);writePageFile(pathB,newer,6,2);
 PatternPagingService::PageMaterialDescriptors selected{};
 assert(PatternPagingService::readPageMaterialDescriptors(2,selected));assert(selected[3].id.value==202);
 GroovePuterMaterial::MaterialPublicationRecord publication{};
 publication.pageIndex=2;publication.slot=static_cast<uint8_t>(GroovePuterMaterial::PublicationSlot::SlotA);
 publication.storageGeneration=1;publication.checksum=GroovePuterMaterial::checksumPublicationRecord(publication);
 assert(GroovePuterPlatform::saveMaterialPublication("metadata",2,publication));
 assert(PatternPagingService::readPageMaterialDescriptors(2,selected));assert(selected[3].id.value==101);
 auto breakFile=[&](const std::string& path){SD.remove(path.c_str());File file=SD.open(path.c_str(),FILE_WRITE);const uint8_t bad[]={1,2,3};assert(file.write(bad,sizeof(bad))==sizeof(bad));};
 breakFile(pathA);
 assert(PatternPagingService::readPageMaterialDescriptors(2,selected));assert(selected[3].id.value==202);
 breakFile(pathB);
 assert(PatternPagingService::readPageMaterialDescriptors(2,selected));assert(selected[3].id.value==101);
 SD.remove(pagePathFor("metadata",2).c_str());
 assert(!PatternPagingService::readPageMaterialDescriptors(2,selected));assert(selected[3].id.value==101);
 puts("G6 metadata generation/NVS/alternate/legacy/error preservation: PASS");
}
