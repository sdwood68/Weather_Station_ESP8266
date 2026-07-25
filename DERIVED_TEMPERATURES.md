# Dew Point and Heat Index

Firmware 0.7.0 derives dew point and heat index from the same AM2315 outdoor
temperature and relative-humidity reading. It does not mix enclosure
temperature or humidity from a different sensor into either calculation.

## Dew point

Dew point is calculated with the Alduchov-Eskridge Magnus approximation over
water:

```text
gamma = ln(RH / 100) + aT / (b + T)
Td = b gamma / (a - gamma)
a = 17.625, b = 243.04 C
```

The result is converted to degrees Fahrenheit for Home Assistant. Invalid
relative humidity at or below 0% or above 100% is rejected. Sensor temperatures
outside the AM2315 rated range of -40 through 80 C are also rejected.

The NWS defines dew point as the temperature to which air must be cooled to
reach saturation at constant pressure and moisture content. The Magnus
approximation is also used in NOAA/NWS-supported meteorological work.

## Heat index

Heat index follows the complete NWS Weather Prediction Center procedure:

1. Calculate the simple Steadman-consistent approximation and average it with
   the measured air temperature.
2. If that preliminary result is at least 80 F, calculate the Rothfusz
   regression.
3. Apply the NWS low-humidity or high-humidity adjustment when its temperature
   and humidity conditions are met.

The heat index model describes shaded, light-wind conditions and includes
assumptions about clothing, activity, body size, and other factors. It is not a
direct measurement and should not be treated as personalized medical advice.

Every AM2315 transaction starts with fresh non-finite values and must report success.
Failed reads, non-finite inputs, out-of-range sensor readings, and calculated heat
indices outside -100 through 250 F are rejected rather than published. The final
bound is a defensive guard against corrupted inputs reaching the polynomial.

Primary references:

- [NWS Weather Prediction Center heat-index equation](https://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml)
- [NWS Rothfusz Technical Attachment SR 90-23](https://www.weather.gov/media/bgm/ta_htindx.PDF)
- [NWS dew-point definition](https://forecast.weather.gov/glossary.php?word=DWPT)

## Validation

`tests/temperature_calculations_test.cpp` checks saturated air, a published
Magnus example range, the NWS 90 F/70% heat-index case, the lower-temperature
simple formula, invalid humidity handling, non-finite values, impossible sensor
inputs, and extreme polynomial outputs. Physical validation requires a
working, calibrated AM2315 exposed to ambient outdoor air.
