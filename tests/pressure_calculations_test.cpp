#include <assert.h>
#include <math.h>

#include "../pressure_calculations.h"

static bool near(float actual, float expected, float tolerance) {
  return fabsf(actual - expected) <= tolerance;
}

int main() {
  // NWS-published equation regression vector at Denver elevation.
  assert(near(
      calculateAltimeterSettingHpa(828.377f, 1655.37f),
      1011.585f,
      0.05f));

  // At mean sea level, the reductions remain effectively the sensor pressure.
  assert(near(
      calculateAltimeterSettingHpa(1013.25f, 0.0f),
      1012.95f,
      0.001f));
  assert(near(
      calculateSeaLevelPressureHpa(1013.25f, 0.0f, 15.0f, 15.0f),
      1013.25f,
      0.001f));

  // A representative elevated-station reduction must increase pressure.
  const float seaLevel =
      calculateSeaLevelPressureHpa(954.61f, 500.0f, 20.0f, 10.0f);
  assert(seaLevel > 1010.0f && seaLevel < 1020.0f);

  return 0;
}
