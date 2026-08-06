#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

inline float calculateAsosMinuteRainInches(
    uint32_t tipCount,
    float tipSizeMillimeters) {
  if (!isfinite(tipSizeMillimeters) ||
      tipSizeMillimeters <= 0.0f ||
      tipSizeMillimeters > 10.0f) {
    return NAN;
  }

  const float measuredInches =
      tipCount * tipSizeMillimeters / 25.4f;
  const float correctedInches =
      measuredInches * (1.0f + 0.60f * measuredInches);
  return isfinite(correctedInches) ? correctedInches : NAN;
}

inline float roundRainToHundredthInch(float inches) {
  if (!isfinite(inches) || inches < 0.0f) {
    return NAN;
  }
  return roundf(inches * 100.0f) / 100.0f;
}

inline float sumLatestRainInches(
    const float* minuteHistory,
    size_t historyCapacity,
    size_t nextWriteIndex,
    size_t availableMinutes,
    size_t requestedMinutes) {
  if (!minuteHistory || historyCapacity == 0 || requestedMinutes == 0 ||
      requestedMinutes > historyCapacity ||
      availableMinutes < requestedMinutes ||
      nextWriteIndex >= historyCapacity) {
    return NAN;
  }

  float total = 0.0f;
  for (size_t i = 0; i < requestedMinutes; ++i) {
    const size_t index =
        (nextWriteIndex + historyCapacity - 1U - i) % historyCapacity;
    if (!isfinite(minuteHistory[index]) || minuteHistory[index] < 0.0f) {
      return NAN;
    }
    total += minuteHistory[index];
  }
  return isfinite(total) ? total : NAN;
}