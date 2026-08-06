# AM2315 Reliability Investigation

## Current status

Investigation began after two physical symptoms on station `541a1d`:

- AM2315 initialization failed during an earlier boot validation.
- A later failed read allowed stale values to reach the heat-index calculation.
  Firmware 0.7.1 now rejects failed, non-finite, and out-of-range readings, so
  that failure cannot produce an extreme published heat index.

The rejection guard protects derived values but does not identify or recover the
underlying I2C failure.

## Confirmed software weaknesses

1. `am2315.begin()` is called only once during `setup()`. If that first test read
   fails, `bAm2315` remains false and the sensor is never retried until reboot.
2. Adafruit AM2315 library 2.2.3 performs wake, command, and eight-byte read
   transactions but ignores the Boolean result of each I2C operation.
3. The driver checks the function code and returned byte count but does not
   validate the two CRC bytes specified by the AM2315 protocol. A complete but
   corrupted frame can therefore be accepted.
4. The existing firmware reports rejected values over serial only. It exposes no
   Home Assistant counters for initialization attempts, successful reads,
   transaction failures, consecutive failures, or last successful sample time.
5. The two-minute reporting interval satisfies the sensor's maximum 0.5 Hz
   update rate. Oversampling is not the cause in the application code.

## Confirmed electrical fault

The installed SDA and SCL pull-ups are 10 kOhm to 5 V. This exceeds the
ESP8266 GPIO limit: Espressif specifies that the ESP8266 is not 5 V tolerant
and limits GPIO voltage to 3.6 V. The wiring can cause unreliable I2C behavior
and can permanently damage the controller.

Corrective wiring:

- Keep the original AM2315 sensor supply on 5 V if required by its 3.5-5.5 V
  specification.
- Disconnect both SDA and SCL pull-ups from 5 V.
- Reconnect the pull-ups from SDA and SCL to the D1 Mini 3.3 V rail. The existing
  10 kOhm values are within Adafruit's documented range, although the effective
  bus pull-up must include any pull-ups already fitted to other sensor boards.
- Before reconnecting the ESP8266, verify with a meter that SDA and SCL idle near
  3.3 V and never rise above 3.6 V.

Do not continue long-duration operation with the bus pulled to 5 V.
## Electrical unknowns to verify

- Sensor supply voltage at the AM2315 during idle and a read transaction. The
  original datasheet specifies 3.5-5.5 V and recommends 5 V.
- SDA and SCL idle-high voltage. ESP8266 inputs must not be pulled up to a 5 V
  sensor supply; the bus pull-ups should establish 3.3 V logic levels.
- Presence and effective parallel value of the required SDA and SCL pull-ups.
  Adafruit specifies external pull-ups in the 2-10 kOhm range.
- The installed cable is approximately 4 feet long. This is substantially longer
  than a typical board-level I2C interconnect; its capacitance can make 10 kOhm
  pull-ups too weak for reliable rise times. Routing beside wind/rain wiring or
  power conductors, connector corrosion, moisture ingress, and ground continuity
  remain to be checked.
- I2C rise time and any NACK, truncated frame, stuck-low line, or CRC failure
  captured at the sensor connector during Wi-Fi transmission and sensor reads.

I2C is intended for short interconnects. Longer outdoor cable, weak pull-ups,
and electrical interference can cause intermittent dropouts even when the bus
works most of the time.

## Targeted test sequence

1. Record sensor wire colors, supply connection, pull-up locations and values,
   cable length, and connector condition.
2. With a multimeter, measure AM2315 supply, SDA idle, and SCL idle at both the
   controller and sensor ends. Stop if either signal is pulled above 3.3 V at the
   ESP8266.
3. After moving the pull-ups to 3.3 V, test the four-foot cable first with the
   existing 10 kOhm resistors, then with a known effective pull-up near 4.7 kOhm
   if failures remain. Measure rise time before trying a stronger value; account
   for pull-ups already present on the BMP280 or AHT20 boards.
4. Temporarily test the AM2315 with a short cable and the same 3.3 V logic
   pull-ups. Compare its success rate with the installed cable.
   Compare its success rate with the installed cable.
5. Capture at least 100 wake/read transactions with a logic analyzer. Classify
   failures by wake NACK, command NACK, short read, invalid header, or CRC.
6. Add firmware diagnostics and retry behavior only after the failure classes
   are observable. Track attempts, successes, consecutive failures, CRC errors,
   recoveries, and last-success age in Home Assistant.
7. Exercise cold boot, slow power rise, Wi-Fi transmission, OTA reboot, cable
   disturbance, and several hours of normal operation.

## Candidate firmware work after measurement

- Retry initialization on a bounded schedule instead of permanently disabling
  the sensor after one failed startup read.
- Implement a project-owned AM2315 transaction wrapper that checks every I2C
  result, waits according to the protocol, validates CRC, and reports a specific
  failure code.
- Attempt bounded I2C bus recovery after a stuck-bus condition, without hiding a
  persistent wiring fault or blocking MQTT and OTA processing.
- Publish diagnostic counters and availability without republishing stale
  temperature or humidity values.

## References

- AM2315 datasheet: https://cdn-shop.adafruit.com/datasheets/AM2315.pdf
- Adafruit AM2315 product and wiring information:
  https://www.adafruit.com/product/1293
- Adafruit I2C cable guidance:
  https://learn.adafruit.com/working-with-i2c-devices/cable-length
- Installed driver: Adafruit AM2315 2.2.3