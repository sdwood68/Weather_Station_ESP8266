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

#define FIRMWARE_VERSION "0.7.3"
#define HARDWARE_MODEL "ESP8266 Weather Station"
#define OTA_COMPILE_BUDGET_MS 120000UL
#define OTA_WINDOW_MS 480000UL  // 2x report period + 2x compile budget
#define PORTAL_RESET_MS 900000UL
#define MQTT_RESOLVE_RETRY_MS 30000UL
#define MQTT_RERESOLVE_MS 30000UL

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
#include <ArduinoHA.h>
#include "wind_calculations.h"
#include "pressure_calculations.h"
#include "temperature_calculations.h"
#include "rain_calculations.h"
#include "connectivity_reliability.h"
#include "unit_system.h"

#define LED_PIN       14    // GPIO14
#define RAIN_PIN      12    // GPIO12
#define WIND_PIN      13    // GPIO13 
#define DIR_PIN       0     // A0

/************************************************/
/*                  PROTOTYPES                  */
/************************************************/
void wind_task();
void report_task();
void rain_task();
void setup_ota();
void onOtaButton(HAButton *);
void onStationElevationCommand(HANumeric, HANumber*);
void onRainTipSizeCommand(HANumeric, HANumber*);
void onUnitSystemCommand(int8_t, HASelect*);
void onMqttConnected();
void onMqttDisconnected();
void onMqttMessage(const char* topic, const uint8_t* payload, uint16_t length);
void openOtaWindow();
void processPendingOtaRequest();
void expireOtaWindow();
bool resolveMqttBroker();
bool startMqtt();
const char* wifiSleepModeName();
void IRAM_ATTR anemometer_isr();
void IRAM_ATTR rain_gauge_isr();
void resetRainHistory();
float get_wind_dir();

/************************************************/
/*   Task Scheduler Related Stuff               */
/************************************************/
#define WIND_SAMPLE_PERIOD_MS 1000UL
#define RAIN_SAMPLE_PERIOD_MS 60000UL
#define RAIN_HISTORY_MINUTES 1440U
#define RAIN_DEBOUNCE_US 20000UL
#define DEFAULT_RAIN_TIP_MICROMETERS 254L
#define WIND_OBSERVATION_PERIOD_MS 120000UL
#define WIND_GUST_PERIOD_MS 3000UL
#define REPORT_PERIOD WIND_OBSERVATION_PERIOD_MS
#define PRESSURE_HISTORY_SAMPLES (43200000UL / REPORT_PERIOD)
#define PRESSURE_TENDENCY_SAMPLES (10800000UL / REPORT_PERIOD)
#define UNCONFIGURED_ELEVATION_METERS -10000L
#define RECORDS (WIND_OBSERVATION_PERIOD_MS / WIND_SAMPLE_PERIOD_MS)
#define GUST_RECORDS (WIND_GUST_PERIOD_MS / WIND_SAMPLE_PERIOD_MS)
#define ANEMOMETER_MPH_PER_HZ 1.492f
#define CALM_THRESHOLD_MPH 2.3f
#define WIND_DIRECTION_OFFSET_DEGREES 0.0f
Scheduler ts;
Task wind (WIND_SAMPLE_PERIOD_MS, TASK_FOREVER, &wind_task, &ts, true);
Task report (REPORT_PERIOD, TASK_FOREVER, &report_task, &ts, true);
Task rain (RAIN_SAMPLE_PERIOD_MS, TASK_FOREVER, &rain_task, &ts, true);

/************************************************/
/* ArduinoHA Stuff                              */
/************************************************/
#define HOSTNAME_PREFIX "weather-"
#define PORT 1883
#define TARGET_HOSTNAME "WoodHA"
WiFiClient client;
HADevice device;
static const uint8_t HA_ENTITY_CAPACITY = 40;
HAMqtt mqtt(client, device, HA_ENTITY_CAPACITY);
IPAddress server_ip;
uint16_t port_number;

HASensorNumber temperature("temperature", HASensorNumber::PrecisionP1);
HASensorNumber humidity("humidity", HASensorNumber::PrecisionP1);
HASensorNumber dewPoint("dew_point", HASensorNumber::PrecisionP1);
HASensorNumber heatIndex("heat_index", HASensorNumber::PrecisionP1);
HASensorNumber airPressure("air_pressure", HASensorNumber::PrecisionP3);
HASensorNumber altimeterSetting("altimeter_setting", HASensorNumber::PrecisionP2);
HASensorNumber seaLevelPressure("sea_level_pressure", HASensorNumber::PrecisionP2);
HASensorNumber pressureChange3h("pressure_change_3h", HASensorNumber::PrecisionP1);
HASensor pressureTrend("pressure_trend");
HANumber stationElevationControl("station_elevation", HANumber::PrecisionP0);
HANumber rainTipSizeControl("rain_tip_size", HANumber::PrecisionP4);
HASelect unitSystemControl("unit_system");
HASensorNumber windSpeed("wind_speed", HASensorNumber::PrecisionP2);
HASensorNumber windGust("wind_gust", HASensorNumber::PrecisionP2);
HASensorNumber windDirection("wind_direction", HASensorNumber::PrecisionP1);
HASensorNumber windPulseCount("wind_pulse_count", HASensorNumber::PrecisionP0);
HASensorNumber windSampleCount("wind_sample_count", HASensorNumber::PrecisionP0);
HASensorNumber windGustPulseCount("wind_gust_pulse_count", HASensorNumber::PrecisionP0);
HASensorNumber rainOneMinute("rain_1m", HASensorNumber::PrecisionP2);
HASensorNumber rainOneHour("rain_1h", HASensorNumber::PrecisionP2);
HASensorNumber rainThreeHour("rain_3h", HASensorNumber::PrecisionP2);
HASensorNumber rainSixHour("rain_6h", HASensorNumber::PrecisionP2);
HASensorNumber rainTwentyFourHour("rain_24h", HASensorNumber::PrecisionP2);
HASensorNumber rainSessionTotal("rain_session_total", HASensorNumber::PrecisionP2);
HASensorNumber rainTipCountSensor("rain_tip_count_1m", HASensorNumber::PrecisionP0);
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
unsigned long mqttConnectStartedAt = 0;
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
long stationElevationMeters = 0;
long pressureOffsetPa = 0;
long rainTipMicrometers = DEFAULT_RAIN_TIP_MICROMETERS;
UnitSystem unitSystem = UnitSystem::USA;
bool unitSystemRestartPending = false;
unsigned long unitSystemRestartAt = 0;
FSInfo fs_info;



void onStationElevationCommand(HANumeric number, HANumber* sender) {
  if (!number.isSet()) {
    Serial.println(F("Rejected empty station-elevation command"));
    return;
  }
  const float requestedEntry = number.toFloat();
  const float minimumEntry = usesFeet(unitSystem) ? -1640.0f : -500.0f;
  const float maximumEntry = usesFeet(unitSystem) ? 29528.0f : 9000.0f;
  if (!isfinite(requestedEntry) ||
      requestedEntry < minimumEntry || requestedEntry > maximumEntry) {
    Serial.printf("Rejected station elevation outside %0.0f..%0.0f %s: %0.1f\n",
                  minimumEntry, maximumEntry,
                  usesFeet(unitSystem) ? "ft" : "m", requestedEntry);
    return;
  }
  const int32_t requestedMeters = static_cast<int32_t>(
      roundf(elevationEntryToMeters(requestedEntry, unitSystem)));
  File elevationFile = LittleFS.open("/weather_station_elevation_m", "w");
  if (!elevationFile) {
    Serial.println(F("Unable to persist station elevation"));
    return;
  }
  const String elevationValue(requestedMeters);
  const size_t written = elevationFile.print(elevationValue);
  elevationFile.close();
  if (written != elevationValue.length()) {
    Serial.println(F("Incomplete station-elevation write; command rejected"));
    return;
  }
  stationElevationMeters = requestedMeters;
  sender->setState(reportedElevation(stationElevationMeters, unitSystem), true);
  Serial.printf("Station elevation updated: %ld m (%0.1f %s)\n",
                static_cast<long>(stationElevationMeters),
                reportedElevation(stationElevationMeters, unitSystem),
                usesFeet(unitSystem) ? "ft" : "m");
}

void onRainTipSizeCommand(HANumeric number, HANumber* sender) {
  if (!number.isSet()) {
    Serial.println(F("Rejected empty rain-tip-size command"));
    return;
  }
  const float requestedEntry = number.toFloat();
  const float minimumEntry = usesInches(unitSystem) ? 0.001f : 0.010f;
  const float maximumEntry = usesInches(unitSystem) ? 0.400f : 10.000f;
  if (!isfinite(requestedEntry) ||
      requestedEntry < minimumEntry || requestedEntry > maximumEntry) {
    Serial.printf("Rejected rain tip size outside %0.3f..%0.3f %s: %0.3f\n",
                  minimumEntry, maximumEntry,
                  usesInches(unitSystem) ? "in/tip" : "mm/tip",
                  requestedEntry);
    return;
  }
  const long requestedMicrometers = static_cast<long>(
      roundf(rainTipEntryToMicrometers(requestedEntry, unitSystem)));
  if (requestedMicrometers < 10L || requestedMicrometers > 10000L) {
    Serial.println(F("Rejected rain tip size after canonical conversion"));
    return;
  }
  File tipFile = LittleFS.open("/weather_rain_tip_um", "w");
  if (!tipFile) {
    Serial.println(F("Unable to persist rain tip size"));
    return;
  }
  const String storedValue(requestedMicrometers);
  const size_t written = tipFile.print(storedValue);
  tipFile.close();
  if (written != storedValue.length()) {
    Serial.println(F("Incomplete rain-tip-size write; command rejected"));
    return;
  }
  rainTipMicrometers = requestedMicrometers;
  resetRainHistory();
  const float reportedTipSize =
      rainTipMicrometersToEntry(rainTipMicrometers, unitSystem);
  sender->setState(reportedTipSize, true);
  Serial.printf("Rain tip size updated: %0.4f %s; history reset\n",
                reportedTipSize,
                usesInches(unitSystem) ? "in/tip" : "mm/tip");
}

void onUnitSystemCommand(int8_t index, HASelect* sender) {
  if (!unitSystemIsValid(index)) {
    Serial.printf("Rejected unknown unit-system index: %d\n", index);
    return;
  }
  File unitFile = LittleFS.open("/weather_unit_system", "w");
  if (!unitFile) {
    Serial.println(F("Unable to persist unit system"));
    return;
  }
  const size_t written = unitFile.print(static_cast<int>(index));
  unitFile.close();
  if (written != 1U) {
    Serial.println(F("Incomplete unit-system write; command rejected"));
    return;
  }
  unitSystem = static_cast<UnitSystem>(index);
  sender->setState(index);
  Serial.println(F("Unit system updated; restarting to refresh Home Assistant metadata"));
  unitSystemRestartPending = true;
  unitSystemRestartAt = millis() + 1500UL;
}

bool resolveMqttBroker() {
  mqttBrokerAddress = IPAddress();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("MQTT resolution deferred; Wi-Fi is not connected"));
    return false;
  }

  if (mqttBrokerAddress.fromString(mqttBrokerHost)) {
    if (mqttBrokerAddress != IPAddress()) {
      Serial.printf("MQTT broker configured as IP: %s\n",
                    mqttBrokerAddress.toString().c_str());
      return true;
    }
    Serial.println(F("Rejected configured MQTT address 0.0.0.0"));
    return false;
  }

  Serial.printf("Resolving configured MQTT host by DNS: %s\n",
                mqttBrokerHost.c_str());
  if (WiFi.hostByName(mqttBrokerHost.c_str(), mqttBrokerAddress, 5000) == 1 &&
      mqttBrokerAddress != IPAddress()) {
    Serial.printf("MQTT broker resolved by DNS: %s -> %s\n",
                  mqttBrokerHost.c_str(),
                  mqttBrokerAddress.toString().c_str());
    return true;
  }

  mqttBrokerAddress = IPAddress();
  Serial.println(F("Configured-host DNS failed; querying Home Assistant mDNS service"));
  const int serviceCount = MDNS.queryService("home-assistant", "tcp");
  for (int i = 0; i < serviceCount; ++i) {
    const IPAddress candidate = MDNS.IP(i);
    if (candidate != IPAddress()) {
      mqttBrokerAddress = candidate;
      Serial.printf("Home Assistant found by mDNS: %s -> %s\n",
                    MDNS.hostname(i).c_str(),
                    mqttBrokerAddress.toString().c_str());
      return true;
    }
  }

  if (serviceCount > 0) {
    Serial.println(F("mDNS returned only invalid 0.0.0.0 broker addresses"));
  } else {
    Serial.println(F("No Home Assistant MQTT service found by mDNS"));
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
  mqttConnectStartedAt = millis();
  if (!mqtt.begin(
          mqttBrokerAddress,
          PORT,
          mqttBrokerUser.c_str(),
          mqttBrokerPass.c_str())) {
    mqttResolveRetryAt = millis() + MQTT_RESOLVE_RETRY_MS;
    Serial.println(F("MQTT client initialization failed; retrying in 30 seconds"));
    return false;
  }
  mqttStarted = true;
  mqttDisconnectedAt = millis();
  return true;
}


const char* wifiSleepModeName() {
  switch (WiFi.getSleepMode()) {
    case WIFI_NONE_SLEEP: return "none";
    case WIFI_LIGHT_SLEEP: return "light";
    case WIFI_MODEM_SLEEP: return "modem";
    default: return "unknown";
  }
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
  pinMode(RAIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIND_PIN), anemometer_isr, FALLING);
  attachInterrupt(digitalPinToInterrupt(RAIN_PIN), rain_gauge_isr, FALLING);
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
    if (portalTimeoutExpired(millis(), portalStartedAt, PORTAL_RESET_MS)) {
      Serial.println(F("Configuration portal timed out; restarting"));
      Serial.flush();
      ESP.restart();
    }
  };

  friendlyName = WiFiSettings.string("weather_friendly_name", 1, 64, deviceHostname.c_str(), "Friendly name");
  httpPort = WiFiSettings.integer("weather_http_port", 8080, "HTTP port");
  stationElevationMeters = WiFiSettings.integer(
      "weather_station_elevation_m", -500, 9000, UNCONFIGURED_ELEVATION_METERS,
      "Station elevation above mean sea level (m)");
  pressureOffsetPa = WiFiSettings.integer(
      "weather_pressure_offset_pa", -5000, 5000, 0,
      "BMP280 calibration offset (Pa)");
  rainTipMicrometers = WiFiSettings.integer(
      "weather_rain_tip_um", 10, 10000, DEFAULT_RAIN_TIP_MICROMETERS,
      "Rain gauge bucket size (micrometers per tip)");
  unitSystem = static_cast<UnitSystem>(WiFiSettings.integer(
      "weather_unit_system", 0, 2, 0,
      "Unit system (0=USA, 1=European Union, 2=United Kingdom)"));
  const String manualWifiSsid = WiFiSettings.string(
      "weather_hidden_ssid", 0, 32, "",
      "Hidden WiFi SSID (optional; clear to use scanned selection)");
  otaPassword = WiFiSettings.string("weather_ota_password", 8, 64, WIFI_SETTINGS_PASSWORD, "OTA password");
  mqttBrokerHost = WiFiSettings.string("weather_mqtt_broker", 1, 64, "", "MQTT broker address");
  mqttBrokerUser = WiFiSettings.string("weather_mqtt_user", 1, 64, MQTT_BROKER_USER, "MQTT username");
  mqttBrokerPass = WiFiSettings.string("weather_mqtt_password", 1, 64, MQTT_BROKER_PASS, "MQTT password");

  Serial.printf("Configuration portal SSID: %s\n", WiFiSettings.hostname.c_str());

  if (manualWifiSsid.length() > 0) {
    File ssidFile = LittleFS.open("/wifi-ssid", "w");
    if (!ssidFile || ssidFile.print(manualWifiSsid) != manualWifiSsid.length()) {
      Serial.println(F("Unable to persist manually entered hidden Wi-Fi SSID"));
    } else {
      Serial.println(F("Using manually entered hidden Wi-Fi SSID"));
    }
    ssidFile.close();
  }

  String configuredWifiSsid;
  File ssidFile = LittleFS.open("/wifi-ssid", "r");
  if (ssidFile) {
    configuredWifiSsid = ssidFile.readString();
    ssidFile.close();
  }

  const bool configurationMissing = requiredConfigurationMissing(
      configuredWifiSsid.length() > 0,
      otaPassword.length() > 0,
      mqttBrokerHost.length() > 0,
      mqttBrokerUser.length() > 0,
      mqttBrokerPass.length() > 0);

  if (configurationMissing) {
    Serial.println(F("Required configuration is missing; starting portal"));
    if (configuredWifiSsid.length() == 0) {
      Serial.println(F("Missing configuration: Wi-Fi SSID"));
    }
    if (otaPassword.length() == 0) {
      Serial.println(F("Missing configuration: OTA password"));
    }
    if (mqttBrokerHost.length() == 0) {
      Serial.println(F("Missing configuration: MQTT broker"));
    }
    if (mqttBrokerUser.length() == 0) {
      Serial.println(F("Missing configuration: MQTT username"));
    }
    if (mqttBrokerPass.length() == 0) {
      Serial.println(F("Missing configuration: MQTT password"));
    }
    WiFiSettings.portal();
  }

  const unsigned long wifiConnectStartedAt = millis();
  WiFiSettings.connect(true, 60);
  Serial.printf("POWER_BASELINE Wi-Fi connect: %lu ms; sleep mode: %s\n",
                millis() - wifiConnectStartedAt,
                wifiSleepModeName());

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

  const char* temperatureUnit = usesFahrenheit(unitSystem) ? "°F" : "°C";
  const char* pressureUnit = usesInchesHg(unitSystem) ? "inHg" : "hPa";
  const char* elevationUnit = usesFeet(unitSystem) ? "ft" : "m";
  const char* rainUnit = usesInches(unitSystem) ? "in" : "mm";
  const char* rainTipUnit = usesInches(unitSystem) ? "in/tip" : "mm/tip";
  const char* windUnit = usesKilometersPerHour(unitSystem) ? "km/h" : "mph";

  temperature.setIcon("mdi:sun-thermometer");
  temperature.setName("Outside temp");
  temperature.setUnitOfMeasurement(temperatureUnit);
  humidity.setIcon("mdi:water-percent");
  humidity.setName("Outside Humidity");
  humidity.setUnitOfMeasurement("%");
  dewPoint.setIcon("mdi:water-thermometer");
  dewPoint.setName("Dew Point");
  dewPoint.setUnitOfMeasurement(temperatureUnit);
  heatIndex.setIcon("mdi:sun-thermometer");
  heatIndex.setName("Heat Index");
  heatIndex.setUnitOfMeasurement(temperatureUnit);
  airPressure.setIcon("mdi:gauge");
  airPressure.setName("Station Pressure");
  airPressure.setUnitOfMeasurement(pressureUnit);
  altimeterSetting.setIcon("mdi:gauge");
  altimeterSetting.setName("Altimeter Setting");
  altimeterSetting.setUnitOfMeasurement(pressureUnit);
  seaLevelPressure.setIcon("mdi:gauge");
  seaLevelPressure.setName("Sea-Level Pressure");
  seaLevelPressure.setUnitOfMeasurement(pressureUnit);
  pressureChange3h.setIcon("mdi:chart-timeline-variant");
  pressureChange3h.setName("3-Hour Pressure Change");
  pressureChange3h.setUnitOfMeasurement(pressureUnit);
  pressureTrend.setIcon("mdi:trending-up");
  pressureTrend.setName("3-Hour Pressure Trend");

  stationElevationControl.setIcon("mdi:elevation-rise");
  stationElevationControl.setName("Station Elevation");
  stationElevationControl.setUnitOfMeasurement(elevationUnit);
  stationElevationControl.setMode(HANumber::ModeBox);
  stationElevationControl.setMin(usesFeet(unitSystem) ? -1640.0f : -500.0f);
  stationElevationControl.setMax(usesFeet(unitSystem) ? 29528.0f : 9000.0f);
  stationElevationControl.setStep(1.0f);
  stationElevationControl.setRetain(false);
  stationElevationControl.setOptimistic(false);
  stationElevationControl.onCommand(onStationElevationCommand);
  if (stationElevationMeters != UNCONFIGURED_ELEVATION_METERS) {
    stationElevationControl.setCurrentState(
        reportedElevation(stationElevationMeters, unitSystem));
  }

  rainTipSizeControl.setIcon("mdi:cup-water");
  rainTipSizeControl.setName("Rain Gauge Tip Size");
  rainTipSizeControl.setUnitOfMeasurement(rainTipUnit);
  rainTipSizeControl.setMode(HANumber::ModeBox);
  rainTipSizeControl.setMin(usesInches(unitSystem) ? 0.001f : 0.010f);
  rainTipSizeControl.setMax(usesInches(unitSystem) ? 0.400f : 10.000f);
  rainTipSizeControl.setStep(0.0001f);
  rainTipSizeControl.setRetain(false);
  rainTipSizeControl.setOptimistic(false);
  rainTipSizeControl.onCommand(onRainTipSizeCommand);
  rainTipSizeControl.setCurrentState(
      rainTipMicrometersToEntry(rainTipMicrometers, unitSystem));

  unitSystemControl.setIcon("mdi:tune-variant");
  unitSystemControl.setName("Measurement Unit System");
  unitSystemControl.setOptions("USA;European Union;United Kingdom");
  unitSystemControl.setRetain(false);
  unitSystemControl.setOptimistic(false);
  unitSystemControl.onCommand(onUnitSystemCommand);
  unitSystemControl.setCurrentState(static_cast<uint8_t>(unitSystem));

  rainOneMinute.setIcon("mdi:weather-rainy");
  rainOneMinute.setName("1-Minute Rain");
  rainOneMinute.setUnitOfMeasurement(rainUnit);
  rainOneMinute.setDeviceClass("precipitation");
  rainOneMinute.setStateClass("measurement");
  rainOneHour.setIcon("mdi:weather-rainy");
  rainOneHour.setName("Latest 60-Minute Rain");
  rainOneHour.setUnitOfMeasurement(rainUnit);
  rainOneHour.setDeviceClass("precipitation");
  rainOneHour.setStateClass("measurement");
  rainThreeHour.setIcon("mdi:weather-rainy");
  rainThreeHour.setName("Latest 3-Hour Rain");
  rainThreeHour.setUnitOfMeasurement(rainUnit);
  rainThreeHour.setDeviceClass("precipitation");
  rainThreeHour.setStateClass("measurement");
  rainSixHour.setIcon("mdi:weather-rainy");
  rainSixHour.setName("Latest 6-Hour Rain");
  rainSixHour.setUnitOfMeasurement(rainUnit);
  rainSixHour.setDeviceClass("precipitation");
  rainSixHour.setStateClass("measurement");
  rainTwentyFourHour.setIcon("mdi:weather-rainy");
  rainTwentyFourHour.setName("Latest 24-Hour Rain");
  rainTwentyFourHour.setUnitOfMeasurement(rainUnit);
  rainTwentyFourHour.setDeviceClass("precipitation");
  rainTwentyFourHour.setStateClass("measurement");
  rainSessionTotal.setIcon("mdi:weather-pouring");
  rainSessionTotal.setName("Rain Since Boot or Calibration Change");
  rainSessionTotal.setUnitOfMeasurement(rainUnit);
  rainSessionTotal.setDeviceClass("precipitation");
  rainSessionTotal.setStateClass("total_increasing");
  rainTipCountSensor.setIcon("mdi:counter");
  rainTipCountSensor.setName("1-Minute Rain Tip Count");
  rainTipCountSensor.setUnitOfMeasurement("tips");
  rainTipCountSensor.setEntityCategory("diagnostic");

  windSpeed.setIcon("mdi:weather-windy");
  windSpeed.setName("2-Minute Sustained Wind Speed");
  windSpeed.setUnitOfMeasurement(windUnit);
  windGust.setIcon("mdi:weather-windy");
  windGust.setName("3-Second Wind Gust");
  windGust.setUnitOfMeasurement(windUnit);
  windDirection.setIcon("mdi:sun-compass");
  windDirection.setName("2-Minute True Wind Direction");
  windDirection.setUnitOfMeasurement("°");
  windPulseCount.setIcon("mdi:counter");
  windPulseCount.setName("2-Minute Wind Pulse Count");
  windPulseCount.setUnitOfMeasurement("pulses");
  windPulseCount.setEntityCategory("diagnostic");
  windSampleCount.setIcon("mdi:counter");
  windSampleCount.setName("Wind Sample Count");
  windSampleCount.setUnitOfMeasurement("samples");
  windSampleCount.setEntityCategory("diagnostic");
  windGustPulseCount.setIcon("mdi:counter");
  windGustPulseCount.setName("3-Second Gust Pulse Count");
  windGustPulseCount.setUnitOfMeasurement("pulses");
  windGustPulseCount.setEntityCategory("diagnostic");
  boxTemperature.setIcon("mdi:thermometer");
  boxTemperature.setName("Internal Temp");
  boxTemperature.setUnitOfMeasurement(temperatureUnit);
  otaButton.setName("Enable OTA");
  otaButton.setIcon("mdi:update");
  otaButton.setRetain(false);
  otaButton.onCommand(onOtaButton);
  otaStatus.setName("OTA Status");
  otaStatus.setIcon("mdi:update");
  otaStatus.setEntityCategory("diagnostic");
  firmwareVersion.setName("Firmware Version");
  firmwareVersion.setIcon("mdi:information-outline");
  firmwareVersion.setEntityCategory("diagnostic");
  chipIdSensor.setName("Chip ID");
  chipIdSensor.setIcon("mdi:identifier");
  chipIdSensor.setEntityCategory("diagnostic");
  macAddressSensor.setName("Wi-Fi MAC Address");
  macAddressSensor.setIcon("mdi:network-outline");
  macAddressSensor.setEntityCategory("diagnostic");
  ipAddressSensor.setName("IP Address");
  ipAddressSensor.setIcon("mdi:ip-network-outline");
  hostnameSensor.setName("Hostname");
  hostnameSensor.setIcon("mdi:web");
  resetReasonSensor.setName("Reset Reason");
  resetReasonSensor.setIcon("mdi:restart-alert");
  resetReasonSensor.setEntityCategory("diagnostic");
  wifiRssiSensor.setName("Wi-Fi RSSI");
  wifiRssiSensor.setIcon("mdi:wifi");
  wifiRssiSensor.setUnitOfMeasurement("dBm");
  wifiRssiSensor.setEntityCategory("diagnostic");

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
    if (!mqtt.isConnected() && mqttDisconnectedAt != 0 &&
        portalTimeoutExpired(millis(), mqttDisconnectedAt, MQTT_RERESOLVE_MS)) {
      Serial.println(F("MQTT unavailable for 30 seconds; re-resolving broker"));
      mqtt.disconnect();
      mqttStarted = false;
      mqttDisconnectedAt = 0;
      mqttResolveRetryAt = millis();
    }
  } else if (static_cast<int32_t>(millis() - mqttResolveRetryAt) >= 0) {
    startMqtt();
  }
  ts.execute();

  if (otaEnabled) {
    ArduinoOTA.handle();
    expireOtaWindow();
  }

  if (unitSystemRestartPending &&
      static_cast<int32_t>(millis() - unitSystemRestartAt) >= 0) {
    Serial.flush();
    ESP.restart();
  }

}