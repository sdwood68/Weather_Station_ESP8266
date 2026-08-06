# Configuration and MQTT Recovery

## Portal and required configuration

The portal starts when WiFi SSID, OTA password, MQTT broker, MQTT username, or
MQTT password is absent. Serial output names missing fields without printing
credentials. It restarts after 15 minutes using a rollover-safe timeout.

`tests/connectivity_reliability_test.cpp` covers the timeout boundary,
`millis()` rollover, complete configuration, and every missing field. A
physical erased-filesystem test remains pending because it deletes credentials:
back up settings or use a spare board, erase LittleFS, confirm portal entry,
wait 15 minutes for restart, then configure and boot normally.

## Hidden SSID

The portal includes **Hidden WiFi SSID (optional; clear to use scanned
selection)**. A non-empty value is copied into WiFiSettings' standard
`/wifi-ssid` credential before connection. The scanned selector and standard
password field remain available. To return to a visible network, clear this
field, select the scanned network, save, and restart. SSIDs are limited to 32
bytes.

## MQTT recovery

Resolution attempts a nonzero literal IP, configured-host DNS, then the
`_home-assistant._tcp` mDNS service. Failure defers startup for 30 seconds;
MQTT is never started with `0.0.0.0`. After 30 seconds disconnected, ArduinoHA
is cleanly disconnected, the broker is resolved again, and MQTT restarts without
rebooting the weather station.

Physical validation remains for DNS success, DNS failure with mDNS success,
complete resolver outage, broker-address change, recovery, and uninterrupted
sensor scheduling.
