# ESP8266_MQTT_Weather_Station
 ESP8266 Weather Station with MQTT

See [ROADMAP.md](ROADMAP.md) for planned work and [VALIDATION.md](VALIDATION.md) for hardware results and MQTT migration notes.

See [CHANGELOG.md](CHANGELOG.md) for release history.

Regional measurement presets and Home Assistant calibration-field behavior are documented in [UNIT_SYSTEMS.md](UNIT_SYSTEMS.md).

Hardware ownership, electrical safety, wiring, and physical validation are
documented in [HARDWARE_README.md](HARDWARE_README.md) and tracked in
[HARDWARE_ROADMAP.md](HARDWARE_ROADMAP.md) for sharing with the Weather Station
Hardware Project.

Configuration portal, hidden-SSID, and MQTT recovery behavior is documented in
[CONNECTIVITY_RELIABILITY.md](CONNECTIVITY_RELIABILITY.md). Tool versions and
upgrade recommendations are in [DEPENDENCY_REVIEW.md](DEPENDENCY_REVIEW.md).
## Home Assistant entity capacity

ArduinoHA is configured for 40 registered entities. This capacity is explicit
because the library default on ESP8266 is 24 and silently ignores entities
declared after that limit. Keep the capacity above the number of declared Home
Assistant entities; otherwise later entities such as **Enable OTA** will not
publish discovery data or subscribe to command topics.
