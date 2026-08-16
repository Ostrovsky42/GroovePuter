#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <malloc.h>
#include <unistd.h>

#include "src/sampler/ram_sample_store.h"
#include "src/sampler/sample_index.h"

namespace {

std::size_t allocatedHeapBytes() {
  malloc_trim(0);
  const struct mallinfo2 info = mallinfo2();
  return static_cast<std::size_t>(info.uordblks);
}

std::filesystem::path createSyntheticCatalog(std::size_t fileCount) {
  char pathTemplate[] = "/tmp/grooveputer_sampler_memory_XXXXXX";
  char* created = mkdtemp(pathTemplate);
  assert(created != nullptr);

  const std::filesystem::path root(created);
  for (std::size_t i = 0; i < fileCount; ++i) {
    char filename[96];
    std::snprintf(filename, sizeof(filename),
                  "sample_%04zu_long_identity_for_memory_audit.wav", i);
    const auto path = root / filename;
    FILE* file = std::fopen(path.c_str(), "wb");
    assert(file != nullptr);
    std::fclose(file);
  }
  return root;
}

struct Measurement {
  std::size_t files = 0;
  std::size_t baseline = 0;
  std::size_t afterScan = 0;
  std::size_t afterBind = 0;
  std::size_t afterDestroy = 0;
};

Measurement measure(std::size_t fileCount) {
  const auto root = createSyntheticCatalog(fileCount);
  const std::string rootPath = root.string();

  Measurement result{};
  result.files = fileCount;
  result.baseline = allocatedHeapBytes();

  {
    SampleIndex index;
    index.scanDirectory(rootPath);
    assert(index.getFiles().size() == fileCount);
    result.afterScan = allocatedHeapBytes();

    RamSampleStore store;
    const auto bind = index.bindToStore(store);
    assert(bind.discovered == fileCount);
    assert(bind.registered == fileCount);
    assert(bind.rejectedStable == 0);
    assert(bind.rejectedStore == 0);
    result.afterBind = allocatedHeapBytes();
  }

  result.afterDestroy = allocatedHeapBytes();
  std::filesystem::remove_all(root);
  return result;
}

std::size_t positiveDelta(std::size_t after, std::size_t before) {
  return after >= before ? after - before : 0;
}

void printMeasurement(const Measurement& m) {
  std::printf(
      "catalog_files=%zu scan_delta=%zu bind_added_delta=%zu "
      "combined_delta=%zu destroy_residual=%zu\n",
      m.files,
      positiveDelta(m.afterScan, m.baseline),
      positiveDelta(m.afterBind, m.afterScan),
      positiveDelta(m.afterBind, m.baseline),
      positiveDelta(m.afterDestroy, m.baseline));
}

}  // namespace

int main() {
  std::printf("SAMPLER_MEMORY_DYNAMIC_BEGIN\n");

  const Measurement empty = measure(0);
  const Measurement library172 = measure(172);
  const Measurement library500 = measure(500);

  printMeasurement(empty);
  printMeasurement(library172);
  printMeasurement(library500);

  const std::size_t scan172 = positiveDelta(library172.afterScan, library172.baseline);
  const std::size_t scan500 = positiveDelta(library500.afterScan, library500.baseline);
  const std::size_t bind172 = positiveDelta(library172.afterBind, library172.afterScan);
  const std::size_t bind500 = positiveDelta(library500.afterBind, library500.afterScan);

  assert(scan172 > 0);
  assert(scan500 > scan172);
  assert(bind172 > 0);
  assert(bind500 > bind172);

  std::printf("SAMPLER_MEMORY_DYNAMIC_END\n");
  return 0;
}
