// Wind direction and speed variables
uint16_t wind_pulses[RECORDS];
float wind_dir[RECORDS];
uint16_t wind_idx = 0;
uint16_t wind_samples = 0;
volatile unsigned long wind_count = 0;
volatile uint32_t rain_tip_count = 0;
volatile uint32_t last_rain_tip_micros = 0;
float minute_rain_history[RAIN_HISTORY_MINUTES];
uint16_t rain_history_index = 0;
uint16_t rain_history_count = 0;
float rain_session_total_inches = 0.0f;
float outdoor_temperature_history[PRESSURE_HISTORY_SAMPLES];
uint16_t pressure_history_index = 0;
uint16_t pressure_history_count = 0;
float station_pressure_history[PRESSURE_TENDENCY_SAMPLES];
uint16_t pressure_tendency_index = 0;
uint16_t pressure_tendency_count = 0;

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

/******************************************************************************/
/* Rain Gauge Interrupt and ASOS-style one-minute accumulation                */
/******************************************************************************/
void IRAM_ATTR rain_gauge_isr() {
  const uint32_t now = micros();
  if (static_cast<uint32_t>(now - last_rain_tip_micros) >= RAIN_DEBOUNCE_US) {
    last_rain_tip_micros = now;
    rain_tip_count++;
  }
}

void resetRainHistory() {
  noInterrupts();
  rain_tip_count = 0;
  last_rain_tip_micros = 0;
  interrupts();

  for (uint16_t i = 0; i < RAIN_HISTORY_MINUTES; ++i) {
    minute_rain_history[i] = 0.0f;
  }
  rain_history_index = 0;
  rain_history_count = 0;
  rain_session_total_inches = 0.0f;
}

void publishRainWindow(
    HASensorNumber& sensor,
    uint16_t minutes,
    const char* label) {
  const float total = sumLatestRainInches(
      minute_rain_history,
      RAIN_HISTORY_MINUTES,
      rain_history_index,
      rain_history_count,
      minutes);
  if (!isnan(total)) {
    const float rounded = reportedRain(total, unitSystem);
    sensor.setValue(rounded);
    Serial.printf("%s: %0.2f %s\n", label, rounded,
                  usesInches(unitSystem) ? "in" : "mm");
  } else {
    Serial.printf("%s pending: %u/%u valid one-minute periods\n",
                  label,
                  rain_history_count,
                  minutes);
  }
}

void rain_task() {
  noInterrupts();
  const uint32_t capturedTips = rain_tip_count;
  rain_tip_count = 0;
  interrupts();

  const float tipSizeMillimeters = rainTipMicrometers / 1000.0f;
  const float correctedInches =
      calculateAsosMinuteRainInches(capturedTips, tipSizeMillimeters);
  if (isnan(correctedInches)) {
    Serial.println(F("Rain minute rejected: invalid rain-tip calibration"));
    return;
  }

  minute_rain_history[rain_history_index] = correctedInches;
  rain_history_index = (rain_history_index + 1U) % RAIN_HISTORY_MINUTES;
  if (rain_history_count < RAIN_HISTORY_MINUTES) {
    rain_history_count++;
  }
  rain_session_total_inches += correctedInches;

  const float minuteOutput = reportedRain(correctedInches, unitSystem);
  rainTipCountSensor.setValue(capturedTips);
  rainOneMinute.setValue(minuteOutput);
  rainSessionTotal.setValue(
      reportedRain(rain_session_total_inches, unitSystem));
  Serial.printf(
      "ASOS rain minute: %lu tips at %0.3f in/tip, %0.4f in corrected, %0.2f in reported\n",
      static_cast<unsigned long>(capturedTips),
      rainTipMicrometers / 25400.0f,
      correctedInches,
      minuteOutput);

  publishRainWindow(rainOneHour, 60, "Latest 60-minute rain");
  publishRainWindow(rainThreeHour, 180, "Latest 3-hour rain");
  publishRainWindow(rainSixHour, 360, "Latest 6-hour rain");
  publishRainWindow(rainTwentyFourHour, 1440, "Latest 24-hour rain");
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

  wind_pulses[wind_idx] =
      static_cast<uint16_t>(min(capturedCount, 65535UL));
  float direction = get_wind_dir() + WIND_DIRECTION_OFFSET_DEGREES;
  if (direction >= 360.0f) direction -= 360.0f;
  if (direction < 0.0f) direction += 360.0f;
  wind_dir[wind_idx] = direction;
  wind_idx++;
  wind_idx %= RECORDS;
  if (wind_samples < RECORDS) wind_samples++;
}

/******************************************************************************/
/* Report Task:                                                               */
/*   Reads the I2C sensors and calcules the values to be reported to the MQTT */
/*   Broker.                                                                  */
/******************************************************************************/
void report_task() {
  const unsigned long reportStartedAt = millis();
  Serial.println("REPORT_TASK: ");
  float fTemp1 = 0.0; 
  float fTemp2 = 0.0;
  float fHumidity = 0.0;
  float fIntTemp = 0.0;
  uint8_t humidityReadings = 0;
  uint8_t internalTempReadings = 0;
  float stationPressureHpa = NAN;
  float outdoorTemperatureC = NAN;
  
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
    bmp280.getSensor(&fTemp1, &fTemp2);
    stationPressureHpa = (fTemp1 + pressureOffsetPa) / 100.0f;
    fTemp2 = 1.8f * fTemp2 + 32.0f;
    Serial.printf("BMP280 corrected station pressure: %0.2f hPa\n",
                  stationPressureHpa);
    Serial.printf("BMP280 Temperature: %0.1f F\n", fTemp2);
    fIntTemp += fTemp2;
    internalTempReadings++;
  }

  // Get the AM2315 outdoor temperature and humidity. Never derive or publish
  // values from a failed read; output variables may otherwise contain stale
  // data left by the preceding sensor operation.
  if (bAm2315) {
    float am2315TemperatureC = NAN;
    float am2315Humidity = NAN;
    const bool am2315ReadOk = am2315.readTemperatureAndHumidity(
        &am2315TemperatureC, &am2315Humidity);
    const bool am2315ValuesValid =
        am2315ReadOk &&
        isfinite(am2315TemperatureC) &&
        isfinite(am2315Humidity) &&
        am2315TemperatureC >= -40.0f &&
        am2315TemperatureC <= 80.0f &&
        am2315Humidity >= 0.0f &&
        am2315Humidity <= 100.0f;

    if (!am2315ValuesValid) {
      Serial.printf(
          "AM2315 reading rejected (ok=%d, temperature=%0.2f C, humidity=%0.2f %%)\n",
          am2315ReadOk,
          am2315TemperatureC,
          am2315Humidity);
    } else {
      outdoorTemperatureC = am2315TemperatureC;
      const float outdoorTemperatureF =
          1.8f * am2315TemperatureC + 32.0f;
      Serial.printf("AM2315 Temperature: %0.1f F\n", outdoorTemperatureF);
      Serial.printf("AM2315 Humidity: %0.1f %%\n", am2315Humidity);
      fHumidity += am2315Humidity;
      humidityReadings++;
      temperature.setValue(reportedTemperature(outdoorTemperatureC, unitSystem));

      const float dewPointC =
          calculateDewPointC(outdoorTemperatureC, am2315Humidity);
      if (!isnan(dewPointC)) {
        const float dewPointF = 1.8f * dewPointC + 32.0f;
        dewPoint.setValue(reportedTemperature(dewPointC, unitSystem));
        Serial.printf("Dew point: %0.1f F\n", dewPointF);
      } else {
        Serial.println(F("Dew-point result rejected by sanity checks"));
      }

      const float calculatedHeatIndexF =
          calculateNwsHeatIndexF(outdoorTemperatureF, am2315Humidity);
      if (!isnan(calculatedHeatIndexF)) {
        heatIndex.setValue(usesFahrenheit(unitSystem)
                               ? calculatedHeatIndexF
                               : fahrenheitToCelsius(calculatedHeatIndexF));
        Serial.printf("NWS heat index: %0.1f F\n", calculatedHeatIndexF);
      } else {
        Serial.println(F("Heat-index result rejected by sanity checks"));
      }
    }
  }

  if (!isnan(stationPressureHpa)) {
    airPressure.setValue(reportedPressure(stationPressureHpa, unitSystem));

    if (pressure_tendency_count == PRESSURE_TENDENCY_SAMPLES) {
      float changeHpa =
          stationPressureHpa - station_pressure_history[pressure_tendency_index];
      changeHpa = roundf(changeHpa * 10.0f) / 10.0f;
      pressureChange3h.setValue(reportedPressure(changeHpa, unitSystem));
      const char* trend =
          changeHpa > 0.0f ? "rising" : (changeHpa < 0.0f ? "falling" : "steady");
      pressureTrend.setValue(trend);
      Serial.printf("3-hour pressure change: %+0.1f hPa (%s)\n", changeHpa, trend);
    } else {
      Serial.printf("Pressure trend pending: %u/%u station-pressure samples\n",
                    pressure_tendency_count,
                    PRESSURE_TENDENCY_SAMPLES);
    }
    station_pressure_history[pressure_tendency_index] = stationPressureHpa;
    pressure_tendency_index++;
    pressure_tendency_index %= PRESSURE_TENDENCY_SAMPLES;
    if (pressure_tendency_count < PRESSURE_TENDENCY_SAMPLES) {
      pressure_tendency_count++;
    }

    if (stationElevationMeters != UNCONFIGURED_ELEVATION_METERS) {
      const float altimeterHpa = calculateAltimeterSettingHpa(
          stationPressureHpa, stationElevationMeters);
      if (!isnan(altimeterHpa)) {
        altimeterSetting.setValue(reportedPressure(altimeterHpa, unitSystem));
        Serial.printf("Altimeter setting: %0.2f hPa\n", altimeterHpa);
      }
    } else {
      Serial.println(F("Altimeter and sea-level pressure pending: configure station elevation"));
    }

    if (stationElevationMeters != UNCONFIGURED_ELEVATION_METERS &&
        !isnan(outdoorTemperatureC)) {
      if (pressure_history_count == PRESSURE_HISTORY_SAMPLES) {
        const float temperatureTwelveHoursAgoC =
            outdoor_temperature_history[pressure_history_index];
        const float seaLevelHpa = calculateSeaLevelPressureHpa(
            stationPressureHpa,
            stationElevationMeters,
            outdoorTemperatureC,
            temperatureTwelveHoursAgoC);
        if (!isnan(seaLevelHpa)) {
          seaLevelPressure.setValue(reportedPressure(seaLevelHpa, unitSystem));
          Serial.printf(
              "Sea-level pressure: %0.2f hPa (current %0.1f C, 12-hour %0.1f C)\n",
              seaLevelHpa,
              outdoorTemperatureC,
              temperatureTwelveHoursAgoC);
        }
      } else {
        Serial.printf("Sea-level pressure pending: %u/%u temperature samples\n",
                      pressure_history_count,
                      PRESSURE_HISTORY_SAMPLES);
      }

      outdoor_temperature_history[pressure_history_index] = outdoorTemperatureC;
      pressure_history_index++;
      pressure_history_index %= PRESSURE_HISTORY_SAMPLES;
      if (pressure_history_count < PRESSURE_HISTORY_SAMPLES) {
        pressure_history_count++;
      }
    }
  }
  if (internalTempReadings > 0) {
    const float averageInternalF = fIntTemp / internalTempReadings;
    boxTemperature.setValue(usesFahrenheit(unitSystem)
                                ? averageInternalF
                                : fahrenheitToCelsius(averageInternalF));
  }
  if (humidityReadings > 0) {
    humidity.setValue(fHumidity / humidityReadings);
  }

  uint16_t orderedPulses[RECORDS];
  float orderedDirections[RECORDS];
  const uint16_t first = wind_samples == RECORDS ? wind_idx : 0;
  for (uint16_t i = 0; i < wind_samples; ++i) {
    const uint16_t source = (first + i) % RECORDS;
    orderedPulses[i] = wind_pulses[source];
    orderedDirections[i] = wind_dir[source];
  }

  const WindObservation observation = calculateWindObservation(
      orderedPulses,
      orderedDirections,
      wind_samples,
      WIND_SAMPLE_PERIOD_MS / 1000.0f,
      GUST_RECORDS,
      ANEMOMETER_MPH_PER_HZ,
      CALM_THRESHOLD_MPH);

  const float reportedSustainedSpeed =
      reportedWindSpeed(observation.sustainedMph, unitSystem);
  const float reportedGustSpeed =
      reportedWindSpeed(observation.gustMph, unitSystem);
  const char* windUnit = usesKilometersPerHour(unitSystem) ? "km/h" : "mph";
  Serial.printf("2-minute sustained wind: %0.2f %s\n",
                reportedSustainedSpeed, windUnit);
  Serial.printf("3-second wind gust: %0.2f %s\n",
                reportedGustSpeed, windUnit);
  if (observation.calm) {
    Serial.println(F("2-minute wind direction: calm (0 degrees)"));
  } else {
    Serial.printf("2-minute true wind direction: %0.1f degrees\n",
                  observation.directionDegrees);
  }
  Serial.printf("Wind diagnostics: %lu pulses, %u samples, %u gust pulses\n",
                observation.pulseCount,
                observation.sampleCount,
                observation.gustPulseCount);

  windSpeed.setValue(reportedSustainedSpeed);
  windGust.setValue(reportedGustSpeed);
  windDirection.setValue(observation.directionDegrees);
  windPulseCount.setValue(observation.pulseCount);
  windSampleCount.setValue(observation.sampleCount);
  windGustPulseCount.setValue(observation.gustPulseCount);

  Serial.printf("POWER_BASELINE report task: %lu ms\n",
                millis() - reportStartedAt);

}
