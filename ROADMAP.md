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

## Future deep-sleep support

An ESP8266 cannot receive Wi-Fi or MQTT commands during deep sleep. MQTT cannot
wake it; waking from deep sleep is effectively a reboot. The OTA workflow must
therefore be checked during each normal sensor wake cycle.

### Sleep-compatible OTA request

1. Home Assistant publishes an expiring, retained request to a dedicated topic,
   such as `weather_station/<chip-id>/ota/request`.
2. The station wakes on its normal sensor schedule, connects to Wi-Fi and MQTT,
   publishes its readings, and briefly waits for commands.
3. If no valid request arrives, it returns to deep sleep.
4. If a valid request arrives, it immediately clears the retained request,
   publishes `ready`, and remains awake for the OTA window.
5. After upload, the station reboots and publishes the running firmware version.
6. Once normal operation is confirmed, it resumes deep sleep.

Example request payload:

```json
{
  "requested": true,
  "expires": 1784563200,
  "target_version": "0.2.0"
}
```

The expiration timestamp prevents an old request from enabling OTA much later.
Do not retain the ArduinoHA button command itself. Instead, have a Home Assistant
automation translate the button press into the timestamped retained request.

Expected deep-sleep handshake:

```text
Home Assistant retained request
  -> scheduled device wake
  -> request accepted and retained request cleared
  -> ready
  -> updating
  -> reboot
  -> running <target firmware version>
  -> resume deep sleep
```

### Possible wake strategies

- Check for OTA requests briefly after every sensor wake. Update latency will be
  no longer than the normal sensor interval.
- Add a scheduled maintenance window during which the device remains awake
  longer for updates.
- Use external GPIO wake hardware if immediate updates are ever required. MQTT
  by itself cannot wake a sleeping ESP8266.

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

- [ ] Finish updating Arduino IDE, ESP8266 board support, and libraries.
- [ ] Add manual SSID entry to the ESP-WiFiSettings configuration portal so a
      freshly erased device can be configured for a hidden network. Prefer a
      small extension to the current library over migrating the complete portal.
      AutoConnect supports manual/hidden SSID entry, but its latest release was
      1.4.2 on 2023-01-31 and its most recent repository commit was 2023-02-22,
      so it is not considered actively maintained.
- [ ] Verify missing-configuration portal entry and the 15-minute portal reset.
- [ ] Make MQTT broker resolution resilient: retry configured DNS and mDNS
      without starting MQTT with `0.0.0.0`, log each resolution path, and use a
      DHCP reservation or router-provided local DNS name as the recommended
      production configuration.
- [ ] Diagnose the AM2315 initialization failure on the current hardware.

Exit criteria: both board profiles compile; a flash-erased NodeMCU can be
configured using either a scanned or manually entered SSID; broker-resolution
failure recovers without exposing an unnecessary AP; portal timeout is verified.

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
- [ ] Document calibration constants and a repeatable bench/field validation
      procedure.

Exit criteria: formulas and intervals are documented with primary references,
test vectors pass, and Home Assistant reports clearly named standards-based wind
measurements.

### Release 0.8.0 - Radio power reduction and sleep readiness (Priority 4)

- [ ] Measure baseline current consumption during sensing, Wi-Fi connection, MQTT
      publication, idle time, and OTA readiness.
- [ ] Turn off or modem-sleep the Wi-Fi radio between reporting events while
      preserving continuous anemometer pulse counting and reliable reconnects.
- [ ] Measure connection latency and energy savings before selecting the final
      radio duty cycle.
- [ ] Validate the retained, expiring OTA-request workflow across a radio-off or
      sleep/wake cycle.
- [ ] Evaluate full deep sleep separately: the ESP8266 cannot count wind pulses
      or receive MQTT while asleep, so it requires external pulse-counting/wake
      hardware or an explicitly accepted loss of wind observations.

Exit criteria: measured power savings and operational tradeoffs are documented;
sensor reporting, MQTT availability, and OTA requests recover reliably after each
radio-off interval.

### Release process for every firmware version

- [ ] Increment `FIRMWARE_VERSION` only after compilation and target-device
      validation succeed.
- [ ] Update `CHANGELOG.md`, validation evidence, and completed roadmap items.
- [ ] Compile both NodeMCU and D1 Mini profiles when shared code changes.
- [ ] Commit only after the applicable release exit criteria pass.