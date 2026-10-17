#include "settings.hpp"

#include <cstdio>
#include <mutex>

#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "settings";
static const char *NS = "k40";

namespace settings {

static std::mutex s_mx;
static Values s_v;

static std::string read_str(nvs_handle_t h, const char *key, const char *def) {
    size_t len = 0;
    if (nvs_get_str(h, key, nullptr, &len) != ESP_OK || len == 0) return def;
    std::string out(len, '\0');
    if (nvs_get_str(h, key, out.data(), &len) != ESP_OK) return def;
    out.resize(len - 1);  // drop the terminator
    return out;
}

static int read_int(nvs_handle_t h, const char *key, int def) {
    int32_t v = 0;
    return nvs_get_i32(h, key, &v) == ESP_OK ? (int)v : def;
}

void init() {
    Values v;
    v.ssid = CONFIG_K40_WIFI_SSID;
    v.password = CONFIG_K40_WIFI_PASSWORD;
    v.allowed_ip = CONFIG_K40_ALLOWED_IP;
    v.max_power_pct = CONFIG_K40_MAX_POWER_PCT;
    v.brightness = CONFIG_K40_BACKLIGHT_PCT;

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        v.ssid = read_str(h, "ssid", v.ssid.c_str());
        v.password = read_str(h, "pass", v.password.c_str());
        v.allowed_ip = read_str(h, "lb_ip", v.allowed_ip.c_str());
        v.max_power_pct = read_int(h, "max_pct", v.max_power_pct);
        v.brightness = read_int(h, "bright", v.brightness);
        nvs_close(h);
        ESP_LOGI(TAG, "loaded saved settings");
    } else {
        ESP_LOGI(TAG, "no saved settings; using menuconfig defaults");
    }
    std::lock_guard<std::mutex> g(s_mx);
    s_v = v;
}

Values get() {
    std::lock_guard<std::mutex> g(s_mx);
    return s_v;
}

bool save(const Values &v) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_str(h, "ssid", v.ssid.c_str()) == ESP_OK &&
              nvs_set_str(h, "pass", v.password.c_str()) == ESP_OK &&
              nvs_set_str(h, "lb_ip", v.allowed_ip.c_str()) == ESP_OK &&
              nvs_set_i32(h, "max_pct", v.max_power_pct) == ESP_OK &&
              nvs_set_i32(h, "bright", v.brightness) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    if (ok) {
        std::lock_guard<std::mutex> g(s_mx);
        s_v = v;
    }
    return ok;
}

const char *check_ssid(const std::string &s) {
    if (s.empty()) return "Enter the Wi-Fi name";
    if (s.size() > 32) return "Wi-Fi name is too long (32 max)";
    return nullptr;
}

const char *check_password(const std::string &s) {
    if (!s.empty() && s.size() < 8) return "Password must be at least 8 characters";
    if (s.size() > 63) return "Password is too long (63 max)";
    return nullptr;
}

const char *check_ip(const std::string &s) {
    if (s.empty()) return nullptr;  // blank = any device
    unsigned a, b, c, d;
    char extra;
    if (sscanf(s.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4 || a > 255 || b > 255 ||
        c > 255 || d > 255)
        return "IP must look like 192.168.1.50";
    return nullptr;
}

}  // namespace settings
