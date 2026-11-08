#include "../platform_sdl/arduino_compat.h"
#include "../src/audio/pattern_paging.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

SerialMock Serial;
SDMock SD;

namespace {
constexpr char kMagic[4] = {'G','P','P','G'};
constexpr uint32_t kCrcInitial = 0xFFFFFFFFu;
constexpr uint32_t kCrcPolynomial = 0xEDB88320u;
constexpr int kPage = 4;
struct HeaderV3 { char magic[4]; uint16_t version; uint16_t headerSize; uint32_t payloadSize; uint32_t payloadCrc32; uint32_t layoutFingerprint; uint32_t synthABytes; uint32_t synthBBytes; uint32_t drumBytes; };
struct HeaderV4 : HeaderV3 { uint32_t materialKindBytes; };
uint32_t update(uint32_t crc,const uint8_t* data,size_t n){for(size_t i=0;i<n;++i){crc^=data[i];for(int b=0;b<8;++b){const uint32_t m=0u-(crc&1u);crc=(crc>>1u)^(kCrcPolynomial&m);}}return crc;}
uint32_t finish(uint32_t crc){return crc^0xFFFFFFFFu;}
uint32_t layoutFingerprint(){const uint32_t values[]={static_cast<uint32_t>(sizeof(DrumStep)),static_cast<uint32_t>(sizeof(DrumPatternSet)),static_cast<uint32_t>(sizeof(SynthStep)),static_cast<uint32_t>(sizeof(SynthPattern)),static_cast<uint32_t>(sizeof(Bank<DrumPatternSet>)),static_cast<uint32_t>(sizeof(Bank<SynthPattern>)),static_cast<uint32_t>(kBankCount),static_cast<uint32_t>(Bank<SynthPattern>::kPatterns),static_cast<uint32_t>(DrumPatternSet::kVoices),static_cast<uint32_t>(DrumPattern::kSteps),static_cast<uint32_t>(SynthPattern::kSteps)};uint32_t h=2166136261u;for(uint32_t v:values){for(int byte=0;byte<4;++byte){h^=static_cast<uint8_t>((v>>(byte*8))&0xFFu);h*=16777619u;}}return h;}
uint32_t banksCrc(const Scene& s){uint32_t c=kCrcInitial;c=update(c,reinterpret_cast<const uint8_t*>(s.synthABanks),sizeof(s.synthABanks));c=update(c,reinterpret_cast<const uint8_t*>(s.synthBBanks),sizeof(s.synthBBanks));c=update(c,reinterpret_cast<const uint8_t*>(s.drumBanks),sizeof(s.drumBanks));return c;}
std::filesystem::path pathFor(const std::filesystem::path& root,const char* project){char name[32];std::snprintf(name,sizeof(name),"page_%02d.gpp",kPage);return root/"patterns"/project/name;}
void writeBanks(std::ofstream& out,const Scene& s){out.write(reinterpret_cast<const char*>(s.synthABanks),sizeof(s.synthABanks));out.write(reinterpret_cast<const char*>(s.synthBBanks),sizeof(s.synthBBanks));out.write(reinterpret_cast<const char*>(s.drumBanks),sizeof(s.drumBanks));}
void assertAllIdsInvalid(const Scene& s){for(int v=0;v<Scene::kMaterialVoices;++v)for(int slot=0;slot<Scene::kMaterialSlotsPerVoice;++slot)assert(!s.materialSlots[v][slot].id.valid());}

void v3(const std::filesystem::path& root){constexpr char project[]="c1a-legacy-v3";assert(PatternPagingService::setProjectName(project));assert(PatternPagingService::clearProjectPages());Scene source{};source.synthABanks[1].patterns[6].steps[12].note=53;HeaderV3 h{};std::memcpy(h.magic,kMagic,4);h.version=PatternPagingService::kLegacyFormatVersion;h.headerSize=sizeof(h);h.payloadSize=sizeof(source.synthABanks)+sizeof(source.synthBBanks)+sizeof(source.drumBanks);h.payloadCrc32=finish(banksCrc(source));h.layoutFingerprint=layoutFingerprint();h.synthABytes=sizeof(source.synthABanks);h.synthBBytes=sizeof(source.synthBBanks);h.drumBytes=sizeof(source.drumBanks);auto p=pathFor(root,project);std::filesystem::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(&h),sizeof(h));writeBanks(out,source);out.close();Scene loaded{};loaded.materialSlots[0][0].id=GroovePuterMaterial::MaterialId{99};assert(PatternPagingService::loadPage(kPage,loaded));assertAllIdsInvalid(loaded);}

void v4(const std::filesystem::path& root){constexpr char project[]="c1a-legacy-v4";assert(PatternPagingService::setProjectName(project));assert(PatternPagingService::clearProjectPages());Scene source{};constexpr size_t count=Scene::kMaterialVoices*Scene::kMaterialSlotsPerVoice;uint8_t kinds[count]{};kinds[Scene::kMaterialSlotsPerVoice+7]=static_cast<uint8_t>(GroovePuterMaterial::MaterialKind::Melody);uint32_t c=banksCrc(source);c=update(c,kinds,sizeof(kinds));HeaderV4 h{};std::memcpy(h.magic,kMagic,4);h.version=PatternPagingService::kKindOnlyFormatVersion;h.headerSize=sizeof(h);h.payloadSize=sizeof(source.synthABanks)+sizeof(source.synthBBanks)+sizeof(source.drumBanks)+sizeof(kinds);h.payloadCrc32=finish(c);h.layoutFingerprint=layoutFingerprint();h.synthABytes=sizeof(source.synthABanks);h.synthBBytes=sizeof(source.synthBBanks);h.drumBytes=sizeof(source.drumBanks);h.materialKindBytes=sizeof(kinds);auto p=pathFor(root,project);std::filesystem::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(&h),sizeof(h));writeBanks(out,source);out.write(reinterpret_cast<const char*>(kinds),sizeof(kinds));out.close();Scene loaded{};loaded.materialSlots[1][7].id=GroovePuterMaterial::MaterialId{123};assert(PatternPagingService::loadPage(kPage,loaded));assert(loaded.materialSlots[1][7].kind==GroovePuterMaterial::MaterialKind::Melody);assertAllIdsInvalid(loaded);}
}
int main(){const auto root=std::filesystem::temp_directory_path()/"grooveputer-c1a-material-id-legacy";std::error_code ec;std::filesystem::remove_all(root,ec);std::filesystem::create_directories(root);SD.setRoot(root);v3(root);v4(root);std::puts("C1A legacy MaterialId compatibility: PASS");return 0;}
