#include "app.hpp"

#include "esp_log.h"

static const char *TAG = "MAIN";

static App app;

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(app.init());

    ESP_LOGI(TAG, "Application started");
}