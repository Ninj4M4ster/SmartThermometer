#pragma once

#include "esp_err.h"
#include "esp_matter.h"

#include <cstdint>

struct MatterDiagnosticStatus {
    bool commissioned;
    bool subscription_active;
    bool sensor_data_ready;
};

class MatterManager {
public:
    esp_err_t init();

    esp_err_t update_sensor_values(
        float temperature_celsius,
        float humidity_percent);

    MatterDiagnosticStatus get_diagnostic_status() const;

    uint16_t get_temperature_endpoint_id() const;
    uint16_t get_humidity_endpoint_id() const;

private:
    esp_err_t init_nvs();
    esp_err_t create_node();
    esp_err_t create_sensor_endpoint();

    uint16_t temperature_endpoint_id_ = 0;
    uint16_t humidity_endpoint_id_ = 0;

    bool has_published_values_ = false;

    int16_t last_temperature_ = 0;
    uint16_t last_humidity_ = 0;

    esp_matter::node_t *node_ = nullptr;
};
