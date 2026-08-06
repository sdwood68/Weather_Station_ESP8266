# Weather Station Roadmap

## MQTT-triggered OTA updates

Use MQTT and Home Assistant as the control handshake for OTA updates. The
firmware image itself will continue to be transferred with `ArduinoOTA`.

### Initial implementation

- Add an ArduinoHA `HAButton` entity named **Enable OTA**.
- Add an OTA status sensor with these states:
  - `standby`
  - `ready`
  - `updating`
  - `success`
  - `error`
  - `timeout`
- When the button command is received, publish `ready` and service
  `ArduinoOTA.handle()` for a limited window (initially 120 seconds).
- Set the button command to non-retained so an old command cannot unexpectedly
  reopen the OTA window after a reconnect or reboot.
- Suspend scheduled sensor work when an upload begins so it cannot interfere
  with the transfer.
- Publish the running firmware version after every boot. Treat that version
  announcement as the reliable final confirmation that an update succeeded.
- Protect OTA with a strong password stored outside source control. Investigate
  signed firmware images because ESP8266 OTA password authentication alone is
  not strong security.

Expected handshake:

```text
standby -> ready -> updating -> reboot -> running <firmware version>
```

## Multi-device identity and version reporting

The firmware must support multiple ESP8266 weather stations without MQTT
discovery, entity, hostname, or OTA collisions. Static names such as
`ESP_Weather_Station` must not be used as the device's unique identity.

### MQTT naming contract

Use a stable, product-specific MQTT hierarchy. The topic identifies the product,
the individual ESP8266, the entity, and the message type:

```text
weather_station/<chip-id>/<entity>/<topic-type>
```

For a station whose chip ID is `541a1d`, examples are:

```text
weather_station/541a1d/temperature/stat_t
weather_station/541a1d/humidity/stat_t
weather_station/541a1d/ota_status/stat_t
weather_station/541a1d/enable_ota/cmd_t
```

Naming rules:

- Use `weather_station` as the ArduinoHA MQTT data prefix.
- Use the lowercase, six-character ESP8266 chip ID as the per-device topic
  segment.
- Use stable lowercase `snake_case` ArduinoHA entity IDs.
- Use `weather-<chip-id>` as the Wi-Fi, mDNS, and ArduinoOTA hostname.
- Keep `homeassistant` as the Home Assistant MQTT discovery prefix.
- Publish numeric sensor values as payloads; publish units through discovery
  metadata rather than embedding values or units in topic names.
- Treat the complete `weather_station/<chip-id>` path as the product-plus-hardware
  MQTT identity. ArduinoHA uses its device unique ID as the second data-topic
  segment, so the chip ID must occupy that segment to produce this exact topic
  hierarchy without modifying the library.

### Identity values

Derive a stable identity during startup from hardware rather than source code:

- **Chip ID:** Read `ESP.getChipId()` and format it as a six-character uppercase
  hexadecimal string.
- **MAC address:** Read the station MAC address from `WiFi.macAddress()`.
- **Device unique ID:** Build a stable value such as
  `weather_station_<chip-id>`. Use it for the ArduinoHA `HADevice` unique ID
  and MQTT client identity.
- **Hostname:** Build a per-device hostname such as `weather-<chip-id>`. Use it
  for Wi-Fi, mDNS, and ArduinoOTA.
- **Friendly location/name:** Keep this user-configurable through WiFiSettings,
  for example `Back Yard` or `Greenhouse`. It must not replace the immutable
  hardware-derived unique ID.

Entity IDs such as `Temperature`, `Humidity`, and `EnableOTA` only need to
be unique within their parent device. The parent `HADevice` ID must be unique
across every deployed station.

### Version values

Maintain explicit build metadata near the top of the main sketch:

```cpp
#define FIRMWARE_VERSION "0.2.0"
#define HARDWARE_MODEL "ESP8266 Weather Station"
```

Increment `FIRMWARE_VERSION` for every deployed firmware change. In the future,
the build process may also inject a Git commit identifier or build timestamp,
but the human-readable firmware version remains the primary release identifier.

### Home Assistant reporting

Expose the following diagnostic information through ArduinoHA and MQTT
discovery:

- Firmware version
- ESP8266 chip ID
- Wi-Fi MAC address
- Current IP address
- Device hostname
- Optional reset reason and Wi-Fi RSSI

Also populate the corresponding `HADevice` metadata:

- Unique ID: hardware-derived device ID
- Name: configured friendly name plus chip ID when needed
- Manufacturer
- Hardware model
- Software/firmware version
- Configuration URL, if a device configuration page is available

Publish identity and version values whenever MQTT connects, including after an
OTA reboot. Configure diagnostic values as retained state where supported so
Home Assistant can display them immediately after restarting.

Expected topic/device separation:

```text
weather_station_A1B2C3
  -> temperature
  -> humidity
  -> wind
  -> firmware version
  -> chip ID
  -> Enable OTA

weather_station_D4E5F6
  -> temperature
  -> humidity
  -> wind
  -> firmware version
  -> chip ID
  -> Enable OTA
```

### OTA targeting

Each Home Assistant OTA button and status sensor must belong to one uniquely
identified device. Any future retained OTA request topic must include the device
ID:

```text
weather_station/<chip-id>/ota/request
weather_station/<chip-id>/ota/status
```

Home Assistant automations must target a specific device ID and expected
firmware version. A broadcast OTA request should not be implemented.

## Future sleep support (deferred)

Radio sleep, retained OTA behavior across sleep, and external pulse-counting or
wake requirements are tracked under **Deferred hardware-dependent firmware and
validation**. Hardware architecture decisions are owned by
[HARDWARE_ROADMAP.md](HARDWARE_ROADMAP.md).

## Hardware project boundary

Electrical, mechanical, sensor-interface, board-selection, power-integrity, and
physical calibration work is owned by the Weather Station Hardware Project and
tracked in [HARDWARE_ROADMAP.md](HARDWARE_ROADMAP.md). Current interfaces and
safety notes are in [HARDWARE_README.md](HARDWARE_README.md).

Firmware tasks that depend on revised hardware are intentionally deferred here.
They are not active release work until the hardware project provides a released
revision, pin map, electrical limits, calibration data, and test results.

### Deferred hardware-dependent firmware and validation

- [ ] After corrected AM2315 wiring and captured bus results are delivered, add
      bounded initialization retries, checked transactions, CRC validation, bus
      recovery, and Home Assistant failure diagnostics.
- [ ] After wind calibration is delivered, validate pulse scaling, vane ADC
      thresholds, true-north offset, sustained wind, and gust with physical data.
- [ ] After rain calibration is delivered, validate debounce, exact tip counts,
      controlled-volume and high-rate behavior, history thresholds, and rollover.
- [ ] After the power design is delivered, publish voltage, current, power,
      battery, and supervisor/brownout diagnostics.
- [ ] After board profiles are released, isolate platform-specific APIs, add the
      ESP32 implementation, preserve MQTT/configuration compatibility, and
      compile and physically test both processor families.
- [ ] After power traces and pulse-retention requirements are delivered,
      implement and validate modem sleep, radio-off intervals, retained OTA
      requests across sleep, and any external pulse-counting/wake interface.

Resume criteria: the applicable hardware handoff in HARDWARE_README.md is
complete and identified by hardware revision.

## Implementation checkpoints

### Completed

- [x] Restore Arduino CLI visibility of the installed ESP8266 core.
- [x] Confirm the updated sketch compiles with ArduinoHA.
- [x] Add firmware-version reporting and Home Assistant device metadata.
- [x] Replace static identity and hostname values with hardware-derived values.
- [x] Adopt `weather_station/<chip-id>/<entity>/<topic-type>` and lowercase
      `snake_case` entity IDs.
- [x] Add a configurable friendly device name without changing immutable identity.
- [x] Publish chip ID, MAC address, IP address, hostname, reset reason, RSSI, and
      firmware version as Home Assistant diagnostic entities.
- [x] Add shared MQTT availability and a retained offline last will.
- [x] Implement the non-sleeping OTA button and status state machine.
- [x] Suspend sensor tasks during OTA and restore them after an OTA error.
- [x] Add device-specific expiring retained OTA requests in preparation for deep sleep.
- [x] Compile, flash, hash-verify, and boot-test firmware 0.5.0 on the COM6 ESP8266.
- [x] Document Home Assistant topic migration and current hardware validation.

### Release 0.5.3 - Configuration and connectivity reliability (Priority 1)

- [x] Review the installed Arduino CLI, ESP8266 core, and direct dependencies;
      record versions and recommendations in DEPENDENCY_REVIEW.md.
- [ ] Test Arduino CLI 1.5.1 and Arduino IDE 2.3.9 alongside the working
      installation before adopting the tool upgrade.
- [x] Add optional manual hidden-SSID entry without forking ESP-WiFiSettings.
- [x] Verify missing-configuration and rollover-safe 15-minute portal timeout
      decisions with platform-independent regression tests.
- [ ] Perform the destructive erased-filesystem portal and 15-minute restart test
      on a spare or backed-up board.
- [x] Retry configured DNS and Home Assistant mDNS without using 0.0.0.0; after
      a 30-second MQTT outage, disconnect, re-resolve, and reconnect without a
      controller reboot.
- [ ] Physically fault DNS, mDNS, and the broker to verify recovery and continuous
      sensor scheduling on target hardware.
- [ ] Hardware dependency: AM2315 recovery remains deferred until unsafe I2C
      pull-ups are corrected and captured-bus results are delivered.

Exit criteria: both board profiles compile; a flash-erased board accepts scanned
or manual SSID configuration; resolver fault injection recovers without an
unnecessary AP or reboot; and the physical portal timeout is verified.

### Release 0.6.0 - OTA production hardening (Priority 2)

- [x] Test the non-sleeping OTA button and status handshake through Home Assistant, including a successful Arduino IDE OTA upload.
- [ ] Verify OTA timeout, authentication failure, interrupted upload, successful
      reboot, and running-version confirmation.
- [x] Diagnose Arduino IDE network-port discovery that leaves
      `{upload.port.properties.port}` unresolved, and document direct-IP Arduino
      CLI upload as the supported fallback.
- [ ] Evaluate and, if practical, enforce signed firmware before relying on
      unattended remote OTA.

Exit criteria: a Home Assistant request can target either physical device, the
authenticated update completes and reports the new version, interruption is
recoverable, and the supported IDE/CLI procedure is documented.

### Release 0.7.0 - Standards-based wind reporting (Priority 3)

- [ ] Review the wind-speed, vector, direction, and gust calculations against
      current NOAA/NWS and American Meteorological Society practices.
- [ ] Define sampling, averaging, calm-wind, gust-duration, and reporting
      intervals explicitly, including how the two-minute MQTT report relates to
      the standards-based observation periods.
- [ ] Implement the selected calculations and publish enough diagnostic data to
      validate them against captured anemometer pulses.
- [x] Document calibration constants and the software test procedure; physical
      wind calibration is deferred to the hardware-dependent section above.
- [ ] Report barometric pressure in U.S. conventional units (inHg), with a
      documented conversion from the sensor's native pressure value and validation
      against known test values.

Exit criteria: formulas and intervals are documented with primary references,
test vectors pass, and Home Assistant reports clearly named standards-based wind
measurements.

### Release 0.8.0 - NWS-guided rain-gauge reporting (Priority 4)

- [x] Count and debounce tipping-bucket closures on the rain-gauge GPIO without
      blocking wind pulse collection.
- [x] Process discrete one-minute tip counts with the ASOS heated-tipping-bucket
      correction and retain unrounded values for cumulative calculations.
- [x] Add a persistent Home Assistant control for inches per tip, defaulting to
      0.010 inch, and reset history when calibration changes.
- [x] Publish one-minute raw tip count, corrected one-minute rain, latest 60-minute,
      3-hour, 6-hour, and 24-hour rain, plus a clearly named session total.
- [x] Add calculation vectors and document validity gating, calibration, wiring,
      ASOS limitations, and the difference between rolling HA values and official
      METAR, SHEF, or climate products in `RAIN_REPORTING.md`.
- Physical rain validation is deferred to the hardware-dependent section above.

Software exit criteria: both ESP8266 profiles compile, calculation vectors pass,
and Home Assistant reports correctly gated accumulations without claiming
aviation-grade ASOS status. Physical acceptance is owned by the hardware project
and is not an active firmware release blocker until updated hardware is delivered.

### Release 0.9.0 - Radio power reduction and sleep readiness (deferred)

All 0.9.0 implementation and validation is hardware-dependent. It is tracked
under **Deferred hardware-dependent firmware and validation** and resumes only
after the hardware project supplies power traces, measurement points, supported
power states, and a decision about continuous pulse counting.

### Release process for every firmware version

- [ ] Increment `FIRMWARE_VERSION` only after compilation and target-device
      validation succeed.
- [ ] Update `CHANGELOG.md`, validation evidence, and completed roadmap items.
- [ ] Compile both NodeMCU and D1 Mini profiles when shared code changes.
- [ ] Commit only after the applicable release exit criteria pass.