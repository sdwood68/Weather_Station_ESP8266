#pragma once

#include <stdint.h>

inline bool portalTimeoutExpired(
    uint32_t now,
    uint32_t portalStartedAt,
    uint32_t timeoutMilliseconds) {
  return static_cast<uint32_t>(now - portalStartedAt) >= timeoutMilliseconds;
}

inline bool requiredConfigurationMissing(
    bool hasWifiSsid,
    bool hasOtaPassword,
    bool hasMqttHost,
    bool hasMqttUser,
    bool hasMqttPassword) {
  return !hasWifiSsid || !hasOtaPassword || !hasMqttHost ||
      !hasMqttUser || !hasMqttPassword;
}
