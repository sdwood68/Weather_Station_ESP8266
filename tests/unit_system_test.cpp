#include <assert.h>
#include <math.h>

#include "../unit_system.h"

static bool near(float actual, float expected, float tolerance = 0.001f) {
  return fabsf(actual - expected) <= tolerance;
}

int main() {
  assert(unitSystemIsValid(0));
  assert(unitSystemIsValid(1));
  assert(unitSystemIsValid(2));
  assert(!unitSystemIsValid(-1));
  assert(!unitSystemIsValid(3));

  assert(usesFahrenheit(UnitSystem::USA));
  assert(!usesFahrenheit(UnitSystem::EuropeanUnion));
  assert(!usesFahrenheit(UnitSystem::UnitedKingdom));
  assert(usesKilometersPerHour(UnitSystem::EuropeanUnion));
  assert(!usesKilometersPerHour(UnitSystem::UnitedKingdom));

  assert(near(reportedTemperature(20.0f, UnitSystem::USA), 68.0f));
  assert(near(reportedTemperature(20.0f, UnitSystem::EuropeanUnion), 20.0f));
  assert(near(reportedWindSpeed(10.0f, UnitSystem::EuropeanUnion), 16.09344f));
  assert(near(reportedWindSpeed(10.0f, UnitSystem::UnitedKingdom), 10.0f));
  assert(near(reportedRain(1.0f, UnitSystem::EuropeanUnion), 25.4f));
  assert(near(reportedRain(1.0f, UnitSystem::USA), 1.0f));
  assert(near(reportedPressure(1013.25f, UnitSystem::USA), 29.9213f));
  assert(near(reportedPressure(1013.25f, UnitSystem::UnitedKingdom), 1013.25f));
  assert(near(reportedElevation(100.0f, UnitSystem::USA), 328.08398f));
  assert(near(elevationEntryToMeters(328.08398f, UnitSystem::USA), 100.0f));
  assert(near(rainTipEntryToMicrometers(0.01f, UnitSystem::USA), 254.0f));
  assert(near(rainTipEntryToMicrometers(0.2f, UnitSystem::EuropeanUnion), 200.0f));
  return 0;
}
