// K40 laser panel: Elecrow CrowPanel Advanced 5" ESP32-P4 <-> grblHAL (SKR Mini E3 V3) <-> LightBurn over Wi-Fi
#include "board.hpp"
#include "display.hpp"
#include "esp_log.h"
#include "grbl_link.hpp"
#include "net_bridge.hpp"
#include "nvs_flash.h"
#include "ui.hpp"

extern "C" void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    board::init();
    display::init();
    grbl::init();

    display::lock();
    ui::build();
    display::unlock();
    display::start();

    net::init();
    ESP_LOGI("main", "K40 panel up");
}
