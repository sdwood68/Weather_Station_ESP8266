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

- [ ] Finish updating Arduino IDE, ESP8266 board support, and libraries.
- [x] Restore Arduino CLI visibility of the installed ESP8266 core.
- [x] Confirm the updated sketch compiles with ArduinoHA.
- [x] Add basic firmware-version reporting.
- [x] Replace the static device ID and hostname with hardware-derived values.
- [x] Publish chip ID, MAC address, IP address, hostname, and firmware version
      as Home Assistant diagnostic entities.
- [ ] Verify two devices can connect simultaneously without MQTT discovery,
      entity, hostname, or OTA collisions.
- [x] Implement the non-sleeping OTA button and status handshake.
- [ ] Test the non-sleeping OTA button and status handshake on hardware.
- [ ] Verify timeout, authentication failure, interrupted upload, and reboot
      behavior.
- [x] Add device-specific expiring retained OTA requests in preparation for deep sleep.
- [ ] Evaluate signed firmware enforcement before relying on remote OTA.
