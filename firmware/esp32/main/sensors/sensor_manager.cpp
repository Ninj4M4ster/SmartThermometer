#include "sensor_manager.hpp"

esp_err_t SensorManager::init()
{
    update_queue_ = xQueueCreate(1, sizeof(SensorData));

    return update_queue_ ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t SensorManager::update_data(const SensorData &data)
{
    if (!update_queue_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueOverwrite(update_queue_, &data) != pdPASS) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t SensorManager::wait_for_update(
    SensorData &data,
    TickType_t timeout
)
{
    if (!update_queue_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueReceive(update_queue_, &data, timeout) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}