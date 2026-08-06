# Rain Gauge Reporting

Firmware 0.8.0 counts a normally open tipping-bucket rain gauge on `RAIN_PIN`
(GPIO12) and reports liquid precipitation accumulations through Home Assistant.
The station is not an ASOS installation; the processing follows the documented
ASOS heated-tipping-bucket accumulation method without claiming ASOS sensor,
heating, siting, calibration, redundancy, or aviation-reporting performance.

## Home Assistant entities

- `rain_tip_size`: persistent control in inches per tip, from 0.001 through
  0.400 inch in 0.001-inch steps. The default is 0.010 inch.
- `rain_tip_count_1m`: debounced raw tips in the latest one-minute period.
- `rain_1m`: corrected latest one-minute liquid accumulation.
- `rain_1h`: sum of the latest 60 valid corrected one-minute periods.
- `rain_3h`: sum of the latest 180 valid corrected one-minute periods.
- `rain_6h`: sum of the latest 360 valid corrected one-minute periods.
- `rain_24h`: sum of the latest 1,440 valid corrected one-minute periods.
- `rain_session_total`: corrected rain since boot or the latest tip-size change.

Accumulations are reported in inches with 0.01-inch precision. Longer-window
entities remain unknown after boot until their complete history exists. The
24-hour history uses approximately 5.6 KiB of RAM and is not persisted across a
restart. Session total is explicitly named so it cannot be mistaken for a
calendar-day climate total.

## ASOS accumulation algorithm

ASOS obtains heated-tipping-bucket accumulation once per minute. For each
one-minute period, measured rainfall `A` in inches is adjusted for tipping-bucket
undercatch at high rates:

```text
C = A × (1 + 0.60A)
A = tip count × configured inches per tip
```

The firmware retains `C` as floating point in its minute history and rounds only
the published final accumulation to the nearest hundredth of an inch. Zero-tip
minutes are valid zero-accumulation periods.

The correction was developed for the ASOS heated tipping bucket. A different
collector and bucket may require its own laboratory or field calibration. The
configurable tip size converts a gauge's nominal bucket volume to depth; it does
not prove that the ASOS high-rate correction characterizes that gauge.

## Electrical input and debounce

`RAIN_PIN` uses the ESP8266 internal pull-up and counts falling edges. The gauge
switch should connect only between GPIO12 and ground. Do not apply 5 V to the
GPIO. A 20 ms interrupt debounce rejects ordinary reed-switch bounce while
remaining shorter than the 60 ms tip spacing implied by the ASOS gauge's stated
10-inch-per-hour upper range at 0.01 inch per tip.

Use twisted-pair or shielded cable where practical, provide surge protection for
long outdoor wiring, and confirm the switch produces exactly one accepted edge
per physical bucket tip.

## Validity and limitations

- Changing `rain_tip_size` persists the new value and clears all rain history.
- An OTA transfer pauses scheduled rain processing. Tips remain interrupt-counted
  until reboot or task recovery, but an interrupted update can invalidate minute
  boundaries and must be treated as missing observational data.
- The rolling HA windows are useful station diagnostics. They are not encoded
  METAR `Prrrr`, `6RRRR`, `7RRRR`, SHEF, calendar-day, or calendar-month products.
- A tipping bucket can under-report intense rain and frozen precipitation. This
  implementation has no heater or precipitation-type sensor.
- Physical calibration requires a measured water volume delivered slowly and at
  controlled higher rates, with the gauge level and installed as intended.

## Validation

`tests/rain_calculations_test.cpp` covers nominal 0.01-inch and 0.2-mm buckets,
the ASOS high-rate adjustment, rounding, invalid calibration, circular history
sums, and startup validity gating.

Physical validation should inject a known number of switch closures, confirm the
one-minute raw count, compare calculated depth with a calibrated water delivery,
and run long enough to verify each history threshold and circular-buffer rollover.

## Primary references

- NWS ASOS Heated Tipping Bucket:
  https://www.weather.gov/asos/TippingBucket.html
- ASOS User's Guide, Sections 3.4.1 and 3.4.2:
  https://www.weather.gov/media/asos/aum-toc.pdf
- NWS ASOS information reporting and hourly precipitation:
  https://www.weather.gov/asos/InformationReporting.html