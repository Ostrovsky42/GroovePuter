# 0.9.10-MEMORY-R0-C — UI PAGE RESIDENCY AND FIXED-ARENA FEASIBILITY

Status: **OPEN — retention model mapped; target-size and hardware page sweeps required**  
Authoritative source: `feature/20260904-02-0.9.10-pattern-phrase-p3-phrase-lifetime` @ `aded0e183a934f78623030226b67b5d0b598648b`  
Production semantic delta: **NONE**

## Primary question

Which UI objects are actually resident simultaneously on Cardputer, and would destroy/recreate or a fixed arena reduce INTERNAL DRAM residency versus the **current** low-memory page policy without changing UX, keyboard behavior, page semantics, or state ownership?

## Current owner topology

`MiniAcidDisplay` owns:

- one heap-allocated `MiniAcidDisplay` root object from `GroovePuter.ino`
- a heap-allocated `CassetteSkin`
- `std::vector<std::unique_ptr<IPage>> pages_`
- optional help dialog/page state
- global help/workspace overlays and UI-session state
- the active page object

The constructor performs all of the following before returning:

1. loads/sanitizes persisted UI-session state
2. creates the cassette skin
3. resizes the page pointer vector
4. creates the persisted/current page immediately through `createPage_()`
5. applies page bounds/style

Therefore the existing top-level `before MiniAcidDisplay` -> `after MiniAcidDisplay` free-heap delta is **root + skin + page-vector capacity + active page + constructor side effects**, not `sizeof(MiniAcidDisplay)` alone.

## Current page factory

The exact-head factory maps active page IDs to these concrete page types:

- `GenrePage`
- `SynthSequencerPage` Synth A
- `SynthSequencerPage` Synth B
- `DrumSequencerPage`
- `SongPage`
- `SequencerHubPage`
- `FeelTexturePage` compatibility alias -> canonical `FeelPage`
- `SettingsPage`
- `ProjectPage`
- `ModePage`
- `PerformPage`
- `PhrasePage` product mode
- `PhrasePage` core mode
- `SamplerPage`
- `SmfPlayerPage`

TAPE exists as `TapePage` in the UI codebase and must remain in the memory sweep if it is reachable in the current workflow/build configuration. The runtime factory/reachability result for TAPE must be recorded explicitly rather than assumed from the class existing in source.

## Critical finding: low-memory Cardputer already destroys/recreates pages

`MiniAcidDisplay::getPage_()` currently does:

```text
freeDRAM = free INTERNAL|8BIT
aggressive = freeDRAM < 16384
```

When a target page is missing, it walks `pages_` before construction:

- target page is kept
- when not aggressive, `previous_page_index_` may also be retained
- all other existing pages are reset
- under `aggressive == true`, **every non-target page is destroyed before the target is constructed**

With the observed post-setup floor around `700–900 B`, normal Cardputer operation is far below the `16384 B` threshold. Therefore the hypothesis “lazy pages eventually all accumulate and consume the sum of all page objects” is false for the current low-memory runtime.

### Consequence for R0-C

The CURRENT model is not simply “lazy allocated + retained”. It is:

- high-memory path: lazy target plus at most the previous cached page during missing-page construction
- low-memory path: effectively destroy/recreate one target page at a time

A proposed one-page `unique_ptr` model may simplify ownership, but **its steady-state page residency may already be close to what the hardware receives today**. A fixed arena therefore cannot be credited with the sum of all page sizes. Its potential advantage is bounded allocation/reuse and fragmentation control, not automatically a multi-page residency win.

## Static page ownership findings

### PERFORM

`PerformPage` keeps only references to `MiniAcid` and `PerformanceKeyboard`, local focus/navigation state, and a `std::string title_`. Musical values explicitly live in `PerformanceKeyboard`, not in the page. This is favorable for destroy/recreate semantics.

### MIDI PLAYER

`SmfPlayerPage` stores a pointer to the global `ISmfPlayerService`, view flags/cursors and title state. The actual playback/task/queue ownership is outside the page. Destroying the page does **not** imply that SMF playback is safe to destroy; that is R0-B's concern.

### PROJECT

`ProjectPage` has a target `static_assert(sizeof(ProjectPage) <= 256)`. It owns `std::vector<std::string> scenes_`, path/name strings and dialog state. The constructor itself does not populate the scene vector; scene browsing does so on demand. Thus object `sizeof` and first-dialog heap cost must be separated.

### PHRASE / PHRASE CORE

`PhrasePage` contains fixed bar-preview arrays plus placement/cursor/session state. The product comments explicitly distinguish page-local request/placement state from authoritative Song/Phrase data. Destruction can intentionally reset some page-local placement state, but R0-C must verify that this matches the already accepted navigation contract.

### GENRE

`GenrePage` is mostly references, scalar pending-selection state and title string. Authoritative musical state remains in the engine.

### FEEL

`FeelPage` is mostly references, scalar focus/hold state and title string. Authoritative musical state is applied to the engine; page state is primarily navigation.

### SAMPLER

`SamplerPage` has ten `std::shared_ptr<LabelValueComponent>` members. `initComponents()` executes lazily from `setBoundaries()` / first draw and allocates **ten shared component objects** via `std::make_shared`, then attaches them as child components. Each component owns two `std::string` values. Consequently:

- page `sizeof` alone materially undercounts the first-visit cost
- constructor cost and first-boundary/draw cost must be measured separately
- leaving the page in aggressive mode should destroy these shared objects if no external child owner remains; that must be verified by before/after heap recovery

### TAPE

`TapePage` embeds `WaveformVisualization`, which itself embeds `int16_t wave_data_[128]` = **256 B fixed payload**, plus eight shared component pointers and title string. As with SAMPLER, first component initialization can dominate over object `sizeof`.

## Required target-size enumeration

A diagnostic build must print target ABI sizes for every concrete `IPage` factory type. Host `sizeof` is not authoritative because STL layout and target ABI can differ.

Required output, sorted descending:

| type | `sizeof(Page)` target | fixed arrays / notable inline payload | ctor heap | first-draw/boundary heap | persistent child objects | refs vs copies | status |
|---|---:|---|---:|---:|---|---|---|
| PhrasePage | PENDING | bar-preview arrays | PENDING | PENDING | none identified statically | engine ref + local preview copies | OPEN |
| TapePage | PENDING | waveform 256 B | PENDING | PENDING | shared controls | engine/gfx refs + local UI state | OPEN |
| SamplerPage | PENDING | none large inline identified | small ctor expected | ten `make_shared` controls + strings | 10 shared controls | engine ref | OPEN |
| ProjectPage | <=256 asserted | vector/string objects | small ctor expected | dialog-dependent vectors/strings | no persistent child tree identified | engine/gfx refs | OPEN |
| SmfPlayerPage | PENDING | scalar view state | PENDING | browser-dependent | external service pointer | service ref/pointer | OPEN |
| PerformPage | PENDING | selected-row[4] | PENDING | PENDING | none identified | engine/keyboard refs | OPEN |
| GenrePage | PENDING | scalar pending state | PENDING | PENDING | none identified | engine ref | OPEN |
| FeelPage | PENDING | hold accelerator | PENDING | PENDING | none identified | engine/guard refs | OPEN |
| all other factory pages | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | OPEN |

Do not infer ordering from header size or source line count. Sort only after the target-size probe is captured.

## Hardware page-residency sweep

Use one exact diagnostic ELF and start from a stable post-setup baseline. For each reachable page, record:

1. `before first visit`
2. `after page construction / setBoundaries`
3. `after first draw`
4. `after leaving for a known small page`
5. `after revisiting`
6. largest block at every point
7. heap integrity at every point
8. loop-task stack HWM if page draw/event processing is nontrivial

Required high-priority pages:

- PERFORM
- MIDI PLAYER
- PROJECT
- PHRASE
- GENRE
- FEEL
- SAMPLER
- TAPE (if reachable in current build; otherwise record NOT REACHABLE and still keep static-size evidence)

Suggested ledger:

| page | pre-first | post-construct | post-first-draw | after-leave | after-revisit | one-time retained | temporary peak | reclaimed on leave | largest-block drift | integrity |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| PERFORM | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| MIDI PLAYER | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| PROJECT | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| PHRASE | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| GENRE | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| FEEL | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| SAMPLER | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| TAPE | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |

### Reclamation interpretation

`after leaving - before first visit` is not automatically a leak. The destination page is now resident and FreeRTOS/driver activity may have occurred. Use paired A->B->A or small-page control cycles and largest-block recovery to distinguish retained page memory from allocator fragmentation.

## State ownership audit

Use the test:

> If a value must persist after leaving the page, is the UI page really its owner?

Classify every page member as:

- **VIEW/NAVIGATION STATE** — safe to reset if current accepted behavior already resets it
- **SESSION UI STATE** — should be persisted in `UiSessionState` or another UI-session owner
- **DOMAIN/MUSICAL STATE** — must live in engine/project/transport/service state, not the page
- **ASYNC HANDLE/CALLBACK STATE** — requires explicit teardown proof

### Already favorable examples

- PERFORM musical values are owned by `PerformanceKeyboard`; page owns selected tool/row.
- MIDI PLAYER service state is outside the page; page owns visibility/scroll/view state.
- GENRE/FEEL applied musical state is in the engine; page mostly owns pending/focus state.

### Areas requiring scrutiny

- PROJECT dialog vectors/paths and whether leaving a dialog mid-operation has any semantic expectation
- PHRASE explicit placement/request state versus the documented onEnter reset rules
- SAMPLER/TAPE child component registration and whether `Container` retains shared ownership beyond the page
- any page that registers callbacks/listeners outside its own child tree
- help dialogs or overlays whose references could outlive the page

## Model comparison

### CURRENT — adaptive lazy cache

Actual behavior:

- constructor creates one current page
- missing-page navigation may retain previous page only when free DRAM >=16 KB
- when free DRAM <16 KB, other pages are destroyed before target construction

**Memory:** already close to one-page residency at the observed hardware floor.  
**Fragmentation:** repeated `new/delete` of heterogeneous page/component graphs can fragment heap.  
**Compatibility:** accepted baseline.

### DESTROY/RECREATE — explicit single `unique_ptr<IPage>`

Potential differences from current low-memory behavior are smaller than initially assumed.

Pros:

- explicit single-page invariant
- eliminates high-memory two-page caching path
- easier accounting

Risks:

- page-local state may reset more consistently than today on systems above threshold
- heterogeneous allocation graphs still allocate/free repeatedly
- does not cap largest temporary constructor allocation

**Feasibility:** statically plausible because the current Cardputer low-memory path already destroys pages, but hardware and ownership sweep must prove all required pages reclaim cleanly.

### FIXED ARENA — storage sized/aligned for maximum page object

A page object arena only bounds the **outer page object**. It does not automatically absorb heap side allocations from:

- `std::string` growth
- `std::vector` growth
- `std::make_shared` child components
- file-browser results
- dialog/help objects

Therefore `max(sizeof(Page))` is not the same as `max(total page residency)`.

Required design checks before calling an arena feasible:

1. `alignas(max_align)` / compile-time alignment for every page type
2. explicit destructor dispatch for active concrete type
3. no references to the page object escaping beyond transition
4. no page-local async callbacks surviving destruction
5. child objects destroyed before arena reuse
6. page-local state reset semantics match accepted behavior
7. persistent domain state already lives outside the page
8. exceptions are not required for construction failure handling on target

**Expected benefit:** potentially a fragmentation/outer-object-allocation improvement; resident-byte gain remains unproven until side allocations are measured.

## Diagnostic instrumentation required

R0-C may add diagnostic-only probes around `MiniAcidDisplay::createPage_()` / `getPage_()` in a temporary build. Record:

- page id and concrete type
- free/largest before destruction
- free/largest after destruction of prior pages
- free/largest immediately after `make_unique<Page>`
- free/largest after `setBoundaries`
- free/largest after first draw
- target `sizeof(Page)`
- heap integrity

Do not log every normal draw; only navigation edges and first-draw boundaries.

## Provisional findings

### TOTAL UI RESIDENT COST

**PENDING hardware phase accounting.**  
Prior top-level observation of roughly `3.5 KB` between post-SMF and post-UI states is an aggregate startup observation, not yet isolated to UI root/page owners.

### LARGEST PAGE

- type: **PENDING target-size + side-allocation sweep**
- sizeof: **PENDING**
- heap side allocations: **PENDING**

Static candidates with notable side/fixed costs include SAMPLER (`make_shared` component graph), TAPE (256-B waveform + shared controls), PHRASE (fixed preview arrays), and PROJECT (on-demand vectors/strings), but no ranking is asserted without target evidence.

### CURRENT RETENTION MODEL

**Adaptive lazy cache; at `free INTERNAL|8BIT < 16384`, destroy all non-target pages before constructing a missing target.**

### DESTROY/RECREATE FEASIBLE

**PARTIAL / already exercised by current low-memory path.**  
A universal explicit one-page model still requires the full page sweep and state/callback audit.

### FIXED ARENA FEASIBLE

**PARTIAL / not yet proven useful.**  
Outer-page polymorphic placement is mechanically feasible in principle, but side allocations mean memory recovery cannot be estimated from `sizeof(Page)` alone.

### EXPECTED MEMORY RECOVERY CLASS

**UNDECIDED pending hardware sweep.**  
Do not claim `4–8 KB` or `>8 KB` merely by summing all page types; current low-memory runtime does not retain that sum.

### STATE OWNERSHIP VIOLATIONS FOUND

**None proven from the initial static pass.**  
Several page-local states need classification, but the inspected high-priority pages generally keep authoritative musical/service state outside the page. SAMPLER/TAPE child ownership and PROJECT/PHRASE session semantics remain evidence gaps.

### IMPLEMENTATION RECOMMENDATION

**NONE until evidence complete**

### PRODUCTION CHANGES

**NONE**
