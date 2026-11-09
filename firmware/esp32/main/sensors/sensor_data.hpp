#pragma once

#include <cstdint>

struct SensorData {
    float temperature_celsius = 0.0f;
    float humidity_percent = 0.0f;
    float pressure_hpa = 0.0f;

    bool valid = false;
};
