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
/*    MQTT by Joel Gaehwiler                                                 */
/*  Tools/Board: LOLIN(WEMOS) D1 R2 & mini                                   */
/*****************************************************************************/


#include <LittleFS.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <WiFiSettings.h>
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include <Wire.h>
#include "BMP280_SDW.h"
#include "AHT20_SDW.h"
#include <Adafruit_AM2315.h>
#include <TaskScheduler.h>
#include <Coordinates.h>

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
void mqttConnect();
void messageReceived(String &, String &);
void IRAM_ATTR anemometer_isr();
float get_wind_dir();
float sin(int);
float cos(int);

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
/* MQTT Weather_Station Message Topics          */
/************************************************/
const char* topics[] = {
  "/Weather_Station/temp_1",
  "/Weather_Station/temp_2",
  "/Weather_Station/temp_3",
  "/Weather_Station/humid_1",
  "/Weather_Station/humid_2",
  "/Weather_Station/press",
  "/Weather_Station/ave_wind_speed",
  "/Weather_Station/wind_gust_speed",
  "/Weather_Station/wind_magnitude",
  "/Weather_Station/wind_direction",
};

const int topic_size = sizeof(topics)/sizeof(const char*);

WiFiClient net;
MQTTClient client;
BMP280_SDW bmp280;    // I2C Address 0x77
AHT20 aht20;          // I2C Address 0x38
Adafruit_AM2315 am2315;

const char clientId[] = "ESP_Weather_Station";
const char userId[] = "MQTT_USER";
const char userPswd[] = "MQTT_PASSWORD";
const char MQTTBroker[] = "192.168.XXX.XXX";

bool bAm2315 = false;
bool bBmp280 = false;
bool bAht20 = false;
float bmp280_temp = 0; 
float bmp280_pressure = 0;
float am2315_temp = 0;    
float am2315_humidity = 0; 
float aht20_humidity = 0;
float aht20_temp = 0;

// Wind direction and speed variables
int wind_speed[RECORDS];
float wind_dir[RECORDS];
int wind_idx = 0;
int wind_count = 0;
int max_wind_count = 0;
int wind_count_sum = 0;
Coordinates point = Coordinates();
float x_sum = 0;
float y_sum = 0;  
float wind_gust = 0;
float wind_average = 0;
float wind_mag = 0;
float wind_angle = 0;


String httpServer = "";
int httpPort = 0;
unsigned long lastMillis = 0; 
volatile bool buttonPress = false;

FSInfo fs_info;

/******************************************************************************/
/* Arduino Ovter The Air                                                      */
/* Start ArduinoOTA via  with the same hostname and password                  */
/******************************************************************************/
void setup_ota() {
  ArduinoOTA.setHostname(WiFiSettings.hostname.c_str());
  ArduinoOTA.setPassword(WiFiSettings.password.c_str());
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else  // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

/******************************************************************************/
/* WiFi Event Handler                                                         */
/* We want to catch the following events:                                     */
/******************************************************************************/
WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;

/******************************************************************************/
/* MQTT Connection Manager                                                    */
/******************************************************************************/
void mqttConnect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  // while (!client.connect("ESP_Weather_Station", "HA_MQTT")) {
  while (!client.connect(clientId, userId, userPswd)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nConnected!\n");
  Serial.print("Number of Topics: ");
  Serial.println(topic_size);
  Serial.println("MQTT Subscriptions:");
  for (int i = 0; i < topic_size; i++) {
    Serial.print("  ");
    Serial.println(topics[i]);
    client.subscribe(topics[i]);
  }
  Serial.println("Finished Subcriptions.\n");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

/*****************************************************************************/
/* Wind Speed Interrupt                                                      */
/*   Used to count revelutions of the wind cup anemometer                    */
/*   used long int wind_count                                                */
/*   1 count per second is a wind speed of 1.492 MPH                         */
/*   Huricane force winds of 150 MPH would result in 100 counts per secont   */
/*   that would be an interupt every 10mS.                                   */
/*****************************************************************************/
void IRAM_ATTR anemometer_isr() {
  wind_count++;
}

/*****************************************************************************/
/* Wind Direction function                                                   */
/* Return degrees for the wind direction                                     */
/*   North:         0 deg.               South:          180 deg.            */
/*   North East:   45 deg.               South West:     225 deg.            */
/*   East :        90 deg.               West:           270 deg,            */
/*   South East:  135 deg.               North West:     315 deg.            */
/*****************************************************************************/
float get_wind_dir() {
  int adc = analogRead(0);
  if (adc < 228) return 112.5;
  if (adc < 238) return 67.5;
  if (adc < 254) return 90.0;
  if (adc < 288) return 157.5;
  if (adc < 332) return 135.0;
  if (adc < 370) return 202.5;
  if (adc < 433) return 180.0;
  if (adc < 502) return 22.5;
  if (adc < 581) return 45.0;
  if (adc < 651) return 247.5;
  if (adc < 696) return 225.0;
  if (adc < 764) return 337.5;
  if (adc < 820) return 0.0;
  if (adc < 866) return 292.5;
  if (adc < 920) return 315.0;
  return 270;
}

/******************************************************************************/
/* Wind Speed Task:                                                           */
/*   Stores the current wind_count and wind direction into arrays, and then   */
/*   clears the wind count variable. I also increments the wind data index    */
/*   and handles roll over.                                                   */
/******************************************************************************/
void wind_task() {
  wind_speed[wind_idx] = wind_count;
  wind_count = 0;
  wind_dir[wind_idx] = get_wind_dir();
  
  Serial.print("WS_TASK: ");
  Serial.printf("Count: %d, ", wind_speed[wind_idx]);
  Serial.printf("Wind direction: %f, ", wind_dir[wind_idx]);
  Serial.printf("Index: %d\n", wind_idx);
  
  wind_idx++;
  wind_idx %= RECORDS;
}

/******************************************************************************/
/* Report Task:                                                               */
/*   Reads the I2C sensors and calcules the values to be reported to the MQTT */
/*   Broker.                                                                  */
/******************************************************************************/
void report_task() {
  Serial.println("REPORT_TASK: "); 
  //****************************************************
  // Read the I2C Sensors
  // float bmp280_temp = 0; 
  // float bmp280_pressure = 0;
  // float am2315_temp = 0;    
  // float am2315_humidity = 0; 
  // float aht20_humidity = 0;
  // float aht20_temp = 0;
  //****************************************************
  
  // Get the AHT20 sensor temperature and Humidity
  // humidity is read in raw format converted to %RH
  if (bAht20) {
    aht20.getSensor(&aht20_humidity, &aht20_temp);
    aht20_temp = 1.8 * aht20_temp + 32; // Convert to Ferinheight
  } else {
    aht20_temp = -99.0;
    aht20_humidity = -99.0;
  }
  
  // Get BMP280 Temperature and Pressure
  // BPM280 Temperature is in C
  if (bBmp280) {
    bmp280.getSensor(&bmp280_pressure, &bmp280_temp);
    bmp280_pressure = bmp280_pressure / 1000; // Convert to KpA
    bmp280_temp = 1.8 * bmp280_temp + 32; // Convert to Ferinheight
  } else {
    bmp280_pressure = -99.0;
    bmp280_temp = -99.0; 
  }

  // Get the AM2315 Temperature and humidity
  if (bAm2315) {
    am2315.readTemperatureAndHumidity(&am2315_temp, &am2315_humidity);
    am2315_temp = 1.8 * am2315_temp + 32; //Convert to Ferinheight
  } else {
    am2315_temp = -99.0;
    am2315_humidity = -99.0; 
  }

  /**************************************************************************/
  /* Calculate Average wind speed, Wind gusts and direction.                */
  /* Anenomiter has a conversion of 1.492 MPH per count per second.         */
  /* Since our timing system is in mS -> 1492 MPH per count per mS.         */
  /*                                                                        */
  /* Wind Gusts: We are counting the number of revolutions per the          */
  /* WIND_PERIOD (5000 mS), and then looking for the max value saved over   */
  /* REPORT_PERIOD (120000 ms).                                             */
  /* wind_gust = max_wind_count * 1492 / WIND_PERIOD                        */ 
  /*                                                                        */
  /* Ave Wind Speed: we will calaculate the total number ouf counts for the */
  /* from each WIND_PERIOD in the REPORT_PERIOD (120000 mS) then take the   */
  /* average.                                                               */
  /* ave_wind_speed = wind_count_sum * 1492 / REPORT_PERIOD                 */  
  /**************************************************************************/
  max_wind_count = 0;
  wind_count_sum = 0;
  x_sum = 0;
  y_sum = 0;  

  for (int i = 0; i < RECORDS; i++) {
    if (wind_speed[i] > max_wind_count) {
      max_wind_count = wind_speed[i];
    }
    wind_count_sum += wind_speed[i];
    // Convert the wind speed and wind direction to carteasian coordinates
    point.fromPolar(float(wind_speed[i]), wind_dir[i] * (3.1416/180));
    // Summ up all the x and y values
    x_sum += point.getX();
    y_sum += point.getY();
  }
  //Convert the summed vectors back to polor coordinates.
  point.fromCartesian(x_sum, y_sum);
  // Get the magnitude and and get the avertage.
  wind_mag = point.getR() * 1491 / REPORT_PERIOD;
  // Convert from radians to degrees
  wind_angle = point.getAngle() * (180.0 / 3.1416);
  // Make sure the angle is not negative.
  if (wind_angle < 0) {
    wind_angle += 360.0;
  }
  // round to the nearest 22.5 degrees
  wind_angle = round(wind_angle/22.5) * 22.5;
  wind_gust = float(max_wind_count) * 1492 / WIND_PERIOD;
  wind_average = float(wind_count_sum) * 1491 / REPORT_PERIOD;

  /************************************************************/
  /* MQTTT Puplish statements                                 */       
  /* 0 "/Weather_Station/temp_1",                             */
  /* 1 "/Weather_Station/temp_2",                             */
  /* 2 "/Weather_Station/temp_3",                             */
  /* 3 "/Weather_Station/humid_1",                            */
  /* 4 "/Weather_Station/humid_2",                            */
  /* 5 "/Weather_Station/press",                              */
  /* 6 "/Weather_Station/ave_wind_speed",                     */
  /* 7 "/Weather_Station/wind_gust_speed",                    */
  /* 8 "/Weather_Station/wind_magnitude",                     */
  /* 9 "/Weather_Station/wind_direction"                      */
  /************************************************************/

  Serial.printf("Temp 1: %0.1f C\n", am2315_temp);
  client.publish(topics[0], String(am2315_temp));
  Serial.printf("Temp 2: %0.1f C\n", bmp280_temp);
  client.publish(topics[1], String(bmp280_temp));
  Serial.printf("Temp 3: %0.1f C\n", aht20_temp);
  client.publish(topics[2], String(aht20_temp));
  Serial.printf("Humid 1: %0.1f %\n", am2315_humidity);
  client.publish(topics[3], String(am2315_humidity));
  Serial.printf("Humid 2: %0.1f %\n", aht20_humidity);
  client.publish(topics[4], String(aht20_humidity));
  Serial.printf("Pressure: %0.1f kPa\n", bmp280_pressure);
  client.publish(topics[5], String(bmp280_pressure));
  Serial.printf("Average Wind Speed: %0.1f mph\n", wind_average);
  client.publish(topics[6], String(wind_average));
  Serial.printf("Peak Wind Gust: %0.1f mph\n", wind_gust);
  client.publish(topics[7], String(wind_gust));
  Serial.printf("Weigted Ave. Wind Speed: %s mph\n", String(wind_mag));
  client.publish(topics[8], String(wind_mag));
  Serial.printf("Wind Direction: %0.1f degrees\n", wind_angle);
  client.publish(topics[9], String(wind_angle));
}


void setup() {
  Serial.begin(115200); 
  Wire.begin();

  Serial.println();
  Serial.println(F("-----------------------------"));
  Serial.println(F("---  ESP Weather Station  ---"));
  Serial.println(F("-----------------------------"));

  /**********************************************/
  /*  Configure GPIO & Pin Interrupt            */
  /**********************************************/
  pinMode(LED_PIN, OUTPUT);
  pinMode(DIR_PIN, INPUT);
  pinMode(WIND_PIN, INPUT_PULLUP);
  attachInterrupt(WIND_PIN, anemometer_isr, FALLING);
  // attachInterrupt(digitalPinToInterrupt(WIND_PIN), anemometer_isr, FALLING);

  /************************************************
    Initialize SPI Flash File System (LittleFS)
  ************************************************/
  Serial.print(F("Mounting the SPI Flash File System... "));
  if (!LittleFS.begin()) {
    Serial.println(F("Failed!"));
  } else {
    Serial.println(F("Success!"));
  }

  // FSInfo fs_info;
  LittleFS.info(fs_info);
  Serial.printf("ESP Flash Chip Size: %4.1f kBytes\n", (float(ESP.getFlashChipSize()) / 1024));
  Serial.printf("File System Size: %4.1f kBytes\n", (float(fs_info.totalBytes) / 1024));
  Serial.printf("File System Used: %4.1f kBytes\n", (float(fs_info.usedBytes) / 1024));

  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event)
  {
    Serial.print("Station connected");
    Serial.println(WiFi.localIP());
    Serial.printf("  IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("  Gateway Address: ");
    Serial.println(WiFi.gatewayIP());
    Serial.printf("  Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
  });

  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event)
  {
    Serial.println("Station disconnected");
  });
  
  
  /**********************************************************
    Start WifiSettings

    This will check to see if we have a WiFi SSID and
    Pasword stored in the SPIFFS. If they don't it will enter
    AP mode and start the configuration web interface. If
    they do exsist It will try to connect to the stored SSID
    using the stored password. If it is unable to connect
    after 60 sec. it will enter AP mode and start the
    configuration web interface.

    First we need to configure any custom options we want to see
    in configuration web page.
  **********************************************************/
  httpServer = "http://" + WiFiSettings.string("HTTP_Server", "L32server.local");
  httpPort = WiFiSettings.integer("HTTP_Port", 8080);
  WiFiSettings.hostname = "ESP_WEATHER_STATION";
  WiFiSettings.password = "12345678";
  //WiFi.onEvent(WiFiEventHandler);

  // Force WPA secured WiFi for the software access point.
  // Because OTA is remote code execution (RCE) by definition, the password
  // should be kept secret. By default,  will become an insecure
  // WiFi access point and happily tell anyone the password. The password
  // will instead be provided on the Serial connection, which is a bit safer.
  WiFiSettings.secure = true;

  // Use stored credentials to connect to your WiFi access point.
  // If no credentials are stored or if the access point is out of reach,
  // an access point will be started with a captive portal to configure WiFi.
  WiFiSettings.connect(true, 30);
  
   // Set up OTA during regular execution
  setup_ota();  // If you also want the OTA during regular execution

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
  /* MQTT Client Setup                                                        */
  /****************************************************************************/
  client.begin(MQTTBroker, net);
  client.onMessage(messageReceived);

  mqttConnect();

  /****************************************************************************/
  /*   Start the Scheduler                                                    */
  /****************************************************************************/
  ts.startNow();
}
 
void loop() {
  ArduinoOTA.handle();
  delay(10);
  if (!client.connected()) {
    mqttConnect();
  }
  client.loop();
  ts.execute();

}
