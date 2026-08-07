# ArduinoHA 2.1.0 extensions

The weather-station firmware requires two small capabilities that ArduinoHA
2.1.0 does not provide:

- PrecisionP4 for four-decimal Home Assistant numbers.
- setEntityCategory() so MQTT discovery can publish diagnostic entity metadata.

Apply patches/arduinoha-2.1.0-diagnostics-p4.patch from the root of the
installed home-assistant-integration library before building:

    git apply <path-to-weather-station>/patches/arduinoha-2.1.0-diagnostics-p4.patch

The patch adds one serializer slot only when an entity category is configured,
so existing ArduinoHA entities retain their original discovery payloads and
allocation sizes.