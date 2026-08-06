/******************************************************************************/
/* MQTT-authorized Arduino OTA window                                          */
/******************************************************************************/

void publishOtaStatus(const char* status) {
  otaState = status;
  otaStatus.setValue(otaState.c_str());
  mqtt.publish(otaRequestStatusTopic.c_str(), otaState.c_str(), true);
}

void pauseSensorTasks() {
  if (sensorTasksPaused) {
    return;
  }

  wind.disable();
  report.disable();
  rain.disable();
  sensorTasksPaused = true;
}

void resumeSensorTasks() {
  if (!sensorTasksPaused) {
    return;
  }

  wind.enable();
  report.enable();
  rain.enable();
  sensorTasksPaused = false;
}

void onMqttConnected() {
  mqttDisconnectedAt = 0;
  Serial.println(F("MQTT connected; publishing discovery and diagnostics"));
  Serial.printf("POWER_BASELINE MQTT connect: %lu ms\n",
                mqttConnectStartedAt ? millis() - mqttConnectStartedAt : 0UL);
  device.setAvailability(true);
  firmwareVersion.setValue(FIRMWARE_VERSION);
  chipIdSensor.setValue(deviceChipId.c_str());
  macAddressSensor.setValue(WiFi.macAddress().c_str());
  ipAddressSensor.setValue(WiFi.localIP().toString().c_str());
  hostnameSensor.setValue(deviceHostname.c_str());
  resetReasonSensor.setValue(ESP.getResetReason().c_str());
  wifiRssiSensor.setValue(WiFi.RSSI());
  mqtt.subscribe(otaRequestTopic.c_str());
  otaStatus.setValue(otaState.c_str());
  mqtt.publish(otaRequestStatusTopic.c_str(), otaState.c_str(), true);
}

void onMqttDisconnected() {
  if (mqttDisconnectedAt == 0) {
    mqttDisconnectedAt = millis();
  }
}

void openOtaWindow() {
  if (otaInProgress) {
    Serial.println(F("Ignoring OTA request while an update is in progress"));
    return;
  }

  otaEnabled = true;
  otaDeadline = millis() + OTA_WINDOW_MS;
  publishOtaStatus("ready");
  Serial.printf("OTA enabled for %lu seconds\n", OTA_WINDOW_MS / 1000UL);
}

void onOtaButton(HAButton* sender) {
  (void)sender;
  openOtaWindow();
}

void onMqttMessage(const char* topic, const uint8_t* payload, uint16_t length) {
  if (strcmp(topic, otaRequestTopic.c_str()) != 0) {
    return;
  }

  if (length == 0 || length > 384) {
    Serial.println(F("Rejected OTA request with invalid payload length"));
    return;
  }

  JsonDocument request;
  const DeserializationError parseError = deserializeJson(request, payload, length);
  if (parseError) {
    Serial.printf("Rejected malformed OTA request: %s\n", parseError.c_str());
    return;
  }

  const bool requested = request["requested"] | false;
  const time_t expires = request["expires"] | 0;
  const char* targetVersion = request["target_version"] | "";
  const time_t now = time(nullptr);

  if (!requested || targetVersion[0] == '\0') {
    Serial.println(F("Rejected incomplete OTA request"));
    return;
  }

  if (now < 1700000000) {
    pendingOtaRequest = "";
    pendingOtaRequest.reserve(length);
    for (uint16_t i = 0; i < length; ++i) {
      pendingOtaRequest += static_cast<char>(payload[i]);
    }
    Serial.println(F("Deferring OTA request until the clock is synchronized"));
    return;
  }

  if (expires <= now) {
    Serial.println(F("Discarding expired retained OTA request"));
    mqtt.publish(otaRequestTopic.c_str(), "", true);
    return;
  }

  if (strcmp(targetVersion, FIRMWARE_VERSION) == 0) {
    Serial.println(F("Clearing OTA request because target version is already running"));
    mqtt.publish(otaRequestTopic.c_str(), "", true);
    return;
  }

  if (!mqtt.publish(otaRequestTopic.c_str(), "", true)) {
    Serial.println(F("Could not clear retained OTA request; OTA will remain disabled"));
    return;
  }

  Serial.printf("Accepted OTA request for firmware %s\n", targetVersion);
  openOtaWindow();
}
void processPendingOtaRequest() {
  if (pendingOtaRequest.length() == 0 || time(nullptr) < 1700000000) {
    return;
  }

  String request = pendingOtaRequest;
  pendingOtaRequest = "";
  onMqttMessage(
      otaRequestTopic.c_str(),
      reinterpret_cast<const uint8_t*>(request.c_str()),
      request.length());
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