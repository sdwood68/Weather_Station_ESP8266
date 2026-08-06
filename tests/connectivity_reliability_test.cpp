#include <assert.h>
#include <stdint.h>

#include "../connectivity_reliability.h"

int main() {
  assert(!portalTimeoutExpired(1000U, 100U, 901U));
  assert(portalTimeoutExpired(1001U, 100U, 901U));
  assert(portalTimeoutExpired(20U, UINT32_MAX - 20U, 40U));
  assert(!portalTimeoutExpired(19U, UINT32_MAX - 20U, 40U));

  assert(!requiredConfigurationMissing(true, true, true, true, true));
  assert(requiredConfigurationMissing(false, true, true, true, true));
  assert(requiredConfigurationMissing(true, false, true, true, true));
  assert(requiredConfigurationMissing(true, true, false, true, true));
  assert(requiredConfigurationMissing(true, true, true, false, true));
  assert(requiredConfigurationMissing(true, true, true, true, false));
  return 0;
}
