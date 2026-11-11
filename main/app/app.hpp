#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include "led_manager.hpp"
#include "matter_manager.hpp"

class App {
public:
    esp_err_t init();

private:
    static void sensor_display_task(void *arg);
    esp_err_t init_i2c_devices();

    LedManager led_manager_;
    MatterManager matter_manager_;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_dev_handle_t htu31_device_ = nullptr;
    i2c_master_dev_handle_t sh1106_device_ = nullptr;
    bool htu31_available_ = false;
    bool sh1106_available_ = false;
    TickType_t welcome_screen_started_ = 0;
};
