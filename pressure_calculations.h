#pragma once

#include <math.h>

inline float calculateAltimeterSettingHpa(
    float stationPressureHpa,
    float stationElevationMeters) {
  // NWS El Paso weather-calculator equation. The 0.3 hPa correction and
  // standard-atmosphere constants are retained as published.
  const float exponent = 0.190284f;
  const float correctedPressure = stationPressureHpa - 0.3f;
  if (correctedPressure <= 0.0f) {
    return NAN;
  }

  const float elevationTerm =
      (powf(1013.25f, exponent) * 0.0065f / 288.0f) *
      stationElevationMeters / powf(correctedPressure, exponent);
  return correctedPressure *
      powf(1.0f + elevationTerm, 1.0f / exponent);
}

inline float calculateSeaLevelPressureHpa(
    float stationPressureHpa,
    float stationElevationMeters,
    float currentTemperatureC,
    float temperatureTwelveHoursAgoC) {
  // Hypsometric reduction using the FMH-1 required 12-hour mean station
  // temperature and the standard 0.0065 K/m environmental lapse rate.
  const float gravityMps2 = 9.80665f;
  const float dryAirGasConstant = 287.05f;
  const float lapseRateKPerMeter = 0.0065f;
  const float meanStationTemperatureK =
      273.15f + (currentTemperatureC + temperatureTwelveHoursAgoC) / 2.0f;
  const float meanColumnTemperatureK =
      meanStationTemperatureK + lapseRateKPerMeter * stationElevationMeters / 2.0f;
  if (stationPressureHpa <= 0.0f || meanColumnTemperatureK <= 0.0f) {
    return NAN;
  }

  return stationPressureHpa *
      expf(gravityMps2 * stationElevationMeters /
           (dryAirGasConstant * meanColumnTemperatureK));
}
