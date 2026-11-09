#include "app.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

esp_err_t App::init()
{
    esp_err_t err;

    err = sensor_manager_.init();
    if (err != ESP_OK) {
        return err;
    }

    err = led_manager_.init();
    if (err != ESP_OK) {
        return err;
    }

    err = matter_manager_.init();
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(
            sensor_publish_task,
            "sensor_publish",
            4096,
            this,
            4,
            nullptr
        ) != pdPASS) {

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void App::sensor_publish_task(void *arg)
{
    auto *self = static_cast<App *>(arg);

    SensorData data;

    while (true) {

        // Block until new measurements arrive
        if (self->sensor_manager_.wait_for_update(data) == ESP_OK) {

            if (!data.valid) {
                continue;
            }

            self->matter_manager_.update_sensor_values(data);
        }
    }
}
