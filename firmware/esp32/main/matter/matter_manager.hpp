#pragma once

#include "esp_err.h"
#include "esp_matter.h"
#include "sensor_data.hpp"

#include <cstdint>

class MatterManager {
public:
    esp_err_t init();

    esp_err_t update_sensor_values(const SensorData &data);

    uint16_t get_endpoint_id() const;

private:
    esp_err_t init_nvs();
    esp_err_t create_node();
    esp_err_t create_sensor_endpoint();

    uint16_t sensor_endpoint_id_ = 0;

    bool has_published_values_ = false;
    
    int16_t last_temperature_ = 0;
    uint16_t last_humidity_ = 0;
    int16_t last_pressure_ = 0;

    esp_matter::node_t *node_ = nullptr;
};
