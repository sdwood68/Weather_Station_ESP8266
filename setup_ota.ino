/******************************************************************************/
/* MQTT-authorized Arduino OTA window                                          */
/******************************************************************************/

void publishOtaStatus(const char* status) {
  otaStatus.setValue(status);
  mqtt.loop();
}

void onMqttConnected() {
  firmwareVersion.setValue(FIRMWARE_VERSION);
  publishOtaStatus(otaEnabled ? "ready" : "standby");
}

void onOtaButton(HAButton* sender) {
  (void)sender;
  otaEnabled = true;
  otaInProgress = false;
  otaDeadline = millis() + OTA_WINDOW_MS;
  publishOtaStatus("ready");
  Serial.println(F("OTA enabled for 120 seconds"));
}

void setup_ota() {
  ArduinoOTA.setHostname(WiFiSettings.hostname.c_str());

  if (otaPassword.length() > 0) {
    ArduinoOTA.setPassword(otaPassword.c_str());
  } else {
    Serial.println(F("WARNING: OTA password is not configured"));
  }

  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    wind.disable();
    report.disable();
    publishOtaStatus("updating");
    Serial.println(F("OTA update started"));
  });

  ArduinoOTA.onEnd([]() {
    publishOtaStatus("success");
    Serial.println(F("\nOTA update complete"));
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    const unsigned int percent = total ? (progress * 100U) / total : 0U;
    Serial.printf("OTA progress: %u%%\r", percent);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    otaEnabled = false;
    otaInProgress = false;
    wind.enable();
    report.enable();

    switch (error) {
      case OTA_AUTH_ERROR:
        publishOtaStatus("authentication error");
        break;
      case OTA_BEGIN_ERROR:
        publishOtaStatus("begin error");
        break;
      case OTA_CONNECT_ERROR:
        publishOtaStatus("connection error");
        break;
      case OTA_RECEIVE_ERROR:
        publishOtaStatus("receive error");
        break;
      case OTA_END_ERROR:
        publishOtaStatus("end error");
        break;
      default:
        publishOtaStatus("unknown error");
        break;
    }

    Serial.printf("\nOTA error: %u\n", error);
  });

  ArduinoOTA.begin();
}