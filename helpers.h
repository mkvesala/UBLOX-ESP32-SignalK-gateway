#pragma once
#include <Arduino.h>
#include <cmath>

// Float validator — used throughout all brokers and processor
inline bool validf(float x) { return !isnan(x) && isfinite(x); }

// Shortest angular difference on a circle [0, 2π), in radians
// Returns value in [-π, +π]
inline float computeAngDiffRad(float a, float b) {
    float d = fmodf(a - b + 3.0f * (float)M_PI, 2.0f * (float)M_PI) - (float)M_PI;
    return d;
}

// Normalise angle to [0, 2π)
inline float normaliseRad(float a) {
    return fmodf(a + 4.0f * (float)M_PI, 2.0f * (float)M_PI);
}
