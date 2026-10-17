// K40 laser panel: Elecrow CrowPanel Advanced 5" ESP32-P4 <-> grblHAL (SKR Mini E3 V3) <-> LightBurn over Wi-Fi
#include "board.hpp"
#include "display.hpp"
#include "esp_log.h"
#include "grbl_link.hpp"
#include "jobs.hpp"
#include "ma_sensor.hpp"
#include "net_bridge.hpp"
#include "preview.hpp"
#include "sd_update.hpp"
#include "settings.hpp"
#include "nvs_flash.h"
#include "ui.hpp"
#include "web.hpp"

extern "C" void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    settings::init();
    board::init();
    display::init();
    sd_update::run();  // firmware.bin check, before anything else uses the card
    grbl::init();
    ma_sensor::init();  // optional tube-current sensor on I2C-OUT

    preview::init();  // before the UI, which shows its image
    display::lock();
    ui::build();
    display::unlock();
    display::start();

    net::init();
    jobs::init();  // remounts the card for G-code jobs (slot 0, alongside Wi-Fi's slot 1)
    web::init();
    ESP_LOGI("main", "K40 panel up");
}
