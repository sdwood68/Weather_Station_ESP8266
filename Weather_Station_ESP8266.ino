/*****************************************************************************/
/*  Arduino Configuration Info                                               */
/*  Board Support Package:                                                   */
/*    http://arduino.esp8266.com/stable/package_esp8266com_index.json        */
/*  Library Manager:                                                         */
/*    LittleFS                                                               */
/*    FS                                                                     */
/*    ArduinoOTA                                                             */
/*    WiFiSettings                                                           */
/*    ESP8266Wifi                                                            */
/*    ArduinoHA                                                              */
/*  Tools/Board: LOLIN(WEMOS) D1 R2 & mini                                   */
/*****************************************************************************/

#define WIFI_SETTINGS_PASSWORD ""
#define MQTT_BROKER_USER ""
#define MQTT_BROKER_PASS ""

#define FIRMWARE_VERSION "0.6.0"
#define HARDWARE_MODEL "ESP8266 Weather Station"
#define OTA_COMPILE_BUDGET_MS 120000UL
#define OTA_WINDOW_MS 480000UL  // 2x report period + 2x compile budget
#define PORTAL_RESET_MS 900000UL
#define MQTT_RESOLVE_RETRY_MS 30000UL
#define MQTT_RESTART_MS 300000UL

#include <LittleFS.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFiSettings.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "BMP280_SDW.h"
#include "AHT20_SDW.h"
#include <Adafruit_AM2315.h>
#include <TaskScheduler.h>
#include <Coordinates.h>
#include <ArduinoHA.h>

#define LED_PIN       14    // GPIO14
#define RAIN_PIN      12    // GPIO12
#define WIND_PIN      13    // GPIO13 
#define DIR_PIN       0     // A0

/************************************************/
/*                  PROTOTYPES                  */
/************************************************/
void wind_task();
void report_task();
void setup_ota();
void onOtaButton(HAButton *);
void onMqttConnected();
void onMqttDisconnected();
void onMqttMessage(const char* topic, const uint8_t* payload, uint16_t length);
void openOtaWindow();
void processPendingOtaRequest();
void expireOtaWindow();
bool resolveMqttBroker();
bool startMqtt();
void IRAM_ATTR anemometer_isr();
float get_wind_dir();

/************************************************/
/*   Task Scheduler Related Stuff               */
/************************************************/
#define WIND_PERIOD 5000  // Wind calaculation every 5 seconds
#define REPORT_PERIOD 120000 // Report weather conditions every 2 minutes
#define RECORDS REPORT_PERIOD/WIND_PERIOD
Scheduler ts;
Task wind (WIND_PERIOD, TASK_FOREVER, &wind_task, &ts, true);
Task report (REPORT_PERIOD, TASK_FOREVER, &report_task, &ts, true);

/************************************************/
/* ArduinoHA Stuff                              */
/************************************************/
#define HOSTNAME_PREFIX "weather-"
#define PORT 1883
#define TARGET_HOSTNAME "WoodHA"
WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
IPAddress server_ip;
uint16_t port_number;

HASensorNumber temperature("temperature", HASensorNumber::PrecisionP1);
HASensorNumber humidity("humidity", HASensorNumber::PrecisionP1);
HASensorNumber airPressure("air_pressure", HASensorNumber::PrecisionP3);
HASensorNumber windSpeed("wind_speed", HASensorNumber::PrecisionP2);
HASensorNumber windGust("wind_gust", HASensorNumber::PrecisionP2);
HASensorNumber windMagnitude("wind_magnitude", HASensorNumber::PrecisionP2);
HASensorNumber windDirection("wind_direction", HASensorNumber::PrecisionP1);
HASensorNumber boxTemperature("box_temperature", HASensorNumber::PrecisionP1);
HAButton otaButton("enable_ota");
HASensor otaStatus("ota_status");
HASensor firmwareVersion("firmware_version");
HASensor chipIdSensor("chip_id");
HASensor macAddressSensor("mac_address");
HASensor ipAddressSensor("ip_address");
HASensor hostnameSensor("hostname");
HASensor resetReasonSensor("reset_reason");
HASensorNumber wifiRssiSensor("wifi_rssi", HASensorNumber::PrecisionP0);

bool otaEnabled = false;
bool otaInProgress = false;
unsigned long otaDeadline = 0;
unsigned long portalStartedAt = 0;
unsigned long mqttDisconnectedAt = 0;
unsigned long mqttResolveRetryAt = 0;
bool mqttStarted = false;
String otaState = "standby";
bool sensorTasksPaused = false;
String deviceHostname;
String deviceChipId;
String friendlyName;
String otaRequestTopic;
String otaRequestStatusTopic;
String pendingOtaRequest;
String otaPassword;
String mqttBrokerHost;
String mqttBrokerUser;
String mqttBrokerPass;
IPAddress mqttBrokerAddress;
byte deviceUniqueId[3];

BMP280_SDW bmp280;    // I2C Address 0x77
AHT20 aht20;          // I2C Address 0x38
Adafruit_AM2315 am2315;

bool bAm2315 = false;
bool bBmp280 = false;
bool bAht20 = false;

int httpPort = 0;
FSInfo fs_info;


bool resolveMqttBroker() {
  mqttBrokerAddress = IPAddress();

  if (mqttBrokerAddress.fromString(mqttBrokerHost)) {
    Serial.printf("MQTT broker configured as IP: %s\n",
                  mqttBrokerAddress.toString().c_str());
    return true;
  }

  if (WiFi.hostByName(mqttBrokerHost.c_str(), mqttBrokerAddress, 5000) == 1) {
    Serial.printf("MQTT broker resolved by DNS: %s -> %s\n",
                  mqttBrokerHost.c_str(),
                  mqttBrokerAddress.toString().c_str());
    return true;
  }

  const int serviceCount = MDNS.queryService("home-assistant", "tcp");
  if (serviceCount > 0) {
    mqttBrokerAddress = MDNS.IP(0);
    if (mqttBrokerAddress != IPAddress()) {
      Serial.printf("Home Assistant found by mDNS: %s -> %s\n",
                    MDNS.hostname(0).c_str(),
                    mqttBrokerAddress.toString().c_str());
      return true;
    }

    Serial.println(F("mDNS returned an invalid 0.0.0.0 broker address"));
  }

  Serial.printf("Unable to resolve MQTT broker: %s\n",
                mqttBrokerHost.c_str());
  return false;
}

bool startMqtt() {
  if (!resolveMqttBroker()) {
    mqttResolveRetryAt = millis() + MQTT_RESOLVE_RETRY_MS;
    Serial.println(F("MQTT start deferred; broker resolution will retry in 30 seconds"));
    return false;
  }

  Serial.printf("Starting MQTT with broker %s:%u\n",
                mqttBrokerAddress.toString().c_str(), PORT);
  mqtt.begin(mqttBrokerAddress, PORT, mqttBrokerUser.c_str(), mqttBrokerPass.c_str());
  mqttStarted = true;
  mqttDisconnectedAt = millis();
  return true;
}

void setup() {
  Serial.begin(115200); 
  Serial.println();
  Serial.println(F("-----------------------------"));
  Serial.println(F("---  ESP Weather Station  ---"));
  Serial.println(F("-----------------------------"));

  Wire.begin();

  /**********************************************/
  /*  Configure GPIO & Pin Interrupt            */
  /**********************************************/
  pinMode(LED_PIN, OUTPUT);
  pinMode(DIR_PIN, INPUT);
  pinMode(WIND_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIND_PIN), anemometer_isr, FALLING);
  // attachInterrupt(digitalPinToInterrupt(WIND_PIN), anemometer_isr, FALLING);

  /************************************************
    Initialize SPI Flash File System (LittleFS)
    snd printout some information
  ************************************************/
  Serial.print(F("Mounting the SPI Flash File System... "));
  if (!LittleFS.begin()) {
    Serial.println(F("Failed!"));
  } else {
    Serial.println(F("Success!"));
    LittleFS.info(fs_info);
    Serial.printf("File System Size: %4.1f kBytes\n", (float(fs_info.totalBytes) / 1024));
    Serial.printf("File System Used: %4.1f kBytes\n", (float(fs_info.usedBytes) / 1024));
  }
  Serial.printf("ESP Flash Chip Size: %4.1f kBytes\n", (float(ESP.getFlashChipSize()) / 1024));

  /**********************************************************
    Start WiFiSettings & Arduino OTA
  **********************************************************/
  char chipId[7];
  snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
  deviceChipId = chipId;
  deviceHostname = String(HOSTNAME_PREFIX) + deviceChipId;
  otaRequestTopic = String("weather_station/") + deviceChipId + "/ota/request";
  otaRequestStatusTopic = String("weather_station/") + deviceChipId + "/ota/status";

  WiFiSettings.hostname = deviceHostname;
  WiFiSettings.secure = true;
  WiFiSettings.onPortal = []() {
    portalStartedAt = millis();
    Serial.println(F("Configuration portal will restart in 15 minutes"));
  };
  WiFiSettings.onPortalWaitLoop = []() {
    if (millis() - portalStartedAt >= PORTAL_RESET_MS) {
      Serial.println(F("Configuration portal timed out; restarting"));
      Serial.flush();
      ESP.restart();
    }
  };

  friendlyName = WiFiSettings.string("weather_friendly_name", 1, 64, deviceHostname.c_str(), "Friendly name");
  httpPort = WiFiSettings.integer("weather_http_port", 8080, "HTTP port");
  otaPassword = WiFiSettings.string("weather_ota_password", 8, 64, WIFI_SETTINGS_PASSWORD, "OTA password");
  mqttBrokerHost = WiFiSettings.string("weather_mqtt_broker", 1, 64, "", "MQTT broker address");
  mqttBrokerUser = WiFiSettings.string("weather_mqtt_user", 1, 64, MQTT_BROKER_USER, "MQTT username");
  mqttBrokerPass = WiFiSettings.string("weather_mqtt_password", 1, 64, MQTT_BROKER_PASS, "MQTT password");

  Serial.printf("Configuration portal SSID: %s\n", WiFiSettings.hostname.c_str());


  const bool configurationMissing =
      otaPassword.length() == 0 ||
      mqttBrokerHost.length() == 0 ||
      mqttBrokerUser.length() == 0 ||
      mqttBrokerPass.length() == 0;

  if (configurationMissing) {
    Serial.println(F("Required configuration is missing; starting portal"));
    WiFiSettings.portal();
  }

  WiFiSettings.connect(true, 60);

  // ArduinoOTA initializes the ESP8266 mDNS responder used by the fallback.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setup_ota();


  /***************************************************************************/
  /* Sensor Initialization                                                   */
  /***************************************************************************/
  delay(10);
  Serial.print(F("Starting BMP280... "));
  bBmp280 = bmp280.begin(0x77);
  if (!bBmp280) {  // BMP280 temperature and presure sensor @ Addr. 0x77
    Serial.println(F("Failed!"));
  } else {
    Serial.println(F("Success!"));
  }

  Serial.print(F("Starting AHT20... "));
  bAht20 = aht20.begin();
  if (!bAht20) {
    Serial.println(F("Failed!"));
  } else {
    Serial.println(F("Success!"));
  }

  Serial.print(F("Starting AM2315... "));
  bAm2315 = am2315.begin();
  if (!bAm2315) {
    Serial.println(F("Failed!"));
  } else {
    Serial.println(F("Success!"));
  }

  /****************************************************************************/
  /* HOME ASSISTANT MQTT STUFF                                                */
  /****************************************************************************/
  const uint32_t numericChipId = ESP.getChipId();
  deviceUniqueId[0] = (numericChipId >> 16) & 0xFF;
  deviceUniqueId[1] = (numericChipId >> 8) & 0xFF;
  deviceUniqueId[2] = numericChipId & 0xFF;
  device.setUniqueId(deviceUniqueId, sizeof(deviceUniqueId));
  device.enableExtendedUniqueIds();
  device.setName(friendlyName.c_str());
  device.setManufacturer("Woody");
  device.setModel(HARDWARE_MODEL);
  device.setSoftwareVersion(FIRMWARE_VERSION);

  temperature.setIcon("mdi:sun-thermometer");
  temperature.setName("Outside temp");
  temperature.setUnitOfMeasurement("°F");
  humidity.setIcon("mdi:water-percent");
  humidity.setName("Outside Humidity");
  humidity.setUnitOfMeasurement("%");
  airPressure.setIcon("mdi:gauge");
  airPressure.setName("Air Pressure");
  airPressure.setUnitOfMeasurement("kPa");
  windSpeed.setIcon("mdi:weather-windy");
  windSpeed.setName("Wind Speed");
  windSpeed.setUnitOfMeasurement("mph");
  windGust.setIcon("mdi:weather-windy");
  windGust.setName("Wind Gust");
  windGust.setUnitOfMeasurement("mph");
  windMagnitude.setIcon("mdi:weather-windy");
  windMagnitude.setName("Wind Magnitude");
  windMagnitude.setUnitOfMeasurement("mph");
  windDirection.setIcon("mdi:sun-compass");
  windDirection.setName("Wind Direction");
  windDirection.setUnitOfMeasurement("°");
  boxTemperature.setIcon("mdi:thermometer");
  boxTemperature.setName("Internal Temp");
  boxTemperature.setUnitOfMeasurement("°F");

  otaButton.setName("Enable OTA");
  otaButton.setIcon("mdi:update");
  otaButton.setRetain(false);
  otaButton.onCommand(onOtaButton);
  otaStatus.setName("OTA Status");
  otaStatus.setIcon("mdi:update");
  firmwareVersion.setName("Firmware Version");
  firmwareVersion.setIcon("mdi:information-outline");
  chipIdSensor.setName("Chip ID");
  chipIdSensor.setIcon("mdi:identifier");
  macAddressSensor.setName("Wi-Fi MAC Address");
  macAddressSensor.setIcon("mdi:network-outline");
  ipAddressSensor.setName("IP Address");
  ipAddressSensor.setIcon("mdi:ip-network-outline");
  hostnameSensor.setName("Hostname");
  hostnameSensor.setIcon("mdi:web");
  resetReasonSensor.setName("Reset Reason");
  resetReasonSensor.setIcon("mdi:restart-alert");
  wifiRssiSensor.setName("Wi-Fi RSSI");
  wifiRssiSensor.setIcon("mdi:wifi");
  wifiRssiSensor.setUnitOfMeasurement("dBm");

  mqtt.setDataPrefix("weather_station");
  mqtt.setBufferSize(512);
  device.enableSharedAvailability();
  device.enableLastWill();
  mqtt.onConnected(onMqttConnected);
  mqtt.onDisconnected(onMqttDisconnected);
  mqtt.onMessage(onMqttMessage);
  startMqtt();

  /****************************************************************************/
  /*   Start the Scheduler                                                    */
  /****************************************************************************/
  ts.startNow();
}
 
void loop() {
  if (mqttStarted) {
    mqtt.loop();
  } else if (static_cast<int32_t>(millis() - mqttResolveRetryAt) >= 0) {
    startMqtt();
  }
  ts.execute();

  if (otaEnabled) {
    ArduinoOTA.handle();
    expireOtaWindow();
  }

  if (mqttStarted && !mqtt.isConnected() && mqttDisconnectedAt != 0 &&
      static_cast<int32_t>(millis() - mqttDisconnectedAt) >= static_cast<int32_t>(MQTT_RESTART_MS)) {
    Serial.println(F("MQTT unavailable for 5 minutes; restarting to resolve broker again"));
    Serial.flush();
    ESP.restart();
  }
}