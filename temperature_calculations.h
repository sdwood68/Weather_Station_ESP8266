#pragma once

#include <math.h>

inline float calculateDewPointC(float temperatureC, float relativeHumidity) {
  if (!isfinite(temperatureC) || !isfinite(relativeHumidity) ||
      temperatureC < -80.0f || temperatureC > 80.0f ||
      relativeHumidity <= 0.0f || relativeHumidity > 100.0f) {
    return NAN;
  }

  // Alduchov-Eskridge Magnus approximation over water.
  const float a = 17.625f;
  const float b = 243.04f;
  const float gamma =
      logf(relativeHumidity / 100.0f) +
      (a * temperatureC) / (b + temperatureC);
  const float dewPointC = (b * gamma) / (a - gamma);
  if (!isfinite(dewPointC) || dewPointC < -100.0f ||
      dewPointC > temperatureC + 0.1f) {
    return NAN;
  }
  return dewPointC;
}

inline float calculateNwsHeatIndexF(
    float temperatureF,
    float relativeHumidity) {
  if (!isfinite(temperatureF) || !isfinite(relativeHumidity) ||
      temperatureF < -40.0f || temperatureF > 130.0f ||
      relativeHumidity < 0.0f || relativeHumidity > 100.0f) {
    return NAN;
  }

  float heatIndex =
      0.5f * (temperatureF + 61.0f +
              (temperatureF - 68.0f) * 1.2f +
              relativeHumidity * 0.094f);
  heatIndex = (heatIndex + temperatureF) / 2.0f;

  if (heatIndex < 80.0f) {
    return isfinite(heatIndex) && heatIndex >= -100.0f && heatIndex <= 250.0f
        ? heatIndex : NAN;
  }

  const float t2 = temperatureF * temperatureF;
  const float rh2 = relativeHumidity * relativeHumidity;
  heatIndex =
      -42.379f +
      2.04901523f * temperatureF +
      10.14333127f * relativeHumidity -
      0.22475541f * temperatureF * relativeHumidity -
      0.00683783f * t2 -
      0.05481717f * rh2 +
      0.00122874f * t2 * relativeHumidity +
      0.00085282f * temperatureF * rh2 -
      0.00000199f * t2 * rh2;

  if (relativeHumidity < 13.0f &&
      temperatureF >= 80.0f && temperatureF <= 112.0f) {
    heatIndex -=
        ((13.0f - relativeHumidity) / 4.0f) *
        sqrtf((17.0f - fabsf(temperatureF - 95.0f)) / 17.0f);
  } else if (relativeHumidity > 85.0f &&
             temperatureF >= 80.0f && temperatureF <= 87.0f) {
    heatIndex +=
        ((relativeHumidity - 85.0f) / 10.0f) *
        ((87.0f - temperatureF) / 5.0f);
  }

  return isfinite(heatIndex) && heatIndex >= -100.0f && heatIndex <= 250.0f
      ? heatIndex : NAN;
}
