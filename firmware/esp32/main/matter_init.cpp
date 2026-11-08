#include "esp_err.h"
#include "esp_log.h"
#include "esp_matter.h"
#include "esp_matter_endpoint.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t g_led_queue;

static const char *TAG = "MATTER";

// Matter attribute callback
static esp_err_t app_attribute_update_cb(
    esp_matter::attribute::callback_type_t type,
    uint16_t endpoint_id,
    uint32_t cluster_id,
    uint32_t attribute_id,
    esp_matter_attr_val_t *val,
    void *priv_data
)
{
    // OnOff cluster = 0x0006
    // OnOff attribute = 0x0000

    if (type == esp_matter::attribute::PRE_UPDATE &&
        cluster_id == 0x0006 &&
        attribute_id == 0x0000) {

        bool led_on = val->val.b;

        ESP_LOGI(
            TAG,
            "Matter command: endpoint=%u LED=%s",
            endpoint_id,
            led_on ? "ON" : "OFF"
        );

        if (xQueueSend(g_led_queue, &led_on, 0) != pdTRUE) {
            ESP_LOGE(TAG, "LED queue full");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

extern "C" esp_err_t matter_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        return err;
    }

    // Create Matter node
    esp_matter::node::config_t node_config{};

    esp_matter::node_t *node =
        esp_matter::node::create(
            &node_config,
            app_attribute_update_cb,
            nullptr,
            nullptr
        );

    if (node == nullptr) {
        ESP_LOGE(TAG, "Node creation failed");
        return ESP_FAIL;
    }

    // Create On/Off Light endpoint
    esp_matter::endpoint::on_off_light::config_t light_config{};

    esp_matter::endpoint_t *endpoint =
        esp_matter::endpoint::on_off_light::create(
            node,
            &light_config,
            esp_matter::ENDPOINT_FLAG_NONE,
            nullptr
        );

    if (endpoint == nullptr) {
        ESP_LOGE(TAG, "Endpoint creation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Matter endpoint created");

    // Start Matter
    err = esp_matter::start(nullptr);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Matter started");

    return ESP_OK;
}