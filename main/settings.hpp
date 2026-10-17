// User settings stored in NVS and editable on the panel's Setup screen.
// The menuconfig (Kconfig) values are only the defaults used until the user saves.
#pragma once
#include <string>

namespace settings {

struct Values {
    std::string ssid;
    std::string password;
    std::string allowed_ip;  // LightBurn PC; empty = accept any device
    int max_power_pct = 70;  // panel test-fire / pointer cap
    int brightness = 100;    // backlight %
};

void init();  // call after nvs_flash_init()
Values get();
bool save(const Values &v);

// Validation helpers for the Setup screen (return nullptr when OK, else a message)
const char *check_ssid(const std::string &s);
const char *check_password(const std::string &s);
const char *check_ip(const std::string &s);

}  // namespace settings
