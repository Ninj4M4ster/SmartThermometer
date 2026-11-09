#include "matter_manager.hpp"

#include <cmath>
#include <limits>

#include "esp_log.h"
#include "esp_matter.h"
#include "esp_matter_endpoint.h"
#include "esp_matter_data_model_provider.h"

#include "nvs_flash.h"

// Matter cluster implementations
#include <app/clusters/temperature-measurement-server/TemperatureMeasurementCluster.h>

#include <app/clusters/relative-humidity-measurement-server/RelativeHumidityMeasurementCluster.h>

#include <app/clusters/pressure-measurement-server/PressureMeasurementCluster.h>

#include <app/ConcreteAttributePath.h>

#include <app/data-model-provider/MetadataTypes.h>

using namespace esp_matter;

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

static const char *TAG = "MATTER";

MatterManager matter_manager;


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
    endpoint::temperature_sensor::config_t config{};

    endpoint_t *endpoint =
        endpoint::temperature_sensor::create(
            node_,
            &config,
            ENDPOINT_FLAG_NONE,
            nullptr
        );

    if (!endpoint) {
        ESP_LOGE(TAG, "Sensor endpoint creation failed");
        return ESP_FAIL;
    }

    sensor_endpoint_id_ = endpoint::get_id(endpoint);

    ESP_LOGI(
        TAG,
        "Temperature sensor endpoint created: %u",
        sensor_endpoint_id_
    );


    // ----------------------------------------------
    // Relative Humidity Measurement cluster
    // ----------------------------------------------

    cluster::relative_humidity_measurement::config_t humidity_config{};

    cluster_t *humidity_cluster =
        cluster::relative_humidity_measurement::create(
            endpoint,
            &humidity_config,
            CLUSTER_FLAG_SERVER
        );

    if (!humidity_cluster) {
        ESP_LOGE(TAG, "Humidity cluster creation failed");
        return ESP_FAIL;
    }


    // ----------------------------------------------
    // Pressure Measurement cluster
    // ----------------------------------------------

    cluster::pressure_measurement::config_t pressure_config{};

    cluster_t *pressure_cluster =
        cluster::pressure_measurement::create(
            endpoint,
            &pressure_config,
            CLUSTER_FLAG_SERVER
        );

    if (!pressure_cluster) {
        ESP_LOGE(TAG, "Pressure cluster creation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Environmental clusters created");

    return ESP_OK;
}


// --------------------------------------------------
// Update Matter sensor values
// --------------------------------------------------

esp_err_t MatterManager::update_sensor_values(
    const SensorData &data)
{
    if (sensor_endpoint_id_ == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data.valid) {

        // Temperature: 0.01 degrees Celsius
        const float temperature_scaled =
            data.temperature_celsius * 100.0f;

        // Humidity: 0.01 percent
        const float humidity_scaled =
            data.humidity_percent * 100.0f;

        // Pressure Measurement uses kPa.
        // Convert hPa to kPa.
        const float pressure_scaled =
            data.pressure_hpa / 10.0f;

        if (!std::isfinite(temperature_scaled) ||
            !std::isfinite(humidity_scaled) ||
            !std::isfinite(pressure_scaled)) {
            return ESP_ERR_INVALID_ARG;
        }

        if (temperature_scaled < -32767.0f ||
            temperature_scaled > 32767.0f ||
            humidity_scaled < 0.0f ||
            humidity_scaled > 10000.0f ||
            pressure_scaled < -32767.0f ||
            pressure_scaled > 32767.0f) {
            return ESP_ERR_INVALID_ARG;
        }

        const int16_t temperature =
            static_cast<int16_t>(std::lround(temperature_scaled));

        const uint16_t humidity =
            static_cast<uint16_t>(std::lround(humidity_scaled));

        const int16_t pressure =
            static_cast<int16_t>(std::lround(pressure_scaled));


        // Avoid unnecessary Matter reports.
        if (has_published_values_ &&
            temperature == last_temperature_ &&
            humidity == last_humidity_ &&
            pressure == last_pressure_) {
            return ESP_OK;
        }

        // All Matter data-model operations must execute
        // with the CHIP stack lock held.
        lock::ScopedChipStackLock chip_lock(portMAX_DELAY);


        // ------------------------------------------
        // Find Temperature Measurement cluster
        // ------------------------------------------

        auto *temperature_cluster =
            static_cast<TemperatureMeasurementCluster *>(
                data_model::provider::get_instance()
                    .registry()
                    .Get(
                        ConcreteClusterPath(
                            sensor_endpoint_id_,
                            TemperatureMeasurement::Id
                        )
                    )
            );

        if (!temperature_cluster) {
            ESP_LOGE(TAG, "Temperature cluster not found");
            return ESP_FAIL;
        }


        // ------------------------------------------
        // Find Relative Humidity Measurement cluster
        // ------------------------------------------

        auto *humidity_cluster =
            static_cast<RelativeHumidityMeasurementCluster *>(
                data_model::provider::get_instance()
                    .registry()
                    .Get(
                        ConcreteClusterPath(
                            sensor_endpoint_id_,
                            RelativeHumidityMeasurement::Id
                        )
                    )
            );

        if (!humidity_cluster) {
            ESP_LOGE(TAG, "Humidity cluster not found");
            return ESP_FAIL;
        }


        // ------------------------------------------
        // Find Pressure Measurement cluster
        // ------------------------------------------

        auto *pressure_cluster =
            static_cast<PressureMeasurementCluster *>(
                data_model::provider::get_instance()
                    .registry()
                    .Get(
                        ConcreteClusterPath(
                            sensor_endpoint_id_,
                            PressureMeasurement::Id
                        )
                    )
            );

        if (!pressure_cluster) {
            ESP_LOGE(TAG, "Pressure cluster not found");
            return ESP_FAIL;
        }


        // ------------------------------------------
        // Publish measurements
        // ------------------------------------------

        CHIP_ERROR chip_err;

        chip_err = temperature_cluster->SetMeasuredValue(
            DataModel::Nullable<int16_t>(temperature)
        );

        if (chip_err != CHIP_NO_ERROR) {
            ESP_LOGE(TAG, "Temperature update failed");
            return ESP_FAIL;
        }


        chip_err = humidity_cluster->SetMeasuredValue(
            DataModel::Nullable<uint16_t>(humidity)
        );

        if (chip_err != CHIP_NO_ERROR) {
            ESP_LOGE(TAG, "Humidity update failed");
            return ESP_FAIL;
        }


        chip_err = pressure_cluster->SetMeasuredValue(
            DataModel::Nullable<int16_t>(pressure)
        );

        if (chip_err != CHIP_NO_ERROR) {
            ESP_LOGE(TAG, "Pressure update failed");
            return ESP_FAIL;
        }


        // Cache last published values.
        last_temperature_ = temperature;
        last_humidity_ = humidity;
        last_pressure_ = pressure;

        has_published_values_ = true;

        ESP_LOGI(
            TAG,
            "Published T=%.2f C, H=%.2f %%, P=%.1f hPa",
            data.temperature_celsius,
            data.humidity_percent,
            data.pressure_hpa
        );

        return ESP_OK;
    }

    return ESP_OK;
}


// --------------------------------------------------
// Get endpoint ID
// --------------------------------------------------

uint16_t MatterManager::get_endpoint_id() const
{
    return sensor_endpoint_id_;
}