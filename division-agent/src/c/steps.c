#include <pebble.h>

// Exposed to JavaScript through the Alloy FFI (see src/embeddedjs/manifest.json).
// Alloy has no JS health API yet, so we call the firmware health service directly.
int32_t getSteps(void) {
  return (int32_t)health_service_sum_today(HealthMetricStepCount);
}
