#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

struct WindObservation {
  float sustainedMph;
  float gustMph;
  float directionDegrees;
  uint32_t pulseCount;
  uint16_t sampleCount;
  uint16_t gustPulseCount;
  bool calm;
};

inline WindObservation calculateWindObservation(
    const uint16_t* pulses,
    const float* directions,
    size_t sampleCount,
    float sampleSeconds,
    size_t gustSamples,
    float mphPerPulsePerSecond,
    float calmThresholdMph) {
  WindObservation result = {};
  if (sampleCount == 0 || sampleSeconds <= 0.0f || gustSamples == 0) {
    result.calm = true;
    return result;
  }

  float northFrom = 0.0f;
  float eastFrom = 0.0f;
  uint32_t rollingGustPulses = 0;

  for (size_t i = 0; i < sampleCount; ++i) {
    const float speedMph =
        pulses[i] * mphPerPulsePerSecond / sampleSeconds;
    const float radians = directions[i] * PI / 180.0f;
    northFrom += speedMph * cosf(radians);
    eastFrom += speedMph * sinf(radians);
    result.pulseCount += pulses[i];

    rollingGustPulses += pulses[i];
    if (i >= gustSamples) {
      rollingGustPulses -= pulses[i - gustSamples];
    }
    if (i + 1 >= gustSamples &&
        rollingGustPulses > result.gustPulseCount) {
      result.gustPulseCount = rollingGustPulses;
    }
  }

  result.sampleCount = static_cast<uint16_t>(sampleCount);
  const float observationSeconds = sampleCount * sampleSeconds;
  result.sustainedMph =
      result.pulseCount * mphPerPulsePerSecond / observationSeconds;

  const size_t effectiveGustSamples =
      sampleCount < gustSamples ? sampleCount : gustSamples;
  if (sampleCount < gustSamples) {
    result.gustPulseCount = result.pulseCount;
  }
  result.gustMph =
      result.gustPulseCount * mphPerPulsePerSecond /
      (effectiveGustSamples * sampleSeconds);

  result.calm = result.sustainedMph < calmThresholdMph;
  if (!result.calm) {
    float direction = atan2f(eastFrom, northFrom) * 180.0f / PI;
    if (direction < 0.0f) {
      direction += 360.0f;
    }
    result.directionDegrees = direction;
  }

  return result;
}
