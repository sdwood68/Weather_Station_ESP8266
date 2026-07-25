#include <assert.h>
#include <math.h>

#include "../temperature_calculations.h"

static bool near(float actual, float expected, float tolerance = 0.05f) {
  return fabsf(actual - expected) <= tolerance;
}

int main() {
  assert(near(calculateDewPointC(30.0f, 70.0f), 23.93f));
  assert(near(calculateDewPointC(20.0f, 100.0f), 20.0f));
  assert(isnan(calculateDewPointC(20.0f, 0.0f)));

  assert(near(calculateNwsHeatIndexF(90.0f, 70.0f), 105.92f));
  assert(near(calculateNwsHeatIndexF(80.0f, 40.0f), 79.79f));
  assert(isnan(calculateNwsHeatIndexF(90.0f, 101.0f)));
  assert(isnan(calculateNwsHeatIndexF(2500000.0f, 50.0f)));
  assert(isnan(calculateNwsHeatIndexF(NAN, 50.0f)));
  assert(isnan(calculateNwsHeatIndexF(130.0f, 100.0f)));
  assert(isnan(calculateDewPointC(2500000.0f, 50.0f)));
  assert(isnan(calculateDewPointC(NAN, 50.0f)));

  return 0;
}
