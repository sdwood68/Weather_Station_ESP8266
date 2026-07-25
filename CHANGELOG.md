# Changelog

All notable changes to the ESP8266 Weather Station firmware are documented here.

## [Unreleased]

### Pending

- Complete authenticated OTA success/interruption and portal-reset testing.

- Diagnose the AM2315 initialization failure seen on the current hardware.
- Evaluate signed firmware enforcement for remote OTA updates.

## [0.5.2] - 2026-07-24

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

### Changed

- Adopted Home Assistant conventional units for Fahrenheit, humidity, wind
  direction, pressure, and wind speed.

## [0.2.0] - 2026-07-22

### Added

- Home Assistant MQTT button for enabling a 120-second ArduinoOTA window.
- OTA status and firmware-version sensors.
- Sensor-task suspension while an OTA upload is active.
- OTA password support outside source-controlled defaults.
