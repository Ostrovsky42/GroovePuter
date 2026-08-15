#include "../src/sampler/sample_index.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using GroovePuterSampler::SampleRef;
using GroovePuterSampler::canonicalSampleKey;
using GroovePuterSampler::stableSampleRefForPath;

namespace {

void touch(const fs::path& path) {
  std::ofstream out(path, std::ios::binary);
  out.put('\0');
}

void testCanonicalMountAlias() {
  assert(canonicalSampleKey("/sd/samples/kick.wav") == "samples/kick.wav");
  assert(canonicalSampleKey("/samples/kick.wav") == "samples/kick.wav");
  assert(canonicalSampleKey("\\sd\\samples\\kick.wav") == "samples/kick.wav");
  assert(canonicalSampleKey("//sd//samples///kick.wav") == "samples/kick.wav");
  assert(canonicalSampleKey("/sd/./samples/kick.wav") == "samples/kick.wav");

  const SampleRef a = stableSampleRefForPath("/sd/samples/kick.wav");
  const SampleRef b = stableSampleRefForPath("/samples/kick.wav");
  assert(a.valid());
  assert(a == b);
}

void testDuplicateBasenameNoLongerCollides() {
  const SampleRef kitA = stableSampleRefForPath("/samples/kit-a/kick.wav");
  const SampleRef kitB = stableSampleRefForPath("/samples/kit-b/kick.wav");
  assert(kitA.valid());
  assert(kitB.valid());
  assert(kitA != kitB);

  const uint32_t legacyA = SampleIndex::calculateHash("kick.wav");
  const uint32_t legacyB = SampleIndex::calculateHash("kick.wav");
  assert(legacyA == legacyB);
}

void testIndexIdentityIsIndependentOfDirectoryEnumerationOrder() {
  const fs::path root =
      fs::temp_directory_path() / "grooveputer_sample_ref_contract";
  const fs::path dirA = root / "a";
  const fs::path dirB = root / "b";
  fs::remove_all(root);
  fs::create_directories(dirA);
  fs::create_directories(dirB);

  // Deliberately create the files in opposite order in the two directories.
  touch(dirA / "snare.wav");
  touch(dirA / "kick.wav");
  touch(dirB / "kick.wav");
  touch(dirB / "snare.wav");

  SampleIndex indexA;
  SampleIndex indexB;
  indexA.scanDirectory(dirA.string());
  indexB.scanDirectory(dirB.string());

  assert(indexA.getFiles().size() == 2);
  assert(indexB.getFiles().size() == 2);

  const SampleRef kickA = indexA.findRefByFilename("kick.wav");
  const SampleRef snareA = indexA.findRefByFilename("snare.wav");
  const SampleRef kickB = indexB.findRefByFilename("kick.wav");
  const SampleRef snareB = indexB.findRefByFilename("snare.wav");

  assert(kickA.valid());
  assert(snareA.valid());
  assert(kickB.valid());
  assert(snareB.valid());

  // The folder is part of the stable logical key, so different folders are
  // intentionally different refs even when basenames match.
  assert(kickA != kickB);
  assert(snareA != snareB);

  // Re-scanning the same folder keeps the exact refs regardless of the sorted
  // UI order that follows discovery.
  const SampleRef kickABefore = kickA;
  const SampleRef snareABefore = snareA;
  indexA.scanDirectory(dirA.string());
  assert(indexA.findRefByFilename("kick.wav") == kickABefore);
  assert(indexA.findRefByFilename("snare.wav") == snareABefore);

  fs::remove_all(root);
}

void testLegacyFallbackResolvesKnownFile() {
  const fs::path root =
      fs::temp_directory_path() / "grooveputer_sample_ref_legacy";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / "kick.wav");
  touch(root / "snare.wav");

  SampleIndex index;
  index.scanDirectory(root.string());

  const SampleId kickLegacy{SampleIndex::calculateHash("kick.wav")};
  const SampleRef kickRef = index.findRefByFilename("kick.wav");
  assert(kickRef.valid());
  assert(index.resolveLegacyId(kickLegacy) == kickRef);
  assert(index.findByRef(kickRef) != nullptr);
  assert(index.findByRef(kickRef)->filename() == "kick.wav");

  const SampleId missing{SampleIndex::calculateHash("missing.wav")};
  assert(!index.resolveLegacyId(missing).valid());
  assert(index.findByRef(SampleRef{}) == nullptr);

  fs::remove_all(root);
}

void testLegacyHashCollisionFailsClosed() {
  // These two distinct filenames are a real FNV-1a32 collision. They model the
  // exact failure class that basename-only persistence could not disambiguate.
  constexpr const char* kNameA = "5oetw2k1.wav";
  constexpr const char* kNameB = "qp363n87.wav";
  const uint32_t hashA = SampleIndex::calculateHash(kNameA);
  const uint32_t hashB = SampleIndex::calculateHash(kNameB);
  assert(hashA == hashB);

  const fs::path root =
      fs::temp_directory_path() / "grooveputer_sample_ref_collision";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / kNameA);
  touch(root / kNameB);

  SampleIndex index;
  index.scanDirectory(root.string());
  assert(index.getFiles().size() == 2);

  const SampleRef refA = index.findRefByFilename(kNameA);
  const SampleRef refB = index.findRefByFilename(kNameB);
  assert(refA.valid());
  assert(refB.valid());
  assert(refA != refB);

  const SampleId ambiguousLegacy{hashA};
  assert(!index.resolveLegacyId(ambiguousLegacy).valid());

  fs::remove_all(root);
}

}  // namespace

int main() {
  static_assert(sizeof(SampleRef) == 8, "SampleRef ABI changed");
  testCanonicalMountAlias();
  testDuplicateBasenameNoLongerCollides();
  testIndexIdentityIsIndependentOfDirectoryEnumerationOrder();
  testLegacyFallbackResolvesKnownFile();
  testLegacyHashCollisionFailsClosed();
  return 0;
}
