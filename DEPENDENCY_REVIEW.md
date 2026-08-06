# Toolchain and Library Review

Reviewed 2026-08-05. No global packages were changed.

| Component | Installed | Current reviewed | Decision |
| --- | ---: | ---: | --- |
| Arduino CLI | 1.3.1 | 1.5.1 stable | Test isolated upgrade |
| Arduino IDE | Not detected by CLI | 2.3.9 | Confirm desktop installation |
| ESP8266 core | 3.1.2 | 3.1.2 | Keep |
| ESP-WiFiSettings | 3.10.1 | 3.10.1 installed | Keep; no fork needed |
| ArduinoHA | 2.1.0 | 2.1.0 | Keep |
| ArduinoJson | 7.4.3 | 7.4.3 installed | Keep |
| TaskScheduler | 4.0.8 | 4.0.8 | Keep |
| Adafruit AM2315 | 2.2.3 | 2.2.3 | Keep; hardware issue remains |
| Adafruit BusIO | 1.17.4 | 1.17.4 installed | Keep |
| PubSubClient | 2.8 | 2.8 installed | Keep |

Upgrade Arduino CLI/IDE alongside the existing installation, pin ESP8266 core
3.1.2 and current libraries, compile both profiles, compare sizes and warnings,
then perform serial boot and authenticated OTA tests before adoption.

Primary sources:

- https://github.com/arduino/arduino-cli/releases
- https://github.com/arduino/arduino-ide/releases
- https://github.com/esp8266/Arduino
- https://github.com/Juerd/ESP-WiFiSettings
- https://github.com/dawidchyrzynski/arduino-home-assistant/releases
- https://github.com/arkhipenko/TaskScheduler
- https://github.com/adafruit/Adafruit_AM2315/releases
