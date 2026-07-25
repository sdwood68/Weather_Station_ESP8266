#include <assert.h>
#include <math.h>

#define PI 3.14159265358979323846
#include "../wind_calculations.h"

static bool near(float actual, float expected, float tolerance = 0.01f) {
  return fabsf(actual - expected) <= tolerance;
}

int main() {
  {
    const uint16_t pulses[] = {2, 2, 2, 2, 2, 2};
    const float directions[] = {90, 90, 90, 90, 90, 90};
    const WindObservation wind =
        calculateWindObservation(pulses, directions, 6, 1, 3, 1.492f, 2.3f);
    assert(near(wind.sustainedMph, 2.984f));
    assert(near(wind.gustMph, 2.984f));
    assert(near(wind.directionDegrees, 90.0f));
    assert(wind.pulseCount == 12);
    assert(wind.gustPulseCount == 6);
    assert(!wind.calm);
  }

  {
    const uint16_t pulses[] = {0, 0, 3, 5, 4, 0};
    const float directions[] = {0, 0, 180, 180, 180, 180};
    const WindObservation wind =
        calculateWindObservation(pulses, directions, 6, 1, 3, 1.492f, 2.3f);
    assert(near(wind.gustMph, 5.968f));
    assert(near(wind.directionDegrees, 180.0f));
    assert(wind.gustPulseCount == 12);
  }

  {
    const uint16_t pulses[] = {1, 1, 1, 1};
    const float directions[] = {350, 10, 350, 10};
    const WindObservation wind =
        calculateWindObservation(pulses, directions, 4, 1, 3, 1.492f, 0.0f);
    assert(wind.directionDegrees < 0.1f ||
           wind.directionDegrees > 359.9f);
  }

  {
    const uint16_t pulses[] = {0, 0, 0};
    const float directions[] = {45, 90, 180};
    const WindObservation wind =
        calculateWindObservation(pulses, directions, 3, 1, 3, 1.492f, 2.3f);
    assert(wind.calm);
    assert(near(wind.directionDegrees, 0.0f));
  }

  return 0;
}
