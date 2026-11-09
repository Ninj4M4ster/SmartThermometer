#pragma once

#include "esp_err.h"

#include "led_manager.hpp"
#include "sensor_manager.hpp"
#include "matter_manager.hpp"

class App {
public:
    esp_err_t init();

private:
    static void sensor_publish_task(void *arg);

    LedManager led_manager_;
    SensorManager sensor_manager_;
    MatterManager matter_manager_;
};
