idf_component_register(
    SRCS "main.cpp" "board.cpp" "display.cpp" "grbl_link.cpp" "net_bridge.cpp" "ui.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_driver_uart esp_driver_i2c esp_driver_gpio
             esp_wifi esp_netif esp_event nvs_flash esp_timer lwip
             lvgl esp_lcd_touch_gt911 esp_wifi_remote esp_hosted)
