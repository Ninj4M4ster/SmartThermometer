#include "led_manager.hpp"

#include "driver/gpio.h"
#include "led_strip.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#define LED_STRIP_GPIO GPIO_NUM_8

static const char *TAG = "LED";

esp_err_t LedManager::init()
{
    const led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = false},
    };

    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {.with_dma = false},
    };

    led_strip_handle_t strip = nullptr;

    esp_err_t err = led_strip_new_rmt_device(
        &strip_config,
        &rmt_config,
        &strip
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED initialization failed");
        return err;
    }

    strip_handle_ = strip;

    led_strip_clear(strip);
    led_strip_refresh(strip);

    if (xTaskCreate(
            task_entry,
            "led_task",
            3072,
            this,
            2,
            nullptr
        ) != pdPASS) {

        ESP_LOGE(TAG, "LED task creation failed");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void LedManager::task_entry(void *arg)
{
    auto *self = static_cast<LedManager *>(arg);
    self->task();
}

void LedManager::task()
{
    auto strip = static_cast<led_strip_handle_t>(strip_handle_);

    bool led_on = false;

    while (true) {
        led_on = !led_on;

        if (led_on) {
            led_strip_set_pixel(strip, 0, 0, 32, 0);
        } else {
            led_strip_clear(strip);
        }

        led_strip_refresh(strip);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}