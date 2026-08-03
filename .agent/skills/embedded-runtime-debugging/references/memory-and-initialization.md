# Memory And Initialization

## Contents

- Memory vocabulary
- Measurement workflow
- Initialization order
- Task stacks
- Fragmentation-resistant design
- Object and ELF inspection
- Failure patterns

## Memory Vocabulary

Keep these budgets separate:

- `.data`/`.bss`: static RAM reserved before runtime;
- general heap: dynamic allocations and C++ containers;
- largest contiguous block: maximum currently satisfiable allocation;
- task stack and TCB: often require one or more contiguous capability-compatible
  allocations;
- DMA/internal/8-bit capable memory: allocator capabilities can make nominally
  free memory unusable for a driver;
- PSRAM: useful for bulk data but not a substitute for internal/DMA memory;
- stack high-water mark: remaining task stack, not system heap.

Always record total free and largest block together. A stable total with a falling
largest block is fragmentation, not a leak.

On ESP32, instrument relevant capabilities explicitly:

```cpp
const auto caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
const size_t free = heap_caps_get_free_size(caps);
const size_t largest = heap_caps_get_largest_free_block(caps);
```

Measure before and after each major boot stage and before every failed allocation.
Log the requested allocation size when known.

## Treat Initialization Order As A Resource Contract

Initialization order changes which contiguous blocks remain available. Use this
default ordering unless hardware constraints require another:

1. start the framework and register descriptor-owning interfaces;
2. reserve critical driver/DMA buffers and task stacks;
3. start realtime tasks;
4. mount storage and initialize parsers/caches;
5. initialize engines and load persistent state;
6. construct optional UI/pages and scan user content lazily.

Register USB classes before `USB.begin()` or the equivalent descriptor finalizer.
Reserve tasks that must always exist before storage/UI/container initialization can
fragment the heap. Log boot-stage edges so reset location survives reboot.

Do not move an initialization step merely to make an allocation pass. Verify its
dependencies, concurrency, and whether callbacks can run before dependent objects
are ready.

## Make Task Creation Deterministic

Task creation can fail even when total free heap exceeds the requested stack.
Account for alignment, TCB, allocator capabilities, and fragmentation.

For mandatory long-lived tasks, consider static allocation:

```cpp
static StaticTask_t taskControl;
static StackType_t taskStack[kStackWords];

TaskHandle_t handle = xTaskCreateStaticPinnedToCore(
    taskMain, "Worker", kStackWords, nullptr, priority,
    taskStack, &taskControl, core);
```

Confirm whether the API stack size is expressed in bytes or words for the selected
framework. Measure high-water marks under worst-case workload before shrinking a
stack. Static stacks trade flexible heap for predictable `.bss`; compare linker
DRAM before and after.

## Avoid Runtime Fragmentation

- Replace unbounded vectors/strings in hot lifecycles with fixed-capacity storage.
- Keep only a visible window of directory entries; rescan for pagination.
- Construct optional pages/dialogs lazily and avoid eager list/name generation.
- Preallocate repeated buffers during controlled initialization.
- Close file/driver handles promptly; their wrappers may own heap allocations.
- Avoid repeatedly constructing differently sized temporary strings and containers.
- Prefer one fixed shared pool over maximum-size buffers per possible consumer.

Reuse storage when two states cannot coexist. Make the lifecycle explicit and
guard future type changes:

```cpp
using BrowserWindow = std::array<Entry, 8>;

union Workspace {
    BrowserWindow browser;
    ScanResult scan;
    Workspace() : browser{} {}
};

static_assert(sizeof(ScanResult) <= sizeof(BrowserWindow));
static_assert(std::is_trivially_destructible_v<BrowserWindow>);
static_assert(std::is_trivially_destructible_v<ScanResult>);
```

Use placement construction when switching active members. Reconstruct the browser
window when returning from scan mode. Do not use this pattern for concurrently
needed or nontrivially owned state without a correct destructor/variant lifecycle.

## Pin Size Contracts

Use compile-time limits for objects whose size protects runtime headroom:

```cpp
static_assert(sizeof(ProjectPage) <= 256,
              "Page must leave contiguous heap for filesystem operations");
```

Choose the limit from measured allocator requirements, not aesthetics. A total
heap budget is insufficient when a later operation requires a specific contiguous
block.

## Inspect The Binary

Use all three views:

1. build output for total static RAM and flash;
2. map/`nm --size-sort` for large symbols and linked components;
3. DWARF or a small host `sizeof` program for C++ object sizes.

Examples:

```bash
xtensa-esp32s3-elf-nm -S --size-sort firmware.elf | tail -n 80
xtensa-esp32s3-elf-size -A firmware.elf
readelf --debug-dump=info firmware.elf | less
```

Search the map for unexpectedly linked Wi-Fi/Bluetooth stacks, frame buffers,
large lookup tables, task stacks, and duplicated caches. Do not infer runtime heap
from ELF size alone; validate the post-init runtime state on hardware.

## Recognize Failure Patterns

| Evidence | Likely failure | Next measurement |
|---|---|---|
| Task creation fails; total free looks adequate | fragmentation/capability mismatch | largest block and requested stack/TCB |
| Optional page creation breaks filesystem open | page/container allocation consumed contiguous block | object size and largest before/after page |
| Free heap recovers after page exit | lifecycle allocation, not leak | identify eager fields/temporaries |
| Static RAM rises but boot becomes stable | deterministic reservation replaced fragmented allocation | compare runtime headroom and high-water |
| Files vanish while card remains mounted | `File`/directory open allocation failed | mounted, exists, open, iteration as separate states |
