#include "app.hpp"

#include "htu31.hpp"
#include "sh1106.hpp"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

static const char *TAG = "APP";

static constexpr gpio_num_t I2C_SDA_GPIO = GPIO_NUM_6;
static constexpr gpio_num_t I2C_SCL_GPIO = GPIO_NUM_7;
static constexpr uint32_t I2C_FREQUENCY_HZ = 100000;

esp_err_t App::init()
{
    esp_err_t err;

    err = led_manager_.init();
    if (err != ESP_OK) {
        return err;
    }

    err = matter_manager_.init();
    if (err != ESP_OK) {
        return err;
    }

    err = init_i2c_devices();
    if (err != ESP_OK) {
        return err;
    }

    if (htu31_available_ && xTaskCreate(
                               sensor_display_task,
                               "sensor_display",
                               4096,
                               this,
                               4,
                               nullptr
                           ) != pdPASS) {

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t App::init_i2c_devices()
{
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = I2C_SDA_GPIO;
    bus_config.scl_io_num = I2C_SCL_GPIO;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &i2c_bus_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t htu31_config = {};
    htu31_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    htu31_config.device_address = HTU31_I2C_ADDRESS;
    htu31_config.scl_speed_hz = I2C_FREQUENCY_HZ;

    err = i2c_master_bus_add_device(
        i2c_bus_,
        &htu31_config,
        &htu31_device_
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTU31 I2C device setup failed: %s", esp_err_to_name(err));
        return err;
    }

    err = HTU31_Init(i2c_bus_);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "HTU31 not found on I2C bus; starting without sensor");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTU31 not found on I2C bus: %s", esp_err_to_name(err));
        return err;
    } else {
        htu31_available_ = true;
    }

    i2c_device_config_t sh1106_config = {};
    sh1106_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    sh1106_config.device_address = SH1106_I2C_ADDRESS;
    sh1106_config.scl_speed_hz = I2C_FREQUENCY_HZ;

    err = i2c_master_bus_add_device(
        i2c_bus_,
        &sh1106_config,
        &sh1106_device_
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SH1106 I2C device setup failed: %s", esp_err_to_name(err));
        return err;
    }

    err = SH1106_Init(sh1106_device_);
    if (err == ESP_ERR_INVALID_RESPONSE) {
        ESP_LOGW(TAG, "SH1106 not responding on I2C bus; starting without display");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "SH1106 initialization failed: %s", esp_err_to_name(err));
        return err;
    } else {
        sh1106_available_ = true;
    }

    ESP_LOGI(
        TAG,
        "I2C initialized; HTU31 %s, SH1106 %s",
        htu31_available_ ? "available" : "unavailable",
        sh1106_available_ ? "available" : "unavailable"
    );
    return ESP_OK;
}

void App::sensor_display_task(void *arg)
{
    auto *self = static_cast<App *>(arg);
    TickType_t last_wake_time = xTaskGetTickCount();

    while (true) {
        float temperature_celsius;
        float humidity_percent;

        esp_err_t err = HTU31_Read(
            self->htu31_device_,
            &temperature_celsius,
            &humidity_percent
        );
        if (err == ESP_OK) {
            const esp_err_t matter_err =
                self->matter_manager_.update_sensor_values(
                    temperature_celsius,
                    humidity_percent
                );
            if (matter_err != ESP_OK) {
                ESP_LOGE(
                    TAG,
                    "Matter sensor update failed: %s",
                    esp_err_to_name(matter_err)
                );
            }

            if (self->sh1106_available_) {
                const esp_err_t display_err = SH1106_PrintMeasurements(
                    self->sh1106_device_,
                    temperature_celsius,
                    humidity_percent
                );
                if (display_err != ESP_OK) {
                    ESP_LOGE(
                        TAG,
                        "OLED update failed: %s",
                        esp_err_to_name(display_err)
                    );
                }
            }
            ESP_LOGI(
                TAG,
                "HTU31: %.2f C, %.2f%% RH",
                (double)temperature_celsius,
                (double)humidity_percent
            );
        } else {
            ESP_LOGE(TAG, "HTU31 read failed: %s", esp_err_to_name(err));
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
    }
}
