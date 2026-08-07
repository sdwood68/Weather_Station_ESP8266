# Regional Unit Systems

Home Assistant exposes one **Measurement Unit System** selector. Its value is
stored in flash and controls every weather measurement plus the units accepted
by the calibration entry fields.

| Setting | Temperature | Wind | Rain | Pressure | Elevation |
| --- | --- | --- | --- | --- | --- |
| USA | °F | mph | in | inHg | ft |
| European Union | °C | km/h | mm | hPa | m |
| United Kingdom | °C | mph | mm | hPa | m |

Changing the selector restarts the station after saving the choice. The restart
is intentional: it republishes Home Assistant MQTT discovery with consistent
unit metadata before new values are sent.

The **Rain Gauge Tip Size** field changes between inches per tip and millimetres
per tip, and **Station Elevation** changes between feet and metres. Existing
calibration values are converted for display and entry; the firmware continues
to store rain calibration in micrometres, elevation in metres, rain history in
inches, wind speed in mph, pressure in hPa, and calculation temperatures in
their canonical units. Therefore changing regions does not reinterpret or erase
calibration data or accumulated rain.

The old independent wind-speed unit selector is replaced by this regional
selector. Diagnostic counts, humidity, wind direction, and pressure-trend text
are unitless or have the same representation in all three systems.