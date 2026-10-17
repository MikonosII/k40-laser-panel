#include "ma_sensor.hpp"

#include <mutex>

#include "board.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "grbl_link.hpp"
#include "settings.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "ma_sensor";

namespace ma_sensor {

static constexpr uint8_t ADDR = 0x40;  // A0/A1 tied low on both common breakouts
static constexpr uint8_t REG_CONFIG = 0x00, REG_SHUNT = 0x01, REG_MFG_ID = 0xFE;

static i2c_master_dev_handle_t s_dev = nullptr;
static std::mutex s_mx;
static Reading s_r;
static volatile int s_trip_ma = 0;

void set_trip_limit(int limit_ma) { s_trip_ma = limit_ma; }

void clear_fault() {
    std::lock_guard<std::mutex> g(s_mx);
    s_r.fault = false;
    s_r.fault_ma = 0;
}

static bool write_reg(uint8_t reg, uint16_t v) {
    uint8_t buf[3] = {reg, (uint8_t)(v >> 8), (uint8_t)v};
    return i2c_master_transmit(s_dev, buf, 3, 50) == ESP_OK;
}

static bool read_reg(uint8_t reg, uint16_t *v) {
    uint8_t out[2];
    if (i2c_master_transmit_receive(s_dev, &reg, 1, out, 2, 50) != ESP_OK) return false;
    *v = (uint16_t)(out[0] << 8 | out[1]);
    return true;
}

static void reader_task(void *) {
    const float shunt_ohm = CONFIG_K40_MA_SHUNT_MOHM / 1000.0f;
    // Shunt-voltage LSB: INA226 2.5 uV, INA219 10 uV
    const float lsb_v = s_r.ina226 ? 2.5e-6f : 10e-6f;
    float peak = 0;
    int64_t peak_at = 0;
    int over = 0;
    int64_t last_stop = 0;
    while (true) {
        uint16_t raw;
        if (read_reg(REG_SHUNT, &raw)) {
            float ma = (int16_t)raw * lsb_v / shunt_ohm * 1000.0f;
            if (ma < 0) ma = -ma;  // wiring direction doesn't matter
            int64_t now = esp_timer_get_time();
            if (ma >= peak || now - peak_at > 2000000) {
                peak = ma;
                peak_at = now;
            }
            // Shut-off: 3 consecutive readings over the limit (~60 ms) ignores single glitches
            int trip = s_trip_ma;
            over = (trip > 0 && ma > trip) ? over + 1 : 0;
            bool stop = false;
            {
                std::lock_guard<std::mutex> g(s_mx);
                s_r.ma = ma;
                s_r.peak_ma = peak;
                if (over >= 3 && now - last_stop > 500000) {
                    if (!s_r.fault) {
                        s_r.fault = true;
                        s_r.fault_ma = ma;
                    }
                    stop = true;  // also re-stop if it fires over the limit again while latched
                }
            }
            if (stop) {
                last_stop = now;
                ESP_LOGE(TAG, "tube current %.1f mA over %d mA shut-off - stopping", ma, trip);
                grbl::panel_stop();
            }
        }
        // 50 Hz: fast enough to trip within ~60 ms of an over-current
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void init() {
    if (i2c_master_probe(board::i2c_bus(), ADDR, 50) != ESP_OK) {
        ESP_LOGI(TAG, "no current sensor at 0x40 (optional)");
        return;
    }
    i2c_device_config_t dev = {};
    dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev.device_address = ADDR;
    dev.scl_speed_hz = 100000;  // conservative through the isolator
    if (i2c_master_bus_add_device(board::i2c_bus(), &dev, &s_dev) != ESP_OK) return;

    uint16_t id = 0;
    bool is226 = read_reg(REG_MFG_ID, &id) && id == 0x5449;  // "TI", INA226 only
    bool ok;
    if (is226) {
        // 16-sample average, 1.1 ms conversions, shunt voltage continuous
        ok = write_reg(REG_CONFIG, 0x4525);
    } else {
        // +/-40 mV range, 16-sample average (8.5 ms), shunt voltage continuous
        ok = write_reg(REG_CONFIG, 0x01E5);
    }
    if (!ok) {
        ESP_LOGE(TAG, "sensor at 0x40 didn't accept its configuration");
        return;
    }
    {
        std::lock_guard<std::mutex> g(s_mx);
        s_r.present = true;
        s_r.ina226 = is226;
    }
    ESP_LOGI(TAG, "%s found, shunt %d mOhm", is226 ? "INA226" : "INA219", CONFIG_K40_MA_SHUNT_MOHM);
    s_trip_ma = settings::get().ma_trip;
    xTaskCreate(reader_task, "ma_sensor", 3072, nullptr, 5, nullptr);
}

Reading get() {
    std::lock_guard<std::mutex> g(s_mx);
    return s_r;
}

}  // namespace ma_sensor
