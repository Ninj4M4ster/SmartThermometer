#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "led_strip.h"

#define LED_STRIP_GPIO GPIO_NUM_8

extern "C" esp_err_t matter_init(void);

static const char *TAG = "APP";

QueueHandle_t g_led_queue = nullptr;
static led_strip_handle_t g_led_strip = nullptr;

// Task responsible for physical LED control
static void led_task(void *arg)
{
    bool led_on = false;

    while (true) {
        if (xQueueReceive(g_led_queue, &led_on, portMAX_DELAY) == pdTRUE) {

            if (led_on) {
                ESP_LOGI(TAG, "LED ON");

                ESP_ERROR_CHECK(
                    led_strip_set_pixel(g_led_strip, 0, 0, 32, 0)
                );
                ESP_ERROR_CHECK(led_strip_refresh(g_led_strip));

            } else {
                ESP_LOGI(TAG, "LED OFF");

                ESP_ERROR_CHECK(led_strip_clear(g_led_strip));
                ESP_ERROR_CHECK(led_strip_refresh(g_led_strip));
            }
        }
    }
}

extern "C" void app_main(void)
{
    // Create queue for Matter -> LED communication
    g_led_queue = xQueueCreate(4, sizeof(bool));
    ESP_ERROR_CHECK(g_led_queue ? ESP_OK : ESP_ERR_NO_MEM);

    // Initialize WS2812
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

    ESP_ERROR_CHECK(
        led_strip_new_rmt_device(
            &strip_config,
            &rmt_config,
            &g_led_strip
        )
    );

    ESP_ERROR_CHECK(led_strip_clear(g_led_strip));
    ESP_ERROR_CHECK(led_strip_refresh(g_led_strip));

    // Start LED task
    xTaskCreate(
        led_task,
        "led_task",
        3072,
        nullptr,
        5,
        nullptr
    );

    // Start Matter
    ESP_ERROR_CHECK(matter_init());

    ESP_LOGI(TAG, "Application started");
}