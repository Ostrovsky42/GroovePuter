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

constexpr char kMagic[4] = {'G', 'P', 'P', 'G'};
constexpr uint32_t kCrcInitial = 0xFFFFFFFFu;
constexpr uint32_t kCrcPolynomial = 0xEDB88320u;
constexpr int kPage = 4;

struct PageFileHeaderV3 {
  char magic[4];
  uint16_t version;
  uint16_t headerSize;
  uint32_t payloadSize;
  uint32_t payloadCrc32;
  uint32_t layoutFingerprint;
  uint32_t synthABytes;
  uint32_t synthBBytes;
  uint32_t drumBytes;
};

struct PageFileHeaderV4 : PageFileHeaderV3 {
  uint32_t materialKindBytes;
};

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (kCrcPolynomial & mask);
    }
  }
  return crc;
}

uint32_t finalizeCrc(uint32_t crc) { return crc ^ 0xFFFFFFFFu; }

uint32_t layoutFingerprint() {
  const uint32_t values[] = {
      static_cast<uint32_t>(sizeof(DrumStep)),
      static_cast<uint32_t>(sizeof(DrumPatternSet)),
      static_cast<uint32_t>(sizeof(SynthStep)),
      static_cast<uint32_t>(sizeof(SynthPattern)),
      static_cast<uint32_t>(sizeof(Bank<DrumPatternSet>)),
      static_cast<uint32_t>(sizeof(Bank<SynthPattern>)),
      static_cast<uint32_t>(kBankCount),
      static_cast<uint32_t>(Bank<SynthPattern>::kPatterns),
      static_cast<uint32_t>(DrumPatternSet::kVoices),
      static_cast<uint32_t>(DrumPattern::kSteps),
      static_cast<uint32_t>(SynthPattern::kSteps),
  };

  uint32_t hash = 2166136261u;
  for (uint32_t value : values) {
    for (int byte = 0; byte < 4; ++byte) {
      hash ^= static_cast<uint8_t>((value >> (byte * 8)) & 0xFFu);
      hash *= 16777619u;
    }
  }
  return hash;
}

std::filesystem::path pagePath(const std::filesystem::path& root,
                               const char* project) {
  char name[32];
  std::snprintf(name, sizeof(name), "page_%02d.gpp", kPage);
  return root / "patterns" / project / name;
}

void setMarker(Scene& scene, int note) {
  scene.synthABanks[1].patterns[6].steps[12].note =
      static_cast<int8_t>(note);
}

void writeBanks(std::ofstream& out, const Scene& scene) {
  out.write(reinterpret_cast<const char*>(scene.synthABanks),
            sizeof(scene.synthABanks));
  out.write(reinterpret_cast<const char*>(scene.synthBBanks),
            sizeof(scene.synthBBanks));
  out.write(reinterpret_cast<const char*>(scene.drumBanks),
            sizeof(scene.drumBanks));
}

uint32_t banksCrc(const Scene& scene) {
  uint32_t crc = kCrcInitial;
  crc = crc32Update(crc,
                    reinterpret_cast<const uint8_t*>(scene.synthABanks),
                    sizeof(scene.synthABanks));
  crc = crc32Update(crc,
                    reinterpret_cast<const uint8_t*>(scene.synthBBanks),
                    sizeof(scene.synthBBanks));
  crc = crc32Update(crc,
                    reinterpret_cast<const uint8_t*>(scene.drumBanks),
                    sizeof(scene.drumBanks));
  return crc;
}

void writeV3Fixture(const std::filesystem::path& root) {
  constexpr char project[] = "material-legacy-v3";
  assert(PatternPagingService::setProjectName(project));
  assert(PatternPagingService::clearProjectPages());

  Scene source{};
  setMarker(source, 53);

  PageFileHeaderV3 header{};
  std::memcpy(header.magic, kMagic, sizeof(kMagic));
  header.version = PatternPagingService::kLegacyFormatVersion;
  header.headerSize = static_cast<uint16_t>(sizeof(header));
  header.payloadSize = static_cast<uint32_t>(
      sizeof(source.synthABanks) + sizeof(source.synthBBanks) +
      sizeof(source.drumBanks));
  header.payloadCrc32 = finalizeCrc(banksCrc(source));
  header.layoutFingerprint = layoutFingerprint();
  header.synthABytes = sizeof(source.synthABanks);
  header.synthBBytes = sizeof(source.synthBBanks);
  header.drumBytes = sizeof(source.drumBanks);

  const auto path = pagePath(root, project);
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.is_open());
  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  writeBanks(out, source);
  out.close();

  Scene loaded{};
  loaded.materialSlots[0][0].kind = GroovePuterMaterial::MaterialKind::Melody;
  loaded.materialSlots[0][0].id = GroovePuterMaterial::MaterialId{99};
  assert(PatternPagingService::loadPage(kPage, loaded));
  assert(loaded.synthABanks[1].patterns[6].steps[12].note == 53);
  for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
    for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
      assert(loaded.materialSlots[voice][slot].kind ==
             GroovePuterMaterial::MaterialKind::Pattern);
      assert(!loaded.materialSlots[voice][slot].id.valid());
    }
  }
}

void writeV4Fixture(const std::filesystem::path& root) {
  constexpr char project[] = "material-legacy-v4";
  assert(PatternPagingService::setProjectName(project));
  assert(PatternPagingService::clearProjectPages());

  Scene source{};
  setMarker(source, 67);

  constexpr size_t kSlotCount =
      Scene::kMaterialVoices * Scene::kMaterialSlotsPerVoice;
  uint8_t kinds[kSlotCount]{};
  const size_t melodyIndex =
      static_cast<size_t>(1 * Scene::kMaterialSlotsPerVoice + 7);
  kinds[melodyIndex] =
      static_cast<uint8_t>(GroovePuterMaterial::MaterialKind::Melody);

  uint32_t crc = banksCrc(source);
  crc = crc32Update(crc, kinds, sizeof(kinds));

  PageFileHeaderV4 header{};
  std::memcpy(header.magic, kMagic, sizeof(kMagic));
  header.version = PatternPagingService::kKindOnlyFormatVersion;
  header.headerSize = static_cast<uint16_t>(sizeof(header));
  header.payloadSize = static_cast<uint32_t>(
      sizeof(source.synthABanks) + sizeof(source.synthBBanks) +
      sizeof(source.drumBanks) + sizeof(kinds));
  header.payloadCrc32 = finalizeCrc(crc);
  header.layoutFingerprint = layoutFingerprint();
  header.synthABytes = sizeof(source.synthABanks);
  header.synthBBytes = sizeof(source.synthBBanks);
  header.drumBytes = sizeof(source.drumBanks);
  header.materialKindBytes = sizeof(kinds);

  const auto path = pagePath(root, project);
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.is_open());
  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  writeBanks(out, source);
  out.write(reinterpret_cast<const char*>(kinds), sizeof(kinds));
  out.close();

  Scene loaded{};
  loaded.materialSlots[1][7].id = GroovePuterMaterial::MaterialId{123};
  assert(PatternPagingService::loadPage(kPage, loaded));
  assert(loaded.synthABanks[1].patterns[6].steps[12].note == 67);
  assert(loaded.materialSlots[1][7].kind ==
         GroovePuterMaterial::MaterialKind::Melody);
  for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
    for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
      assert(!loaded.materialSlots[voice][slot].id.valid());
    }
  }
}

}  // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "grooveputer-material-id-legacy-page-compat";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  SD.setRoot(root);

  writeV3Fixture(root);
  writeV4Fixture(root);

  std::filesystem::remove_all(root, ec);
  return 0;
}
