#pragma once

#include "esp_err.h"
#include "sensor_data.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class SensorManager {
public:
    SensorManager() = default;

    esp_err_t init();

    esp_err_t update_data(const SensorData &data);

    esp_err_t wait_for_update(
        SensorData &data,
        TickType_t timeout = portMAX_DELAY
    );

private:
    QueueHandle_t update_queue_ = nullptr;
};
