#include "runtime_pattern_event_bank.h"

// RuntimePatternEventBank is intentionally header-inline. MiniAcid is built by
// several retained source-list surfaces; keeping this fixed value helper inline
// avoids adding a second build-graph ownership requirement for prepared Pattern
// data while the authoritative executor is still legacy.
