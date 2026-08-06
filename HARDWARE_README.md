# Weather Station Hardware Project

This is the hardware-facing entry point for the ESP8266 MQTT Weather Station.
Share this file and `HARDWARE_ROADMAP.md` with the Weather Station Hardware
Project. Firmware-only planning remains in `ROADMAP.md`.

## Immediate electrical safety issue

The installed AM2315 I2C bus has 10 kOhm SDA and SCL pull-ups connected to 5 V.
ESP8266 GPIO is not 5 V tolerant. Do not continue long-duration operation in
this configuration.

Required correction:

1. Keep the AM2315 supply at 5 V if required by the sensor.
2. Disconnect SDA and SCL pull-ups from 5 V.
3. Pull SDA and SCL up to the controller's 3.3 V rail.
4. Verify both signals idle near 3.3 V and never exceed 3.6 V at the ESP8266.
5. Account for parallel pull-ups fitted to other sensor boards.

## Current hardware baseline

- Controller: Wemos/LOLIN D1 Mini-class ESP8266.
- I2C sensors: BMP280, AHT20, and outdoor AM2315.
- AM2315 cable: approximately four feet.
- Wind speed: dry-contact/reed-switch cup anemometer.
- Wind direction: analog resistor vane using the ESP8266 analog input.
- Rain: normally open tipping-bucket switch.
- No dedicated voltage supervisor, current monitor, or verified brownout
  ride-through design is documented.

## Current firmware interface contract

| Function | ESP8266 connection | Electrical expectation |
| --- | --- | --- |
| I2C SDA | Board default, normally GPIO4/D2 | 3.3 V logic only |
| I2C SCL | Board default, normally GPIO5/D1 | 3.3 V logic only |
| Wind pulse | GPIO13/D7 | Dry contact to ground; internal pull-up |
| Rain pulse | GPIO12/D6 | Dry contact to ground; internal pull-up; never 5 V |
| Wind direction | ADC/A0 | Remain within the selected board's ADC range |
| Status LED | GPIO14/D5 | Verify polarity and series resistance |

The hardware project must publish an explicit pin map for every supported board.
Do not assume NodeMCU, D1 Mini, Feather ESP8266, and ESP32 pin labels or ADC
ranges are interchangeable.

## Environmental and installation requirements

- Use weather-resistant connectors, strain relief, drip loops, and appropriate
  enclosure ventilation.
- Protect long outdoor wind and rain conductors from ESD and surge energy.
- Route I2C separately from pulse wiring and noisy power conductors.
- Level the tipping-bucket gauge and provide a controlled-water test.
- Mount the pressure sensor in a shared static-pressure environment.
- Survey wind-vane true north and record the direction offset.

## Hardware-to-firmware handoff

Before hardware-dependent firmware work resumes, provide:

- board name, revision, schematic, BOM, and assembly notes;
- confirmed rails, logic levels, pull-ups, cable lengths, and connectors;
- explicit GPIO and ADC mapping for every supported controller;
- measured I2C rise times and classified AM2315 transaction results;
- anemometer calibration, vane ADC ranges, and rain depth per tip;
- power traces for boot, Wi-Fi, MQTT, idle, reporting, OTA, and recovery;
- supervisor thresholds and observed reset/brownout behavior;
- power-monitor scaling and accuracy data;
- validation logs identified by hardware revision and firmware commit.

See `AM2315_RELIABILITY.md` and `POWER_BASELINE.md` for detailed procedures.
