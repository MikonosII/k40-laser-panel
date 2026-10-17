// Board support for the Elecrow CrowPanel Advanced 5" ESP32-P4 (SKU DHE04005D, 800x480 IPS).
// Pins and protocols from Elecrow's ESP-IDF course code (example/V1.0/idf-code).
#pragma once
#include "driver/gpio.h"
#include "driver/i2c_master.h"

namespace board {

// I2C bus: GT911 touch (0x5D / 0x14) and the STC8 helper MCU (backlight, touch reset)
constexpr gpio_num_t I2C_SDA = GPIO_NUM_45;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_46;
constexpr uint8_t STC8_ADDR = 0x2F;
constexpr gpio_num_t TOUCH_RST = GPIO_NUM_36;
constexpr gpio_num_t TOUCH_INT = GPIO_NUM_42;

// 16-bit parallel RGB565 panel
constexpr int LCD_H_RES = 800;
constexpr int LCD_V_RES = 480;
constexpr gpio_num_t LCD_PCLK = GPIO_NUM_3;
constexpr gpio_num_t LCD_DE = GPIO_NUM_2;
constexpr gpio_num_t LCD_HSYNC = GPIO_NUM_40;
constexpr gpio_num_t LCD_VSYNC = GPIO_NUM_41;
// B0..B4, G0..G5, R0..R4
constexpr gpio_num_t LCD_DATA[16] = {
    GPIO_NUM_8,  GPIO_NUM_7,  GPIO_NUM_6,  GPIO_NUM_5,  GPIO_NUM_4,
    GPIO_NUM_14, GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_11, GPIO_NUM_10, GPIO_NUM_9,
    GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_16, GPIO_NUM_15,
};

void init();                       // I2C bus + STC8 helper
i2c_master_bus_handle_t i2c_bus();
void backlight(uint8_t percent);   // 0..100 via the STC8 PWM

}  // namespace board
