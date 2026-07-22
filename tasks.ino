// Wind direction and speed variables
int wind_speed[RECORDS];
float wind_dir[RECORDS];
int wind_idx = 0;
volatile unsigned long wind_count = 0;
int max_wind_count = 0;
int wind_count_sum = 0;
Coordinates point = Coordinates();
float x_sum = 0;
float y_sum = 0;  
float wind_gust = 0;
float wind_average = 0;
float wind_mag = 0;
float wind_angle = 0;

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
  noInterrupts();
  const unsigned long capturedCount = wind_count;
  wind_count = 0;
  interrupts();

  wind_speed[wind_idx] = static_cast<int>(capturedCount);
  wind_dir[wind_idx] = get_wind_dir();
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
  float fTemp1 = 0.0; 
  float fTemp2 = 0.0;
  float fHumidity = 0.0;
  float fIntTemp = 0.0;
  uint8_t humidityReadings = 0;
  uint8_t internalTempReadings = 0;
  
  // Get the AHT20 sensor temperature and Humidity
  // humidity is read in raw format converted to %RH
  if (bAht20) {
    aht20.getSensor(&fTemp1, &fTemp2);
    fTemp2 =  1.8 * fTemp2 + 32; // Convert to Ferinheight
    Serial.printf("AHT20 Humidity: %0.1f %%\n", fTemp1);
    Serial.printf("AHT20 Temperature: %0.1f F\n", fTemp2);
    fHumidity += fTemp1;
    humidityReadings++;
    fIntTemp += fTemp2;
    internalTempReadings++;
  }
  
  // Get BMP280 Temperature and Pressure
  // BPM280 Temperature is in C
  if (bBmp280) {
    // bmp280.getSensor(&bmp280_pressure, &bmp280_temp);
    bmp280.getSensor(&fTemp1, &fTemp2);
    fTemp1 = fTemp1 / 1000; // Convert to KpA
    fTemp2 = 1.8 * fTemp2 + 32; // Convert to Ferinheight
    Serial.printf("BMP280 Pressure: %0.3f kPa\n", fTemp1);
    Serial.printf("BMP280 Temperature: %0.1f F\n", fTemp2);
    airPressure.setValue(fTemp1);
    fIntTemp += fTemp2;
    internalTempReadings++;
  }

  // Get the AM2315 Temperature and humidity
  if (bAm2315) {
    // am2315.readTemperatureAndHumidity(&am2315_temp, &am2315_humidity);
    am2315.readTemperatureAndHumidity(&fTemp1, &fTemp2);
    fTemp1 = 1.8 * fTemp1 + 32; //Convert to Ferinheight
    Serial.printf("AM2315 Temperature: %0.1f F\n", fTemp1);
    Serial.printf("AM2315 Humidity: %0.1f %%\n", fTemp2);
    fHumidity += fTemp2;
    humidityReadings++;
    temperature.setValue(fTemp1);
  }

  if (internalTempReadings > 0) {
    boxTemperature.setValue(fIntTemp / internalTempReadings);
  }
  if (humidityReadings > 0) {
    humidity.setValue(fHumidity / humidityReadings);
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

  Serial.printf("Average Wind Speed: %0.1f mph\n", wind_average);
  Serial.printf("Peak Wind Gust: %0.1f mph\n", wind_gust);
  Serial.printf("Weigted Ave. Wind Speed: %0.1f mph\n", wind_mag);
  Serial.printf("Wind Direction: %0.1f degrees\n", wind_angle);

  windSpeed.setValue(wind_average);
  windGust.setValue(wind_gust);
  windMagnitude.setValue(wind_mag);
  windDirection.setValue(wind_angle);

}