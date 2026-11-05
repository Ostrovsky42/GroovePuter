#include "src/debug_log.h"
#include "src/platform/log.h"

int main() {
  LOG_DEBUG("TEST", "structured logger value=%d", 1);
  LOG_PLATFORM_DEBUG("platform logger value=%d\n", 2);
  return 0;
}
