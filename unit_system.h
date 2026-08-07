#pragma once

#include <math.h>
#include <stdint.h>

enum class UnitSystem : uint8_t {
  USA = 0,
  EuropeanUnion = 1,
  UnitedKingdom = 2
};

inline bool unitSystemIsValid(int value) {
  return value >= static_cast<int>(UnitSystem::USA) &&
      value <= static_cast<int>(UnitSystem::UnitedKingdom);
}

inline bool usesFahrenheit(UnitSystem system) {
  return system == UnitSystem::USA;
}

inline bool usesKilometersPerHour(UnitSystem system) {
  return system == UnitSystem::EuropeanUnion;
}

inline bool usesInches(UnitSystem system) {
  return system == UnitSystem::USA;
}

inline bool usesFeet(UnitSystem system) {
  return system == UnitSystem::USA;
}

inline bool usesInchesHg(UnitSystem system) {
  return system == UnitSystem::USA;
}

inline float celsiusToFahrenheit(float valueC) {
  return valueC * 1.8f + 32.0f;
}

inline float fahrenheitToCelsius(float valueF) {
  return (valueF - 32.0f) / 1.8f;
}

inline float mphToKmh(float valueMph) {
  return valueMph * 1.609344f;
}

inline float inchesToMillimeters(float valueInches) {
  return valueInches * 25.4f;
}

inline float hpaToInHg(float valueHpa) {
  return valueHpa * 0.0295299831f;
}

inline float metersToFeet(float valueMeters) {
  return valueMeters / 0.3048f;
}

inline float feetToMeters(float valueFeet) {
  return valueFeet * 0.3048f;
}

inline float reportedTemperature(float valueC, UnitSystem system) {
  return usesFahrenheit(system) ? celsiusToFahrenheit(valueC) : valueC;
}

inline float reportedWindSpeed(float valueMph, UnitSystem system) {
  return usesKilometersPerHour(system) ? mphToKmh(valueMph) : valueMph;
}

inline float reportedRain(float valueInches, UnitSystem system) {
  const float value = usesInches(system)
      ? valueInches
      : inchesToMillimeters(valueInches);
  return roundf(value * 100.0f) / 100.0f;
}

inline float reportedPressure(float valueHpa, UnitSystem system) {
  return usesInchesHg(system) ? hpaToInHg(valueHpa) : valueHpa;
}

inline float reportedElevation(float valueMeters, UnitSystem system) {
  return usesFeet(system) ? metersToFeet(valueMeters) : valueMeters;
}

inline float elevationEntryToMeters(float value, UnitSystem system) {
  return usesFeet(system) ? feetToMeters(value) : value;
}

inline float rainTipEntryToMicrometers(float value, UnitSystem system) {
  return usesInches(system) ? value * 25400.0f : value * 1000.0f;
}

inline float rainTipMicrometersToEntry(float micrometers, UnitSystem system) {
  return usesInches(system) ? micrometers / 25400.0f : micrometers / 1000.0f;
}
