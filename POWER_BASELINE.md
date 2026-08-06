# Firmware 0.9.0 Power Baseline

## Purpose

Measure the existing firmware before changing radio behavior. The baseline must
separate energy used by boot, Wi-Fi association, MQTT connection and discovery,
the two-minute sensor report, steady connected idle, and an open OTA window.
Power changes will be accepted only when wind pulse counting, Home Assistant
availability, MQTT recovery, and OTA targeting remain reliable.

## Current behavior

- The ESP8266 remains associated with Wi-Fi and services `mqtt.loop()`
  continuously.
- Wind pulses are counted by a GPIO interrupt. The scheduler snapshots pulse
  count and wind direction once per second.
- Sensors and MQTT state are published every 120 seconds.
- ArduinoOTA is serviced only during an eight-minute authorized window.
- No explicit Wi-Fi sleep mode is selected by application code. The 0.9.0
  diagnostic build prints the mode selected by the installed ESP8266 core.
- Full deep sleep would stop the one-second wind-direction samples and MQTT/OTA
  command reception. It is not a drop-in power optimization for this design.

## Measurement equipment

Use an inline DC power analyzer or current monitor with sufficient bandwidth for
ESP8266 Wi-Fi transmit peaks. Record voltage and current at the station input,
not at a USB port that bypasses the intended field supply. A slow multimeter can
provide an average but cannot characterize transient peaks or brownouts.

Record the following for every run:

- board profile and physical board revision;
- firmware version and Git commit;
- input supply and cable configuration;
- attached sensors and pull-up network;
- Wi-Fi RSSI, access-point DTIM/beacon settings when known, and MQTT broker;
- sampling rate and integration method of the power instrument.

## Baseline phases

1. **Boot:** power application through completion of setup.
2. **Wi-Fi association:** start of `WiFiSettings.connect()` through connection.
3. **MQTT connection:** broker start through the connected callback, including
   discovery and diagnostics publication.
4. **Connected idle:** a quiet interval between reports with OTA disabled.
5. **Sensor report:** complete `report_task()` execution and MQTT publication.
6. **OTA ready:** authorized OTA window with no transfer in progress.
7. **OTA transfer:** authenticated firmware upload through reboot.
8. **Network recovery:** reconnect after access-point or broker interruption.

The firmware emits `POWER_BASELINE` serial timings for Wi-Fi connection, MQTT
connection, and report-task duration. These timestamps help align the current
trace but are not substitutes for electrical measurements.

## Metrics

For each phase record:

- duration;
- mean current;
- peak current;
- minimum input voltage;
- energy or charge consumed;
- Wi-Fi/MQTT success and retry count;
- wind pulses injected versus pulses reported;
- missed or delayed reports.

Use at least ten boot/connect runs and at least thirty minutes of connected
operation. Include three normal reports and one OTA-ready window. Repeat any
candidate optimization under the same supply, RF, sensor, and instrument setup.

## Initial acceptance criteria

- No lost injected anemometer pulses.
- Wind direction retains one-second sampling unless a documented alternative is
  selected.
- MQTT reconnects without a reboot under the tested interruption.
- A retained, expiring OTA request is accepted after the radio is available.
- The OTA transfer completes and the station publishes its running version.
- No new sensor failures, filesystem errors, watchdog resets, or brownout loops.
- Energy reduction is repeatable and greater than measurement uncertainty.

## Planned experiments

1. Capture the unchanged connected baseline and identify the core-selected Wi-Fi
   sleep mode.
2. Explicitly test modem sleep while remaining associated and running the
   one-second wind task.
3. Compare reconnect latency and energy when Wi-Fi is disconnected between
   reports.
4. Test the retained OTA request across each radio-off interval.
5. Evaluate external pulse-counting hardware before any full deep-sleep design.

Do not select the final duty cycle until baseline and candidate traces are
available from the same physical station.