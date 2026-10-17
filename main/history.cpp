#include "history.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>

#include "esp_log.h"
#include "esp_timer.h"
#include "grbl_link.hpp"
#include "jobs.hpp"
#include "ma_sensor.hpp"
#include "nvs.h"

static const char *TAG = "history";

namespace history {

static constexpr int MAX = 20;
static constexpr uint32_t MIN_SECS = 3;  // ignore blips

static std::mutex s_mx;
static std::vector<Entry> s_list;  // newest first
static uint32_t s_rev = 1;

static bool s_active = false;
static Entry s_cur;
static int64_t s_t0 = 0;
static float s_peak = 0;

static void save_locked() {
    nvs_handle_t h;
    if (nvs_open("k40hist", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "list", s_list.data(), s_list.size() * sizeof(Entry));
    nvs_commit(h);
    nvs_close(h);
}

void init() {
    std::lock_guard<std::mutex> g(s_mx);
    nvs_handle_t h;
    if (nvs_open("k40hist", NVS_READONLY, &h) != ESP_OK) return;
    size_t len = 0;
    if (nvs_get_blob(h, "list", nullptr, &len) == ESP_OK && len % sizeof(Entry) == 0 &&
        len <= MAX * sizeof(Entry)) {
        s_list.resize(len / sizeof(Entry));
        if (nvs_get_blob(h, "list", s_list.data(), &len) != ESP_OK) s_list.clear();
    }
    nvs_close(h);
}

static uint32_t now_unix() {
    time_t t = time(nullptr);
    return t > 1700000000 ? (uint32_t)t : 0;  // before 2023 = clock not set yet
}

void tick() {
    grbl::Status st = grbl::status();
    ma_sensor::Reading mr = ma_sensor::get();
    if (st.job_active && !s_active) {
        s_active = true;
        memset(&s_cur, 0, sizeof(s_cur));
        jobs::Progress p = jobs::progress();
        std::string name = p.state == jobs::RunState::Running ? p.name : "LightBurn job";
        snprintf(s_cur.name, sizeof(s_cur.name), "%s", name.c_str());
        s_cur.start = now_unix();
        s_t0 = esp_timer_get_time();
        s_peak = 0;
    }
    if (s_active && mr.present) s_peak = std::max(s_peak, mr.ma);
    if (!st.job_active && s_active) {
        s_active = false;
        s_cur.secs = (uint32_t)((esp_timer_get_time() - s_t0) / 1000000);
        s_cur.peak_ma10 = (uint16_t)std::min(65535.0f, s_peak * 10 + 0.5f);
        jobs::Progress p = jobs::progress();
        bool panel = strcmp(s_cur.name, "LightBurn job") != 0;
        if (panel) {
            s_cur.result = (uint8_t)(p.state == jobs::RunState::Done      ? Result::Done
                                     : p.state == jobs::RunState::Failed  ? Result::Failed
                                                                          : Result::Stopped);
        } else {
            s_cur.result = (uint8_t)(st.state == grbl::State::Alarm || mr.fault ? Result::Stopped
                                                                                : Result::LightBurn);
        }
        if (s_cur.secs < MIN_SECS) return;
        std::lock_guard<std::mutex> g(s_mx);
        s_list.insert(s_list.begin(), s_cur);
        if (s_list.size() > MAX) s_list.resize(MAX);
        ++s_rev;
        save_locked();
        ESP_LOGI(TAG, "%s: %us", s_cur.name, (unsigned)s_cur.secs);
    }
}

std::vector<Entry> list() {
    std::lock_guard<std::mutex> g(s_mx);
    return s_list;
}

uint32_t revision() {
    std::lock_guard<std::mutex> g(s_mx);
    return s_rev;
}

std::string describe(const Entry &e) {
    char when[32] = "";
    if (e.start) {
        time_t t = e.start;
        struct tm tm;
        localtime_r(&t, &tm);
        strftime(when, sizeof(when), "%b %d %I:%M %p", &tm);
        if (when[7] == '0') memmove(when + 7, when + 8, strlen(when + 7));  // "03:02" -> "3:02"
    }
    static const char *res[] = {"Done", "Stopped", "Failed", "LightBurn"};
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%s%s%02u:%02u", when, *when ? "  -  " : "", (unsigned)(e.secs / 60),
                     (unsigned)(e.secs % 60));
    if (e.peak_ma10) n += snprintf(buf + n, sizeof(buf) - n, "  -  peak %u.%u mA", e.peak_ma10 / 10, e.peak_ma10 % 10);
    snprintf(buf + n, sizeof(buf) - n, "  -  %s", res[std::min<uint8_t>(e.result, 3)]);
    return buf;
}

}  // namespace history
