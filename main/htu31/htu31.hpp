#ifndef HTU31_H
#define HTU31_H

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <stdint.h>

#define HTU31_I2C_ADDRESS 0x40

esp_err_t HTU31_Init(i2c_master_bus_handle_t bus);
esp_err_t HTU31_Read(
    i2c_master_dev_handle_t device,
    float *temperature_celsius,
    float *humidity_percent);

#endif
