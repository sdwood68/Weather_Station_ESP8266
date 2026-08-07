# Changelog

All notable changes to the ESP8266 Weather Station firmware are documented here.

## [Unreleased]

## [0.7.3] - 2026-08-07

- Increased the Rain Gauge Tip Size Home Assistant control to four decimal places.
- Categorized raw rain and wind counts plus OTA, firmware, reset, chip, Wi-Fi MAC, and Wi-Fi RSSI entities as Home Assistant diagnostics.

## [0.7.2] - 2026-08-07

- Increased ArduinoHA entity capacity from its 24-entity ESP8266 default to 40, restoring the Enable OTA button and all later-declared diagnostic entities.

- Replaced separate measurement-unit choices with one persistent Home Assistant regional selector for USA, European Union, and United Kingdom conventions.
- Applied the selected region to temperature, wind, rain, pressure, and elevation reports, including converted rain-tip and station-elevation entry fields.

- Added optional manual hidden-SSID entry to the WiFiSettings portal.
- Added explicit missing-configuration diagnostics and rollover-safe portal
  timeout regression coverage.
- Replaced five-minute MQTT recovery reboots with 30-second DNS/mDNS
  re-resolution and clean ArduinoHA reconnection.
- Documented the 2026-08-05 toolchain and library review.

- Changed the rain-gauge tip-size Home Assistant control to inches per tip.
- Added a persistent mph/km/h wind-speed selector and applied it to sustained
  wind and gust calculations and Home Assistant units.
### Added

- Started firmware 0.9.0 power-baseline work with Wi-Fi connection, MQTT
  connection, and report-task timing diagnostics.
- Added a repeatable electrical measurement procedure and acceptance criteria for
  radio power-reduction experiments.
- Added firmware 0.8.0 tipping-bucket rain reporting with one-minute ASOS
  correction, 60-minute, 3-hour, 6-hour, and 24-hour accumulations, a session
  total, raw tip diagnostics, and startup validity gating.
- Added a persistent Home Assistant rain-tip-size control in millimeters per tip,
  plus rain calculation vectors and calibration and wiring guidance.

## [0.7.1] - 2026-07-25

### Added

- Began firmware 0.7.0 standards-based wind reporting.
- Added two-minute sustained wind, two-minute speed-weighted true direction,
  rolling three-second gust, calm handling, and pulse/sample diagnostics.
- Added platform-independent wind calculation test vectors and a calibration and
  field-validation procedure.
- Added calibrated station pressure, NWS altimeter setting, temperature-adjusted
  sea-level pressure, three-hour pressure change, and rising/falling/steady trend
  reporting with calculation test vectors and validation guidance.
- Added outdoor dew point using the Magnus approximation and NWS heat index
  using the complete simple/Rothfusz/adjustment procedure.
- Added a persistent Home Assistant Station Elevation control for the derived
  altimeter and sea-level pressure calculations, and documented wind sample-count
  diagnostics.

### Fixed

- Prevented invalid or failed AM2315 reads from entering dew-point and heat-index
  calculations, and added finite, sensor-range, and hard output bounds so corrupt
  heat-index values can never be published.

### Changed

- Began firmware 0.6.0 OTA production-hardening work.
- Made MQTT startup wait for a valid broker address.
- Retry configured-IP, DNS, and mDNS broker resolution every 30 seconds instead
  of starting ArduinoHA with `0.0.0.0` or waiting for a device restart.
- Added serial diagnostics for deferred MQTT startup and the selected broker
  address.
- Documented direct-IP Arduino CLI upload as the fallback when Arduino IDE
  network-port discovery leaves `{upload.port.properties.port}` unresolved.

### Verified

- Compiled the initial 0.7.0 wind implementation for NodeMCU 1.0 and
  LOLIN(WEMOS) D1 R2 & mini profiles with ESP8266 core 3.1.2.
- Authenticated direct-IP OTA upload to station `541a1d`, including the expected
  reboot, network recovery, and closure of the temporary OTA service.
### Pending

- Complete authenticated OTA interruption and portal-reset testing.
- Verify hidden-network manual SSID entry and the broker-resolution recovery path
  on physical hardware.
- Diagnose the AM2315 initialization failure seen on the current hardware.
- Evaluate signed firmware enforcement for remote OTA updates.

## [0.5.2] - 2026-07-24

### Fixed

- Prevented invalid or failed AM2315 reads from entering dew-point and heat-index
  calculations, and added finite, sensor-range, and hard output bounds so corrupt
  heat-index values can never be published.

### Changed

- Increased the Home Assistant-triggered OTA window from 120 seconds to 480
  seconds (eight minutes).
- Based the window on twice the 120-second publishing period plus twice a
  two-minute clean-compile allowance; the measured clean compile was 103 seconds.
- Made the serial OTA-window message derive its duration from `OTA_WINDOW_MS`.

### Verified

- Compiled and hash-verified serial uploads to both COM5 and COM6.
- Confirmed both devices publish firmware `0.5.2` and retained `online`
  availability after reboot.
## [0.5.1] - 2026-07-24

### Fixed

- Enabled ArduinoHA extended unique IDs so every Home Assistant entity is
  qualified by its hardware device ID, for example
  `af47f7_firmware_version` and `541a1d_firmware_version`.
- Prevented Home Assistant from ignoring or merging a second station whose
  entity object IDs matched those of the first station.

### Verified

- Compiled and hash-verified serial uploads to both COM5 and COM6.
- Confirmed both devices publish firmware `0.5.1`, retained `online`
  availability, and distinct retained Home Assistant discovery identifiers.
## [0.5.0] - 2026-07-23

### Added

- Device-specific retained OTA request topic at
  `weather_station/<chip-id>/ota/request`.
- Retained OTA status topic at `weather_station/<chip-id>/ota/status`.
- JSON request validation for `requested`, `expires`, and `target_version`.
- NTP-based request expiration checking and deferred processing while the clock
  synchronizes.

### Security

- Clear an accepted retained request before opening the OTA window.
- Reject malformed, expired, incomplete, and already-running-version requests.

### Verified

- Compiled, flashed, hash-verified, and boot-tested on the COM6 ESP8266.
- Confirmed Wi-Fi, mDNS broker discovery, MQTT connection, diagnostics, and
  scheduled sensor reporting after the update.
- Verified two physical ESP8266 stations simultaneously with distinct chip IDs,
  IP addresses, hostnames, MQTT namespaces, and discovery identities.
- Verified retained Home Assistant discovery separation, OTA request timeout and
  rejection paths, MQTT last-will recovery, and OTA authentication failure.

## [0.4.3] - 2026-07-23

### Security

- Removed the configuration-portal password from serial output.

### Added

- Non-sensitive MQTT connection logging for hardware validation.

## [0.4.2] - 2026-07-23

### Fixed

- Prevented invalid or failed AM2315 reads from entering dew-point and heat-index
  calculations, and added finite, sensor-range, and hard output bounds so corrupt
  heat-index values can never be published.

### Changed

- Hardened OTA handling into persistent `standby`, `ready`, `updating`,
  `success`, `error`, and `timeout` states.
- Made sensor-task suspension and restoration idempotent during OTA.
- Preserved OTA state across MQTT reconnects.
- Added a restart after five minutes without MQTT so the broker hostname is
  resolved again after an address change.

## [0.4.1] - 2026-07-23

### Added

- Configurable friendly device name.
- Home Assistant diagnostics for firmware version, chip ID, Wi-Fi MAC address,
  IP address, hostname, reset reason, and Wi-Fi RSSI.
- Shared online/offline availability with a retained MQTT last will.
- Hardware model and improved Home Assistant device metadata.

## [0.4.0] - 2026-07-23

### Fixed

- Prevented invalid or failed AM2315 reads from entering dew-point and heat-index
  calculations, and added finite, sensor-range, and hard output bounds so corrupt
  heat-index values can never be published.

### Changed

- Replaced MAC-based MQTT paths with the six-character ESP8266 chip ID.
- Changed the ArduinoHA data prefix from `aha` to `weather_station`.
- Standardized entity IDs as lowercase `snake_case`.
- Established the topic format
  `weather_station/<chip-id>/<entity>/<topic-type>`.

### Migration

- This release creates new Home Assistant discovery entities. Old retained
  MAC-based discovery and state topics must be removed as described in
  [VALIDATION.md](VALIDATION.md).

## [0.3.2] - 2026-07-23

### Documentation

- Defined the MQTT naming contract and updated roadmap checkpoints.

## [0.3.1] - 2026-07-23

### Added

- Hardware-derived hostname and Home Assistant identity foundations.
- WiFiSettings configuration for MQTT broker, credentials, OTA password, and
  HTTP port.
- Automatic configuration portal when required settings are missing or Wi-Fi
  cannot connect within one minute.
- Fifteen-minute portal reset timer.
- DNS and mDNS MQTT broker resolution.

### Fixed

- Prevented invalid or failed AM2315 reads from entering dew-point and heat-index
  calculations, and added finite, sensor-range, and hard output bounds so corrupt
  heat-index values can never be published.

### Changed

- Adopted Home Assistant conventional units for Fahrenheit, humidity, wind
  direction, pressure, and wind speed.

## [0.2.0] - 2026-07-22

### Added

- Home Assistant MQTT button for enabling a 120-second ArduinoOTA window.
- OTA status and firmware-version sensors.
- Sensor-task suspension while an OTA upload is active.
- OTA password support outside source-controlled defaults.
