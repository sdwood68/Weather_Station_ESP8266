# Wind Reporting Design

Firmware 0.7.0 replaces the original five-second peak and rounded vector
calculation with explicitly named observation periods.

## Selected observation model

- **Pulse sampling:** The cup-anemometer interrupt count is captured every one
  second. The direction vane is sampled at the same time.
- **Sustained speed:** Total pulses in the latest 120 one-second samples are
  converted to a scalar two-minute mean. The MQTT report period is also two
  minutes, so each normal report describes the latest complete observation.
- **Direction:** The one-second directions are resolved as speed-weighted
  north/east "from" components. Their resultant is converted back to the
  meteorological direction from which the wind blows. Direction is not rounded
  to the vane's 22.5-degree positions because vector averaging can legitimately
  produce an intermediate direction.
- **Calm:** Sustained speed below 2 knots (2.3 mph) is calm. Calm direction is
  reported as 0 degrees. This follows the ASOS response/reporting threshold,
  while allowing the Home Assistant wind unit to be mph or km/h.
- **Gust:** The largest rolling three-second mean in the two-minute observation
  is published as `3-Second Wind Gust`. This is a measured maximum, not an ASOS
  determination that a formal gust remark is warranted; no peak/lull threshold
  is applied.
- **Startup:** Before 120 samples exist, speed and gust use the number of
  samples actually captured. `Wind Sample Count` makes this partial window
  visible.

The NWS defines sustained wind as a two-minute average. Its ASOS description
states that wind speed and direction use two-minute observations and that wind
at 2 knots or less is reported calm. NOAA's 2003 ASOS transition notice
documents the move to the WMO three-second running-average gust. The AMS
Glossary defines true wind direction as the direction from which the wind blows,
relative to true north.

Primary references:

- [NWS ASOS wind sensor](https://www.weather.gov/asos/WindSensor.html)
- [NWS glossary: sustained wind](https://forecast.weather.gov/glossary.php?word=sustained%20wind)
- [NWS ASOS three-second gust transition notice](https://www.weather.gov/media/notification/tins/tin03-15asos_wind.pdf)
- [AMS Glossary: true wind direction](https://glossary.ametsoc.org/wiki/true-wind-direction/)
- [AMS Glossary: gust](https://glossary.ametsoc.org/wiki/gust/)

This station is not an ASOS installation. In particular, sensor height,
exposure, calibration, resolution, and report qualification differ. Entity
names therefore state the averaging duration without claiming METAR
conformance.

## Calibration constants

The Home Assistant `wind_speed_unit` selector chooses `mph` or `km/h`. The selection is persisted, scales both sustained speed and gust (`mph × 1.609344` for km/h), and restarts the station so Home Assistant discovery metadata and reported values always use the same unit.

`ANEMOMETER_MPH_PER_HZ` is `1.492 mph` per pulse per second, the current
manufacturer conversion for the installed cup anemometer. Validate this value
for the exact sensor and reed-switch pulse count; some anemometers produce more
than one pulse per revolution.

`WIND_DIRECTION_OFFSET_DEGREES` is added clockwise to every decoded vane
direction. It defaults to `0.0`. Set it to the surveyed difference between the
vane's electrical north and true north. Magnetic-compass alignment must be
corrected for local declination before entering the offset.

The direction ADC thresholds in `get_wind_dir()` map the installed 16-position
resistor vane. They must be checked against actual ADC readings after any
change to the vane, supply, cable, or resistor network.

## Repeatable validation

1. Disconnect or immobilize the cups. Run for at least two minutes and confirm
   zero pulse diagnostics, 0 mph sustained/gust, direction 0, and 120 samples.
2. Inject a known dry-contact pulse rate for two minutes. Expected sustained
   speed is `total pulses × 1.492 / 120 mph`.
3. Inject a known three-second burst. Expected gust is
   `largest pulses in any three adjacent one-second samples × 1.492 / 3 mph`.
   Confirm `3-Second Gust Pulse Count` matches that numerator.
4. Place the vane at all 16 compass positions, record raw ADC values, and
   confirm every threshold maps to the intended direction without chatter.
5. Exercise directions around north (for example, 350 and 10 degrees) and
   confirm the vector result stays near north rather than incorrectly averaging
   to south.
6. Align the vane to surveyed true north, determine the required clockwise
   offset, and repeat the cardinal-direction checks.
7. In the field, colocate the station with a calibrated reference anemometer in
   unobstructed exposure. Compare synchronized two-minute speed/direction and
   three-second gust observations across calm, moderate, and gusty conditions.

The Home Assistant diagnostic entities `2-Minute Wind Pulse Count`, `Wind
Sample Count`, and `3-Second Gust Pulse Count` provide the inputs needed to
reconstruct speed and gust results. Serial output reports the same values.
### Diagnostic topic interpretation

- `wind_sample_count` is the number of one-second buckets currently included in
  the sustained-speed and direction observation. It rises from 1 to 120 after
  boot and should remain 120 during normal operation. A smaller value means the
  report covers a partial startup/recovery window.
- `wind_pulse_count` is the total anemometer reed-switch closures across those
  samples. Sustained mph is `pulse count × 1.492 / sample count` while samples
  remain one second long.
- `wind_gust_pulse_count` is the largest pulse total found in any three adjacent
  one-second samples. Three-second gust mph is `gust pulse count × 1.492 / 3`.

Sample count is not wind speed and is not the number of anemometer revolutions.
It makes timing/completeness problems distinguishable from actual calm wind.

## Test vectors

`tests/wind_calculations_test.cpp` covers constant easterly wind, a three-second
southerly burst, direction wraparound at true north, and calm behavior. It is a
host-side test for the platform-independent calculation in
`wind_calculations.h`.
