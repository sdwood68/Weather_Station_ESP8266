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