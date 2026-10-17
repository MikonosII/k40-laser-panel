// User settings stored in NVS and editable on the panel's Setup screen.
// The menuconfig (Kconfig) values are only the defaults used until the user saves.
#pragma once
#include <cstdint>
#include <string>

namespace settings {

struct Values {
    std::string ssid;
    std::string password;
    std::string allowed_ip;  // LightBurn PC; empty = accept any device
    int max_power_pct = 70;  // panel test-fire / pointer cap
    int brightness = 100;    // backlight %
    int sleep_min = 5;       // screen sleep after this many idle minutes; 0 = never
    std::string pin;         // 4 digits guarding Setup and test fire; empty = no lock
    bool flipped = false;    // rotate the portrait UI 180 degrees
    int ma_limit = 18;       // warn above this tube current (needs the current sensor)
    int ma_trip = 22;        // shut the laser off above this tube current
};

void init();  // call after nvs_flash_init()
Values get();
bool save(const Values &v);
void clear_pin();  // recovery: reset_pin.txt on the SD card

// Laser on-time counter (tube hours), kept in RAM and written to flash periodically
void tube_add_ms(uint32_t ms);
uint32_t tube_seconds();
void tube_flush();  // write to NVS if changed
void tube_reset();

// Validation helpers for the Setup screen (return nullptr when OK, else a message)
const char *check_ssid(const std::string &s);
const char *check_password(const std::string &s);
const char *check_ip(const std::string &s);
const char *check_pin(const std::string &s);

}  // namespace settings
