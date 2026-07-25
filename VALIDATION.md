# Firmware validation

## Firmware 0.4.3 hardware results

Validated on the ESP8266 attached to COM6:

- Flash identity: MAC `48:3f:da:54:1a:1d`, chip ID `541a1d`.
- Hostname and configuration SSID: `weather-541a1d`.
- Wi-Fi connection: successful; DHCP address `192.168.12.192`.
- Home Assistant mDNS discovery: successful; resolved broker host to `192.168.12.163`.
- BMP280 initialization and pressure/temperature reporting: successful.
- AHT20 initialization and humidity/temperature reporting: successful.
- Wind reporting scheduler: successful.
- AM2315 initialization: failed on the tested hardware and needs wiring/sensor diagnosis.
- Arduino CLI compile and serial upload: successful; flash hash verified.
- Sensitive portal-password serial output discovered during testing and removed in 0.4.3.

The following tests still require deliberate interaction and are not yet claimed:

- Home Assistant discovery inspection after clearing old retained discovery records.
- MQTT OTA button, ready state, timeout, incorrect password, interrupted transfer, and successful OTA reboot.
- MQTT last-will behavior after an ungraceful power loss.
- Missing-configuration portal entry and 15-minute portal reset.

## Multi-device collision analysis

Only one ESP8266 is connected. COM7 is a GD32 USB serial device, not a second
ESP8266, so a physical two-station test has not been completed.

The naming algorithm is deterministic. Different 24-bit ESP8266 chip IDs produce
separate resources:

```text
weather_station/541a1d/temperature/stat_t
weather_station/a1b2c3/temperature/stat_t

weather-541a1d.local
weather-a1b2c3.local
```

ArduinoHA discovery also includes the device ID, so `541a1d` and `a1b2c3` do not
share discovery topics or MQTT client identities. This static analysis does not
replace a physical two-device test.

## Home Assistant migration

Firmware 0.4.0 changed both the ArduinoHA device ID and entity IDs. Before judging
discovery results, remove the retained records for the former MAC-based device.
Use MQTT Explorer or equivalent broker tooling to delete retained messages under:

```text
aha/483fda541a1d/#
homeassistant/sensor/483fda541a1d/#
homeassistant/button/483fda541a1d/#
```

The new device publishes under:

```text
weather_station/541a1d/#
homeassistant/sensor/541a1d/#
homeassistant/button/541a1d/#
```

Do not publish an empty retained message to a wildcard topic; MQTT deletion must
target each concrete retained topic or be performed through a broker browser.
After cleanup, restart MQTT discovery or reload the MQTT integration in Home
Assistant if stale entities remain.
## Firmware 0.5.0 retained OTA request

Validated on COM6 after flashing firmware 0.5.0:

- Flash completed and the image hash was verified.
- Device rebooted as `weather-541a1d` and rejoined Wi-Fi.
- Home Assistant/broker mDNS resolution succeeded.
- MQTT connected and diagnostic publication callback ran.
- No configuration password was printed to serial.
- BMP280/AHT20 and scheduled weather reporting remained operational.

The request parser and topic subscription compile and boot successfully. Publishing
live retained test requests still requires broker credentials or a Home Assistant
automation and has not been claimed as a completed failure-path test.

Request topic and status topic:

```text
weather_station/541a1d/ota/request
weather_station/541a1d/ota/status
```

Example request:

```json
{
  "requested": true,
  "expires": 1784563200,
  "target_version": "0.5.1"
}
```

The firmware clears an accepted retained request before opening the OTA window.
Malformed, expired, and already-running-version requests do not enable OTA. If
NTP has not synchronized yet, the request is held in RAM and evaluated when a
valid clock becomes available.
## Two-device validation

Validated two physical ESP8266 stations operating simultaneously on 2026-07-24:

| Chip ID | MAC address | IP address | Hostname | MQTT |
| --- | --- | --- | --- | --- |
| `541a1d` | `48:3f:da:54:1a:1d` | `192.168.12.192` | `weather-541a1d` | Existing station online |
| `af47f7` | `e0:98:06:af:47:f7` | `192.168.12.161` | `weather-af47f7` | Connection confirmed by serial log |

Both devices replied to three simultaneous reachability checks with zero packet
loss, and the ARP table associated each IP address with the expected distinct
MAC address. The second station resolved the MQTT broker through mDNS and logged
`MQTT connected; publishing discovery and diagnostics` while the first station
remained reachable.

Their firmware-derived namespaces are distinct:

```text
weather_station/541a1d/#
weather_station/af47f7/#
```

Their Wi-Fi/mDNS/ArduinoOTA hostnames and ArduinoHA discovery device IDs are also
derived from those different chip IDs, so no hostname, MQTT data-topic, OTA, or
discovery identity collision was observed. The second board has no sensors
attached; BMP280, AHT20, and AM2315 initialization failures are expected on that
test unit, while MQTT diagnostics and zero-value wind reporting remain active.
## Item 5 MQTT and OTA validation

Validated against both physical stations on 2026-07-24 using retained broker
state and the sensorless `af47f7` unit for disruptive tests:

- Both devices published firmware `0.5.0` and retained `online` availability under
  separate `weather_station/<chip-id>/...` namespaces.
- Home Assistant retained discovery contained 16 sensor configurations and one
  `enable_ota` button for each device ID, with no overlapping discovery topics.
- A malformed retained OTA request did not enable OTA and remained available for
  diagnosis until explicitly cleared.
- A valid request for target `0.5.1` was accepted, cleared from the retained
  request topic, and changed status from `standby` to `ready`.
- With no upload, the 120-second window changed status from `ready` to `timeout`.
- An already-running `0.5.0` request and an expired `0.5.1` request were both
  cleared without reopening OTA; status remained `timeout`.
- An abrupt serial reflash produced retained availability transitions
  `online -> offline -> online`, confirming MQTT last-will behavior.
- A deliberately incorrect OTA password was rejected by the network uploader;
  the device published `error`, remained online, and did not affect `541a1d`.

Still requiring explicit credentials or destructive configuration changes:

- Successful authenticated OTA upload and running-version confirmation.
- Interrupted authenticated OTA upload and recovery.
- Home Assistant UI button press (the retained MQTT request path is validated).
- Missing-configuration portal entry and 15-minute reset after clearing settings.
## Firmware 0.5.1 Home Assistant identity fix

The initial two-device broker test exposed a Home Assistant registry collision:
discovery topics were device-specific, but ArduinoHA published unqualified entity
unique IDs such as `firmware_version` for both devices. Home Assistant could
therefore ignore or merge the second station even though MQTT topics were separate.

Firmware 0.5.1 enables ArduinoHA extended unique IDs. Verified retained discovery
examples are now:

```text
541a1d_firmware_version
af47f7_firmware_version
```

Both COM5 and COM6 were flashed and hash-verified. The broker subsequently
reported firmware `0.5.1`, `online` availability, and distinct discovery payloads
for both devices.
## Firmware 0.5.2 OTA window

A clean Arduino CLI build took 103 seconds. The OTA window was increased to 480
seconds: twice the 120-second reporting period plus twice a rounded two-minute
compile allowance. Both COM5 and COM6 were flashed and hash-verified, then
confirmed online while reporting firmware `0.5.2`.

## Firmware 0.6.0 OTA upload procedure

Start each update by targeting exactly one station through its Home Assistant
**Enable OTA** button or its retained request topic. Confirm that the selected
station reports `ready` before uploading.

Arduino IDE network discovery may list `weather-<chip-id>` while still leaving
`{upload.port.properties.port}` unresolved. The supported fallback is a direct-IP
Arduino CLI upload:

```powershell
arduino-cli compile --fqbn esp8266:esp8266:d1_mini Weather_Station_ESP8266
arduino-cli upload --fqbn esp8266:esp8266:d1_mini `
  --protocol network `
  --port 192.168.12.192 `
  --upload-field password="<OTA password>" `
  Weather_Station_ESP8266
```

Replace the IP address, board FQBN, and password for the selected station. Keep
the password out of shell history when possible. If the installed Arduino CLI
does not accept `--upload-field`, use a temporary environment variable or the
IDE password prompt supported by the installed ESP8266 core rather than adding
the credential to this repository.

For every OTA attempt, record the device chip ID, source and target versions,
upload method, and observed status sequence. A successful update is not complete
until the station reboots, reconnects to MQTT, publishes `online`, and reports
firmware `0.6.0`. After an authentication or transfer error, confirm that status
is `error`, sensor tasks resume, MQTT remains usable, and a new OTA request can
open another window.
### Initial 0.6.0 hardware results — 2026-07-25

Validated on the sensor-equipped station attached to COM6:

- Both `esp8266:esp8266:d1_mini` and `esp8266:esp8266:nodemcuv2` profiles compiled
  successfully with ESP8266 core 3.1.2.
- Serial upload to chip ID `541a1d` (MAC `48:3f:da:54:1a:1d`) completed and the
  flashed image hash was verified.
- Before the upload, a controlled reboot confirmed Wi-Fi at `192.168.12.192`,
  broker resolution at `192.168.12.163`, MQTT connection, BMP280/AHT20 readings,
  and scheduled weather reporting.
- After the upload, the station recovered at `192.168.12.192` with four of four
  ICMP replies and zero packet loss.
- Arduino CLI 1.3.1 accepts `--upload-field password=...`, confirming the
  documented direct-IP command syntax.
- `arduino-cli board list --discovery-timeout 10s` exposed COM1 and COM6 but no
  network port, reproducing the network-discovery failure for
  `weather-541a1d`.
- A direct-IP upload without `--protocol network` incorrectly selected the serial
  `esptool.py` recipe; the documented command now selects the OTA recipe explicitly.
- With `--protocol network`, the station answered the OTA authentication exchange
  but rejected the supplied credential. The installed ESP8266 3.1.2 upload recipe
  confirms that Arduino CLI field `password` maps to `espota.py --auth`; the
  station's stored OTA password therefore differs from the tested value.
- Anonymous reads of retained MQTT version, availability, and OTA status were
  rejected by the broker as expected; authenticated retained-state confirmation
  remains pending.

Still pending: authenticated direct-IP OTA upload, retained `0.6.0` version and
`online` confirmation, OTA timeout, interrupted-transfer recovery, and signed
firmware evaluation.
### Authenticated 0.6.0 OTA success — 2026-07-25

After station `541a1d` reported `ready`, Arduino CLI uploaded by direct IP with
`--protocol network` and the configured OTA password. Authentication completed
with `OK` and the complete image transferred successfully. The password was
supplied only to the uploader and was not written to the repository.

The first post-upload ping timed out during the expected reboot. The following
nine replies confirmed recovery at `192.168.12.192`. A subsequent five-second
Arduino CLI discovery scan exposed no network port, confirming that the temporary
OTA service closed after reboot. Home Assistant subsequently reported firmware
`0.6.0` and OTA status `standby`,
confirming the running version and expected post-reboot state. Shared MQTT
availability is configured as entity/device availability metadata rather than a
standalone Home Assistant sensor, so no separate `online` entity is expected;
entity availability or the retained availability topic must be inspected instead.
