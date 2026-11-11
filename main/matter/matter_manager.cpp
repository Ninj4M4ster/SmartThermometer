#include "matter_manager.hpp"

#include <cmath>
#include <limits>
#include <atomic>

#include "esp_log.h"
#include "esp_matter.h"
#include "esp_matter_endpoint.h"
#include "esp_matter_data_model_provider.h"

#include "nvs_flash.h"
#include <app/InteractionModelEngine.h>
#include <app/ReadHandler.h>
#include <app/server/Server.h>

// Matter cluster implementations
#include <app/clusters/temperature-measurement-server/TemperatureMeasurementCluster.h>

#include <app/clusters/relative-humidity-measurement-server/RelativeHumidityMeasurementCluster.h>

#include <app/ConcreteAttributePath.h>

#include <app/data-model-provider/MetadataTypes.h>

using namespace esp_matter;

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

static const char *TAG = "MATTER";

MatterManager matter_manager;

namespace {

std::atomic<uint32_t> active_subscription_count{0};

class SubscriptionStatusCallback final
    : public chip::app::ReadHandler::ApplicationCallback {
public:
    void OnSubscriptionEstablished(chip::app::ReadHandler &) override
    {
        active_subscription_count.fetch_add(1, std::memory_order_relaxed);
    }

    void OnSubscriptionTerminated(chip::app::ReadHandler &) override
    {
        uint32_t count = active_subscription_count.load(std::memory_order_relaxed);
        while (count > 0 &&
               !active_subscription_count.compare_exchange_weak(
                   count,
                   count - 1,
                   std::memory_order_relaxed
               )) {
        }
    }
};

SubscriptionStatusCallback subscription_status_callback;

} // namespace


// --------------------------------------------------
// NVS initialization
// --------------------------------------------------

esp_err_t MatterManager::init_nvs()
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());

        err = nvs_flash_init();
    }

    return err;
}


// --------------------------------------------------
// Matter initialization
// --------------------------------------------------

esp_err_t MatterManager::init()
{
    esp_err_t err = init_nvs();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed");
        return err;
    }

    err = create_node();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter node creation failed");
        return err;
    }

    err = esp_matter::start(nullptr);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter start failed: %s",
                 esp_err_to_name(err));

        return err;
    }

    chip::app::InteractionModelEngine::GetInstance()
        ->RegisterReadHandlerAppCallback(&subscription_status_callback);

    ESP_LOGI(TAG, "Matter started successfully");

    return ESP_OK;
}


// --------------------------------------------------
// Create Matter node
// --------------------------------------------------

esp_err_t MatterManager::create_node()
{
    node::config_t node_config{};

    node_ = node::create(
        &node_config,
        nullptr,
        nullptr,
        nullptr
    );

    if (!node_) {
        ESP_LOGE(TAG, "Node creation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Matter node created");

    return create_sensor_endpoint();
}


// --------------------------------------------------
// Create environmental sensor endpoint
// --------------------------------------------------

esp_err_t MatterManager::create_sensor_endpoint()
{
    // ----------------------------------------------
    // Temperature Sensor endpoint
    // Device type: 0x0302
    // Cluster:     0x0402
    // ----------------------------------------------

    endpoint::temperature_sensor::config_t temperature_config{};

    endpoint_t *temperature_endpoint =
        endpoint::temperature_sensor::create(
            node_,
            &temperature_config,
            ENDPOINT_FLAG_NONE,
            nullptr
        );

    if (!temperature_endpoint) {
        ESP_LOGE(TAG, "Temperature endpoint creation failed");
        return ESP_FAIL;
    }

    temperature_endpoint_id_ = endpoint::get_id(temperature_endpoint);


    // ----------------------------------------------
    // Humidity Sensor endpoint
    // Device type: 0x0307
    // Cluster:     0x0405
    // ----------------------------------------------

    endpoint::humidity_sensor::config_t humidity_config{};

    endpoint_t *humidity_endpoint =
        endpoint::humidity_sensor::create(
            node_,
            &humidity_config,
            ENDPOINT_FLAG_NONE,
            nullptr
        );

    if (!humidity_endpoint) {
        ESP_LOGE(TAG, "Humidity endpoint creation failed");
        return ESP_FAIL;
    }

    humidity_endpoint_id_ = endpoint::get_id(humidity_endpoint);


    ESP_LOGI(
        TAG,
        "Temperature and humidity endpoints created: T=%u, H=%u",
        temperature_endpoint_id_,
        humidity_endpoint_id_
    );

    return ESP_OK;
}


// --------------------------------------------------
// Update Matter sensor values
// --------------------------------------------------

esp_err_t MatterManager::update_sensor_values(
    float temperature_celsius,
    float humidity_percent)
{
    if (temperature_endpoint_id_ == 0 ||
        humidity_endpoint_id_ == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    const float temperature_scaled = temperature_celsius * 100.0f;
    const float humidity_scaled = humidity_percent * 100.0f;

    if (!std::isfinite(temperature_scaled) ||
        !std::isfinite(humidity_scaled)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (temperature_scaled < -32767.0f ||
        temperature_scaled > 32767.0f ||
        humidity_scaled < 0.0f ||
        humidity_scaled > 10000.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    const int16_t temperature =
        static_cast<int16_t>(std::lround(temperature_scaled));

    const uint16_t humidity =
        static_cast<uint16_t>(std::lround(humidity_scaled));

    // Skip unchanged values.
    if (has_published_values_ &&
        temperature == last_temperature_ &&
        humidity == last_humidity_) {
        return ESP_OK;
    }

    // Matter data-model operations require the CHIP stack lock.
    lock::ScopedChipStackLock chip_lock(portMAX_DELAY);

    auto &registry =
        data_model::provider::get_instance().registry();

    // ----------------------------------------------
    // Find Temperature Measurement cluster
    // ----------------------------------------------

    auto *temperature_cluster =
        static_cast<TemperatureMeasurementCluster *>(
            registry.Get(
                ConcreteClusterPath(
                    temperature_endpoint_id_,
                    TemperatureMeasurement::Id
                )
            )
        );

    if (!temperature_cluster) {
        ESP_LOGE(TAG, "Temperature cluster not found");
        return ESP_FAIL;
    }

    // ----------------------------------------------
    // Find Relative Humidity Measurement cluster
    // ----------------------------------------------

    auto *humidity_cluster =
        static_cast<RelativeHumidityMeasurementCluster *>(
            registry.Get(
                ConcreteClusterPath(
                    humidity_endpoint_id_,
                    RelativeHumidityMeasurement::Id
                )
            )
        );

    if (!humidity_cluster) {
        ESP_LOGE(TAG, "Humidity cluster not found");
        return ESP_FAIL;
    }

    // ----------------------------------------------
    // Publish measurements
    // ----------------------------------------------

    CHIP_ERROR chip_err;

    chip_err = temperature_cluster->SetMeasuredValue(
        DataModel::Nullable<int16_t>(temperature)
    );

    if (chip_err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Temperature update failed: %" CHIP_ERROR_FORMAT,
                 chip_err.Format());
        return ESP_FAIL;
    }

    chip_err = humidity_cluster->SetMeasuredValue(
        DataModel::Nullable<uint16_t>(humidity)
    );

    if (chip_err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Humidity update failed: %" CHIP_ERROR_FORMAT,
                 chip_err.Format());
        return ESP_FAIL;
    }

    // Cache only after both updates succeed.
    last_temperature_ = temperature;
    last_humidity_ = humidity;
    has_published_values_ = true;

    ESP_LOGI(
        TAG,
        "Published HTU31 T=%.2f C, H=%.2f %%",
        temperature_celsius,
        humidity_percent
    );

    return ESP_OK;
}

MatterDiagnosticStatus MatterManager::get_diagnostic_status() const
{
    lock::ScopedChipStackLock chip_lock(portMAX_DELAY);

    return {
        chip::Server::GetInstance().GetFabricTable().FabricCount() > 0,
        active_subscription_count.load(std::memory_order_relaxed) > 0,
        has_published_values_
    };
}


// --------------------------------------------------
// Get endpoint ID
// --------------------------------------------------

uint16_t MatterManager::get_temperature_endpoint_id() const
{
    return temperature_endpoint_id_;
}

uint16_t MatterManager::get_humidity_endpoint_id() const
{
    return humidity_endpoint_id_;
}
