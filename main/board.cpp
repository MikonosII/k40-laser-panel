#include "board.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";

namespace board {

// STC8 helper register map (Elecrow bsp_stc8h1kxx.h)
static constexpr uint8_t STC8_REG_SET_PWM = 0x20;  // + channel; 0 = LCD backlight, duty 0..100

static i2c_master_bus_handle_t s_bus = nullptr;
static i2c_master_dev_handle_t s_stc8 = nullptr;

i2c_master_bus_handle_t i2c_bus() { return s_bus; }

void backlight(uint8_t percent) {
    if (!s_stc8) return;
    if (percent > 100) percent = 100;
    uint8_t buf[2] = {STC8_REG_SET_PWM, percent};
    esp_err_t err = i2c_master_transmit(s_stc8, buf, sizeof(buf), 100);
    if (err != ESP_OK) ESP_LOGW(TAG, "backlight write failed: %s", esp_err_to_name(err));
}

void init() {
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = I2C_SDA;
    cfg.scl_io_num = I2C_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus));
    vTaskDelay(pdMS_TO_TICKS(200));  // Elecrow waits for the STC8 to boot

    i2c_device_config_t dev = {};
    dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev.device_address = STC8_ADDR;
    dev.scl_speed_hz = 400000;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev, &s_stc8));
    if (i2c_master_probe(s_bus, STC8_ADDR, 100) != ESP_OK)
        ESP_LOGE(TAG, "STC8 helper (0x2F) not answering - backlight may stay off");
}

}  // namespace board
