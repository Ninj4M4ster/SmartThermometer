
#ifndef SH1106_H
#define SH1106_H

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <stdint.h>

/* Display geometry */
#define SH1106_WIDTH                   128U
#define SH1106_HEIGHT                  64U
#define SH1106_PAGE_HEIGHT             8U
#define SH1106_PAGE_COUNT              (SH1106_HEIGHT / SH1106_PAGE_HEIGHT)
#define SH1106_FRAMEBUFFER_SIZE        (SH1106_WIDTH * SH1106_PAGE_COUNT)

/* I2C */
#define SH1106_I2C_ADDRESS             0x3CU
#define SH1106_I2C_TIMEOUT_MS          100U

/* Control bytes */
#define SH1106_I2C_CONTROL_COMMAND     0x00U
#define SH1106_I2C_CONTROL_DATA        0x40U

/* Fundamental commands */
#define SH1106_CMD_DISPLAY_OFF         0xAEU
#define SH1106_CMD_DISPLAY_ON          0xAFU
#define SH1106_CMD_DISPLAY_NORMAL      0xA6U
#define SH1106_CMD_DISPLAY_INVERSE     0xA7U
#define SH1106_CMD_DISPLAY_RAM         0xA4U
#define SH1106_CMD_DISPLAY_ALL_ON      0xA5U

/* Addressing */
#define SH1106_CMD_SET_PAGE_ADDRESS    0xB0U
#define SH1106_CMD_SET_LOWER_COLUMN    0x00U
#define SH1106_CMD_SET_HIGHER_COLUMN   0x10U
#define SH1106_CMD_SET_START_LINE      0x40U

#define SH1106_PAGE_ADDRESS_MASK       0x07U
#define SH1106_COLUMN_NIBBLE_MASK      0x0FU
#define SH1106_COLUMN_HIGH_MASK        0x0FU

/* Segment and COM mapping */
#define SH1106_CMD_SEGMENT_NORMAL      0xA0U
#define SH1106_CMD_SEGMENT_REMAP       0xA1U
#define SH1106_CMD_COM_SCAN_NORMAL     0xC0U
#define SH1106_CMD_COM_SCAN_REMAP      0xC8U

/* Display configuration */
#define SH1106_CMD_SET_CONTRAST        0x81U
#define SH1106_CMD_SET_MULTIPLEX       0xA8U
#define SH1106_CMD_SET_DISPLAY_OFFSET  0xD3U
#define SH1106_CMD_SET_CLOCK_DIVIDE    0xD5U
#define SH1106_CMD_SET_PRECHARGE       0xD9U
#define SH1106_CMD_SET_COM_PINS        0xDAU
#define SH1106_CMD_SET_VCOM_DESELECT   0xDBU
#define SH1106_CMD_SET_DC_DC           0xADU

/* Typical initialization parameters */
#define SH1106_CLOCK_DIVIDE_DEFAULT    0x80U
#define SH1106_MULTIPLEX_64_ROWS       0x3FU
#define SH1106_DISPLAY_OFFSET_DEFAULT  0x00U
#define SH1106_DC_DC_ENABLE            0x8BU
#define SH1106_CONTRAST_DEFAULT        0x7FU
#define SH1106_PRECHARGE_DEFAULT       0x22U
#define SH1106_COM_PINS_128X64         0x12U
#define SH1106_VCOM_DESELECT_DEFAULT   0x35U

/* Addressing configuration */
#define SH1106_COLUMN_OFFSET           0x02U

esp_err_t SH1106_Init(i2c_master_dev_handle_t device);
esp_err_t SH1106_Update(i2c_master_dev_handle_t device);
esp_err_t SH1106_Clear(i2c_master_dev_handle_t device);
void SH1106_Fill(uint8_t pattern);

void SH1106_WriteChar(uint8_t x, uint8_t page, char c);
void SH1106_WriteString(uint8_t x, uint8_t page, const char *str);

esp_err_t SH1106_PrintMeasurements(
    i2c_master_dev_handle_t device,
    float temperature_celsius,
    float humidity_percent);

#endif /* SH1106_H */
