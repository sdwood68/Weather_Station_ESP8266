# Atmospheric Pressure Reporting Design

Firmware 0.7.0 separates the pressure parameters that were previously presented
as one generic `Air Pressure` value.

## Parameters

- **Station pressure:** The calibrated BMP280 sensor pressure at the station.
  The existing `air_pressure` entity remains in kPa for compatibility but is
  renamed `Station Pressure`.
- **Altimeter setting:** Station pressure reduced using the NWS standard-
  atmosphere equation and configured station elevation. It is published in hPa.
- **Three-hour change and trend:** Corrected station pressure is compared with
  the value three hours earlier. Change is rounded to the NWS reporting precision
  of 0.1 hPa; its sign produces ising, alling, or steady.
- **Sea-level pressure:** Station pressure reduced using station elevation and
  the mean of the present outdoor temperature and the outdoor temperature from
  12 hours earlier, as required by Federal Meteorological Handbook No. 1. It is
  published in hPa only after a valid 12-hour temperature history exists.

The NWS distinguishes these values: station pressure is pressure at the
designated station elevation, altimeter setting uses the standard-atmosphere
temperature profile for aviation, and mean sea-level pressure uses observed
temperature conditions. Primary references:

- [NWS pressure definitions](https://www.weather.gov/bou/pressure_definitions)
- [NWS ASOS barometric pressure sensor](https://www.weather.gov/asos/BarometricPressureSensor.html)
- [NWS altimeter-setting equation](https://www.weather.gov/media/epz/wxcalc/altimeterSetting.pdf)
- [Federal Meteorological Handbook No. 1, Chapter 11](https://repository.library.noaa.gov/view/noaa/56580/noaa_56580_DS1.pdf)

This weather station is not an ASOS installation. ASOS uses redundant,
traceably calibrated pressure sensors and withholds a report when sensor
agreement is unacceptable. This station has one BMP280, so the entity names
describe NWS-defined parameters without claiming ASOS accuracy or redundancy.

## Configuration and calibration

The WiFiSettings portal provides:

- `weather_station_elevation_m`: designated station/sensor elevation above mean
  sea level in whole meters. This setting has no default; altimeter and sea-level
  pressure remain unpublished until it is configured. The same value is exposed as a
  non-retained Home Assistant Station Elevation number control, displayed in
  feet for USA or metres for European Union and U.K. Accepted
  commands are persisted to LittleFS and applied immediately; Altimeter Setting
  remains a calculated sensor rather than a manual override.
- `weather_pressure_offset_pa`: signed calibration correction in pascals.

Determine elevation from a surveyed benchmark or reliable geodetic source, not
the BMP280's pressure-derived altitude. Compare the sensor against a
NIST-traceable reference at the same elevation and in a shared static-pressure
environment. Enter `reference pressure - BMP280 pressure` as the calibration
offset. Do not calibrate raw station pressure to a consumer weather service's
sea-level pressure.

## Validation

1. With both sensors at the same height, compare corrected station pressure
   against a traceable reference across several hours.
2. Verify the configured elevation independently.
3. Compare `Altimeter Setting` with the NWS calculator using the reported
   station pressure and configured elevation.
4. Keep the station running for more than 12 hours with a working outdoor
   AM2315. Confirm sea-level pressure remains unpublished before the history is
   complete.
5. Reconstruct sea-level pressure from station pressure, elevation, current
   temperature, and the temperature recorded 12 hours earlier.
6. Compare trends—not aviation suitability—with a nearby official station.

`tests/pressure_calculations_test.cpp` contains an NWS-equation regression at Denver elevation, sea-level identity
cases, and an
elevated-station sea-level reduction range check.
