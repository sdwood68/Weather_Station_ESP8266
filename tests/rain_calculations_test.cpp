#include <assert.h>
#include <math.h>

#include "../rain_calculations.h"

static bool near(float actual, float expected, float tolerance = 0.0001f) {
  return fabsf(actual - expected) <= tolerance;
}

int main() {
  assert(near(calculateAsosMinuteRainInches(1, 0.254f), 0.01006f));
  assert(near(roundRainToHundredthInch(
                  calculateAsosMinuteRainInches(1, 0.254f)),
              0.01f));

  assert(near(calculateAsosMinuteRainInches(10, 0.254f), 0.106f));
  assert(near(roundRainToHundredthInch(
                  calculateAsosMinuteRainInches(10, 0.254f)),
              0.11f));

  assert(near(calculateAsosMinuteRainInches(5, 0.2f),
              (1.0f / 25.4f) * (1.0f + 0.60f / 25.4f)));

  assert(isnan(calculateAsosMinuteRainInches(1, 0.0f)));
  assert(isnan(calculateAsosMinuteRainInches(1, NAN)));

  const float history[] = {0.01f, 0.02f, 0.03f, 0.04f, 0.05f};
  assert(near(sumLatestRainInches(history, 5, 0, 5, 3), 0.12f));
  assert(near(sumLatestRainInches(history, 5, 3, 5, 4), 0.11f));
  assert(isnan(sumLatestRainInches(history, 5, 3, 2, 3)));

  return 0;
}