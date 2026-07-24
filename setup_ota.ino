/******************************************************************************/
/* MQTT-authorized Arduino OTA window                                          */
/******************************************************************************/

void publishOtaStatus(const char* status) {
  otaState = status;
  otaStatus.setValue(otaState.c_str());
}

void pauseSensorTasks() {
  if (sensorTasksPaused) {
    return;
  }

  wind.disable();
  report.disable();
  sensorTasksPaused = true;
}

void resumeSensorTasks() {
  if (!sensorTasksPaused) {
    return;
  }

  wind.enable();
  report.enable();
  sensorTasksPaused = false;
}

void onMqttConnected() {
  mqttDisconnectedAt = 0;
  device.setAvailability(true);
  firmwareVersion.setValue(FIRMWARE_VERSION);
  chipIdSensor.setValue(deviceChipId.c_str());
  macAddressSensor.setValue(WiFi.macAddress().c_str());
  ipAddressSensor.setValue(WiFi.localIP().toString().c_str());
  hostnameSensor.setValue(deviceHostname.c_str());
  resetReasonSensor.setValue(ESP.getResetReason().c_str());
  wifiRssiSensor.setValue(WiFi.RSSI());
  otaStatus.setValue(otaState.c_str());
}

void onMqttDisconnected() {
  if (mqttDisconnectedAt == 0) {
    mqttDisconnectedAt = millis();
  }
}

void onOtaButton(HAButton* sender) {
  (void)sender;

  if (otaInProgress) {
    Serial.println(F("Ignoring OTA command while an update is in progress"));
    return;
  }

  otaEnabled = true;
  otaDeadline = millis() + OTA_WINDOW_MS;
  publishOtaStatus("ready");
  Serial.println(F("OTA enabled for 120 seconds"));
}

void expireOtaWindow() {
  if (!otaEnabled || otaInProgress ||
      static_cast<int32_t>(millis() - otaDeadline) < 0) {
    return;
  }

  otaEnabled = false;
  publishOtaStatus("timeout");
  Serial.println(F("OTA window expired"));
}

void setup_ota() {
  ArduinoOTA.setHostname(deviceHostname.c_str());

  if (otaPassword.length() > 0) {
    ArduinoOTA.setPassword(otaPassword.c_str());
  } else {
    Serial.println(F("WARNING: OTA password is not configured"));
  }

  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    pauseSensorTasks();
    publishOtaStatus("updating");
    Serial.println(F("OTA update started"));
  });

  ArduinoOTA.onEnd([]() {
    publishOtaStatus("success");
    Serial.println(F("\nOTA update complete; reboot will confirm the running version"));
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    const unsigned int percent = total ? (progress * 100U) / total : 0U;
    Serial.printf("OTA progress: %u%%\r", percent);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    otaEnabled = false;
    otaInProgress = false;
    resumeSensorTasks();
    publishOtaStatus("error");

    const char* detail = "unknown";
    switch (error) {
      case OTA_AUTH_ERROR: detail = "authentication"; break;
      case OTA_BEGIN_ERROR: detail = "begin"; break;
      case OTA_CONNECT_ERROR: detail = "connection"; break;
      case OTA_RECEIVE_ERROR: detail = "receive"; break;
      case OTA_END_ERROR: detail = "end"; break;
      default: break;
    }

    Serial.printf("\nOTA %s error (%u); sensor tasks resumed\n", detail, error);
  });

  ArduinoOTA.begin();
}