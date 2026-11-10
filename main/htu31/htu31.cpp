
#include "htu31.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define HTU31_CMD_CONVERSION   0x5E
#define HTU31_CMD_READ_T_RH    0x00

#define HTU31_TIMEOUT_MS       100

static uint8_t htu31_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }

    return crc;
}

esp_err_t HTU31_Init(i2c_master_bus_handle_t bus)
{
    if (bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_probe(bus, HTU31_I2C_ADDRESS, HTU31_TIMEOUT_MS);
}

esp_err_t HTU31_Read(
    i2c_master_dev_handle_t device,
    float *temperature_celsius,
    float *humidity_percent)
{
    if (device == NULL ||
        temperature_celsius == NULL ||
        humidity_percent == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t command = HTU31_CMD_CONVERSION;
    uint8_t rx_data[6];

    esp_err_t err = i2c_master_transmit(
        device,
        &command,
        sizeof(command),
        HTU31_TIMEOUT_MS
    );
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(25));

    command = HTU31_CMD_READ_T_RH;
    err = i2c_master_transmit(
        device,
        &command,
        sizeof(command),
        HTU31_TIMEOUT_MS
    );
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_master_receive(
        device,
        rx_data,
        sizeof(rx_data),
        HTU31_TIMEOUT_MS
    );
    if (err != ESP_OK) {
        return err;
    }

    if (htu31_crc8(&rx_data[0], 2) != rx_data[2] ||
        htu31_crc8(&rx_data[3], 2) != rx_data[5]) {
        return ESP_FAIL;
    }

    const uint16_t raw_temp =
        ((uint16_t)rx_data[0] << 8) | rx_data[1];
    const uint16_t raw_humidity =
        ((uint16_t)rx_data[3] << 8) | rx_data[4];

    *temperature_celsius =
        -40.0f + 165.0f * ((float)raw_temp / 65536.0f);
    *humidity_percent =
        100.0f * ((float)raw_humidity / 65536.0f);

    return ESP_OK;
}
