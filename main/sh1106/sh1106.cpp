#include "sh1106.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct
{
    uint8_t command;
    uint8_t parameter;
    uint8_t has_parameter;
} SH1106_InitCommand;

static const SH1106_InitCommand init_sequence[] =
{
    { SH1106_CMD_DISPLAY_OFF,          0, false },

    { SH1106_CMD_SET_CLOCK_DIVIDE,     SH1106_CLOCK_DIVIDE_DEFAULT,   true },
    { SH1106_CMD_SET_MULTIPLEX,        SH1106_MULTIPLEX_64_ROWS,      true },
    { SH1106_CMD_SET_DISPLAY_OFFSET,   SH1106_DISPLAY_OFFSET_DEFAULT, true },
    { SH1106_CMD_SET_START_LINE,       0, false },
    { SH1106_CMD_SET_DC_DC,            SH1106_DC_DC_ENABLE,           true },

    { SH1106_CMD_SEGMENT_REMAP,        0, false },
    { SH1106_CMD_COM_SCAN_REMAP,       0, false },

    { SH1106_CMD_SET_COM_PINS,         SH1106_COM_PINS_128X64,        true },
    { SH1106_CMD_SET_CONTRAST,         SH1106_CONTRAST_DEFAULT,       true },
    { SH1106_CMD_SET_PRECHARGE,        SH1106_PRECHARGE_DEFAULT,      true },
    { SH1106_CMD_SET_VCOM_DESELECT,    SH1106_VCOM_DESELECT_DEFAULT,  true },

    { SH1106_CMD_DISPLAY_RAM,          0, false },
    { SH1106_CMD_DISPLAY_NORMAL,       0, false },
};

static uint8_t framebuffer[SH1106_WIDTH * SH1106_HEIGHT / 8];

static esp_err_t write_command(
    i2c_master_dev_handle_t device,
    uint8_t command)
{
    uint8_t packet[2] = {
        SH1106_I2C_CONTROL_COMMAND,
        command
    };

    return i2c_master_transmit(
        device,
        packet,
        sizeof(packet),
        SH1106_I2C_TIMEOUT_MS
    );
}

static esp_err_t write_command_with_param(
    i2c_master_dev_handle_t device,
    uint8_t command,
    uint8_t parameter)
{
    esp_err_t status = write_command(device, command);

    if (status != ESP_OK)
        return status;

    return write_command(device, parameter);
}

static esp_err_t set_page(
    i2c_master_dev_handle_t device,
    uint8_t page)
{
    if (page >= SH1106_PAGE_COUNT)
        return ESP_ERR_INVALID_ARG;

    return write_command(
        device,
        SH1106_CMD_SET_PAGE_ADDRESS |
        (page & SH1106_PAGE_ADDRESS_MASK)
    );
}

static esp_err_t set_column(
    i2c_master_dev_handle_t device,
    uint8_t column)
{
    uint16_t address = column + SH1106_COLUMN_OFFSET;

    if (column >= SH1106_WIDTH || address >= 132U)
        return ESP_ERR_INVALID_ARG;

    esp_err_t status = write_command(
        device,
        SH1106_CMD_SET_LOWER_COLUMN |
        (address & SH1106_COLUMN_NIBBLE_MASK)
    );

    if (status != ESP_OK)
        return status;

    return write_command(
        device,
        SH1106_CMD_SET_HIGHER_COLUMN |
        ((address >> 4) & SH1106_COLUMN_HIGH_MASK)
    );
}

esp_err_t SH1106_Init(i2c_master_dev_handle_t device)
{
    if (device == NULL)
        return ESP_ERR_INVALID_ARG;

    vTaskDelay(pdMS_TO_TICKS(100));

    for (uint32_t i = 0;
         i < sizeof(init_sequence) / sizeof(init_sequence[0]);
         i++)
    {
        const SH1106_InitCommand *cmd = &init_sequence[i];

        esp_err_t status;

        if (cmd->has_parameter)
        {
            status = write_command_with_param(
                device,
                cmd->command,
                cmd->parameter
            );
        }
        else
        {
            status = write_command(device, cmd->command);
        }

        if (status != ESP_OK)
            return status;
    }

    memset(framebuffer, 0, sizeof(framebuffer));

    esp_err_t status = SH1106_Update(device);

    if (status != ESP_OK)
        return status;

    return write_command(device, SH1106_CMD_DISPLAY_ON);
}


esp_err_t SH1106_Update(i2c_master_dev_handle_t device)
{
    if (device == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t packet[1U + SH1106_WIDTH];

    for (uint8_t page = 0; page < SH1106_PAGE_COUNT; page++)
    {
        esp_err_t status = set_page(device, page);
        if (status != ESP_OK)
            return status;

        status = set_column(device, 0U);
        if (status != ESP_OK)
            return status;

        packet[0] = SH1106_I2C_CONTROL_DATA;

        memcpy(
            &packet[1],
            &framebuffer[page * SH1106_WIDTH],
            SH1106_WIDTH
        );

        status = i2c_master_transmit(
            device,
            packet,
            sizeof(packet),
            SH1106_I2C_TIMEOUT_MS
        );

        if (status != ESP_OK)
            return status;
    }

    return ESP_OK;
}

esp_err_t SH1106_Clear(i2c_master_dev_handle_t device)
{
    memset(framebuffer, 0, sizeof(framebuffer));
    return SH1106_Update(device);
}

void SH1106_Fill(uint8_t pattern)
{
    memset(framebuffer, pattern, sizeof(framebuffer));
}

static void get_glyph(char c, uint8_t glyph[5])
{
    static const uint8_t digits[10][5] =
    {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
    };

    static const uint8_t letters[26][5] =
    {
        {0x7E,0x11,0x11,0x11,0x7E}, // A
        {0x7F,0x49,0x49,0x49,0x36}, // B
        {0x3E,0x41,0x41,0x41,0x22}, // C
        {0x7F,0x41,0x41,0x22,0x1C}, // D
        {0x7F,0x49,0x49,0x49,0x41}, // E
        {0x7F,0x09,0x09,0x09,0x01}, // F
        {0x3E,0x41,0x49,0x49,0x7A}, // G
        {0x7F,0x08,0x08,0x08,0x7F}, // H
        {0x00,0x41,0x7F,0x41,0x00}, // I
        {0x20,0x40,0x41,0x3F,0x01}, // J
        {0x7F,0x08,0x14,0x22,0x41}, // K
        {0x7F,0x40,0x40,0x40,0x40}, // L
        {0x7F,0x02,0x0C,0x02,0x7F}, // M
        {0x7F,0x04,0x08,0x10,0x7F}, // N
        {0x3E,0x41,0x41,0x41,0x3E}, // O
        {0x7F,0x09,0x09,0x09,0x06}, // P
        {0x3E,0x41,0x51,0x21,0x5E}, // Q
        {0x7F,0x09,0x19,0x29,0x46}, // R
        {0x46,0x49,0x49,0x49,0x31}, // S
        {0x01,0x01,0x7F,0x01,0x01}, // T
        {0x3F,0x40,0x40,0x40,0x3F}, // U
        {0x1F,0x20,0x40,0x20,0x1F}, // V
        {0x3F,0x40,0x38,0x40,0x3F}, // W
        {0x63,0x14,0x08,0x14,0x63}, // X
        {0x07,0x08,0x70,0x08,0x07}, // Y
        {0x61,0x51,0x49,0x45,0x43}  // Z
    };

    memset(glyph, 0, 5);

    if (c >= '0' && c <= '9')
    {
        memcpy(glyph, digits[c - '0'], 5);
    }
    else if (c >= 'A' && c <= 'Z')
    {
        memcpy(glyph, letters[c - 'A'], 5);
    }
    else
    {
        switch (c)
        {
            case ':':
                glyph[1] = 0x36;
                break;

            case '.':
                glyph[2] = 0x60;
                break;

            case '-':
                glyph[1] = 0x08;
                glyph[2] = 0x08;
                glyph[3] = 0x08;
                break;

            case '/':
                glyph[0] = 0x60;
                glyph[1] = 0x18;
                glyph[2] = 0x06;
                break;

            case '%':
                glyph[0] = 0x63;
                glyph[1] = 0x13;
                glyph[2] = 0x08;
                glyph[3] = 0x64;
                glyph[4] = 0x63;
                break;

            default:
                break;
        }
    }
}

void SH1106_WriteChar(uint8_t x, uint8_t page, char c)
{
    if (x > SH1106_WIDTH - 6U ||
        page >= SH1106_PAGE_COUNT)
        return;

    uint8_t glyph[5];

    get_glyph(c, glyph);

    uint32_t offset = page * SH1106_WIDTH + x;

    for (uint8_t i = 0; i < 5; i++)
    {
        framebuffer[offset + i] = glyph[i];
    }

    framebuffer[offset + 5] = 0x00;
}

void SH1106_WriteString(
    uint8_t x,
    uint8_t page,
    const char *str)
{
    if (str == NULL || page >= SH1106_PAGE_COUNT)
        return;

    while (*str != '\0' && x <= SH1106_WIDTH - 6U)
    {
        SH1106_WriteChar(x, page, *str);

        x += 6;
        str++;
    }
}

esp_err_t SH1106_PrintMeasurements(
    i2c_master_dev_handle_t device,
    float temperature_celsius,
    float humidity_percent)
{
    const int32_t temperature_hundredths =
        (int32_t)(temperature_celsius * 100.0f +
                  (temperature_celsius >= 0 ? 0.5f : -0.5f));
    const int32_t temperature_abs =
        temperature_hundredths < 0
            ? -temperature_hundredths
            : temperature_hundredths;
    const uint32_t humidity_hundredths =
        (uint32_t)(humidity_percent * 100.0f + 0.5f);

    char temperature_string[24];
    char humidity_string[24];

    snprintf(
        temperature_string,
        sizeof(temperature_string),
        "TEMP: %s%ld.%02ld C",
        temperature_hundredths < 0 ? "-" : "",
        (long)(temperature_abs / 100),
        (long)(temperature_abs % 100)
    );
    snprintf(
        humidity_string,
        sizeof(humidity_string),
        "HUMIDITY: %lu.%02lu%%",
        (unsigned long)(humidity_hundredths / 100),
        (unsigned long)(humidity_hundredths % 100)
    );

    memset(framebuffer, 0, sizeof(framebuffer));
    SH1106_WriteString(0, 1, temperature_string);
    SH1106_WriteString(0, 6, humidity_string);

    return SH1106_Update(device);
}
