#include "grbl_link.hpp"

#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "grbl";

namespace grbl {

static constexpr uart_port_t PORT = UART_NUM_1;
static constexpr size_t CONSOLE_LINES = 14;
static constexpr uint32_t HOST_QUIET_MS = 1500;  // after a panel Stop
static constexpr uint32_t JOB_END_IDLE_MS = 2000;

enum class Owner : uint8_t { Host, Panel };

static std::mutex s_mx;
static Status s_st;
static float s_wco[3] = {};
static uint32_t s_last_report_ms = 0;

static std::deque<Owner> s_inflight;          // one entry per line sent, oldest first
static std::deque<std::string> s_host_pending;  // host lines held behind a panel line
static std::string s_host_line;               // partial line from host
static int s_panel_inflight = 0;

static HostWriter s_host = nullptr;
static bool s_host_blocked = false;
static uint32_t s_host_last_ms = 0;

static uint32_t s_last_host_line_ms = 0;
static uint32_t s_job_start_ms = 0;
static uint32_t s_idle_since_ms = 0;

static std::deque<std::string> s_console;

static uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }

static bool is_realtime(uint8_t c) {
    return c == '?' || c == '!' || c == '~' || c == 0x18 || c >= 0x80;
}

const char *state_name(State s) {
    switch (s) {
        case State::Idle: return "Idle";
        case State::Run: return "Run";
        case State::Hold: return "Hold";
        case State::Jog: return "Jog";
        case State::Alarm: return "Alarm";
        case State::Door: return "Door";
        case State::Check: return "Check";
        case State::Home: return "Homing";
        case State::Sleep: return "Sleep";
        case State::Tool: return "Tool";
        default: return "No link";
    }
}

// ---- helpers (call with s_mx held) ----------------------------------------

static void console_push(const std::string &line) {
    s_console.push_back(line);
    while (s_console.size() > CONSOLE_LINES) s_console.pop_front();
}

static void uart_put(const char *data, size_t len) { uart_write_bytes(PORT, data, len); }

static void write_line_locked(Owner who, const std::string &line_with_nl) {
    s_inflight.push_back(who);
    if (who == Owner::Panel) {
        ++s_panel_inflight;
    } else {
        s_last_host_line_ms = now_ms();
    }
    uart_put(line_with_nl.data(), line_with_nl.size());
}

static void flush_host_pending_locked() {
    while (s_panel_inflight == 0 && !s_host_pending.empty()) {
        write_line_locked(Owner::Host, s_host_pending.front());
        s_host_pending.pop_front();
    }
}

static void clear_streams_locked() {
    s_inflight.clear();
    s_host_pending.clear();
    s_host_line.clear();
    s_panel_inflight = 0;
}

static bool job_running_locked() {
    State s = s_st.state;
    return s_st.job_active || s == State::Run || s == State::Hold || s == State::Door;
}

// ---- status report parsing ------------------------------------------------

static void parse_floats(const char *p, float *out, int n) {
    for (int i = 0; i < n && p && *p; ++i) {
        out[i] = strtof(p, (char **)&p);
        if (*p == ',') ++p;
    }
}

static State parse_state(const std::string &f) {
    if (f.rfind("Idle", 0) == 0) return State::Idle;
    if (f.rfind("Run", 0) == 0) return State::Run;
    if (f.rfind("Hold", 0) == 0) return State::Hold;
    if (f.rfind("Jog", 0) == 0) return State::Jog;
    if (f.rfind("Alarm", 0) == 0) return State::Alarm;
    if (f.rfind("Door", 0) == 0) return State::Door;
    if (f.rfind("Check", 0) == 0) return State::Check;
    if (f.rfind("Home", 0) == 0) return State::Home;
    if (f.rfind("Sleep", 0) == 0) return State::Sleep;
    if (f.rfind("Tool", 0) == 0) return State::Tool;
    return State::Unknown;
}

static void parse_status_locked(const std::string &line) {
    // <Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:...|Ov:100,100,100|A:SF|Pn:XYD>
    std::string body = line.substr(1, line.size() - 2);
    bool have_acc = false, have_pins = false, have_m = false, have_w = false;
    float m[3] = {}, w[3] = {};
    size_t start = 0;
    int idx = 0;
    while (start <= body.size()) {
        size_t bar = body.find('|', start);
        std::string f = body.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
        if (idx == 0) {
            s_st.state = parse_state(f);
        } else if (f.rfind("MPos:", 0) == 0) {
            parse_floats(f.c_str() + 5, m, 3);
            have_m = true;
        } else if (f.rfind("WPos:", 0) == 0) {
            parse_floats(f.c_str() + 5, w, 3);
            have_w = true;
        } else if (f.rfind("WCO:", 0) == 0) {
            parse_floats(f.c_str() + 4, s_wco, 3);
        } else if (f.rfind("FS:", 0) == 0) {
            float fs[2] = {};
            parse_floats(f.c_str() + 3, fs, 2);
            s_st.feed = fs[0];
            s_st.spindle = fs[1];
        } else if (f.rfind("F:", 0) == 0) {
            s_st.feed = strtof(f.c_str() + 2, nullptr);
        } else if (f.rfind("Ov:", 0) == 0) {
            float ov[3] = {100, 100, 100};
            parse_floats(f.c_str() + 3, ov, 3);
            s_st.ov_feed = (int)ov[0];
            s_st.ov_rapid = (int)ov[1];
            s_st.ov_spindle = (int)ov[2];
        } else if (f.rfind("A:", 0) == 0) {
            have_acc = true;
            s_st.spindle_on = f.find_first_of("SC", 2) != std::string::npos;
            s_st.flood = f.find('F', 2) != std::string::npos;
            s_st.mist = f.find('M', 2) != std::string::npos;
        } else if (f.rfind("Pn:", 0) == 0) {
            have_pins = true;
            s_st.door_pin = f.find('D', 3) != std::string::npos;
        }
        if (bar == std::string::npos) break;
        start = bar + 1;
        ++idx;
    }
    // grbl omits A: and Pn: when nothing is active
    if (!have_acc) s_st.spindle_on = s_st.flood = s_st.mist = false;
    if (!have_pins) s_st.door_pin = false;
    for (int i = 0; i < 3; ++i) {
        if (have_m) {
            s_st.mpos[i] = m[i];
            s_st.wpos[i] = m[i] - s_wco[i];
        } else if (have_w) {
            s_st.wpos[i] = w[i];
            s_st.mpos[i] = w[i] + s_wco[i];
        }
    }
    if (s_st.state != State::Alarm) s_st.alarm = 0;
    s_last_report_ms = now_ms();

    // Job timer: a job is LightBurn lines + motion, ending after a quiet idle
    uint32_t t = now_ms();
    bool moving = s_st.state == State::Run || s_st.state == State::Hold || s_st.state == State::Door;
    bool host_recent = t - s_last_host_line_ms < 3000 || !s_inflight.empty();
    if (moving && host_recent && !s_st.job_active) {
        s_st.job_active = true;
        s_job_start_ms = t;
    }
    if (s_st.job_active) {
        s_st.job_seconds = (t - s_job_start_ms) / 1000;
        bool quiet = s_inflight.empty() && s_host_pending.empty();
        if ((s_st.state == State::Idle && quiet) || s_st.state == State::Alarm) {
            if (s_idle_since_ms == 0) s_idle_since_ms = t;
            if (t - s_idle_since_ms > JOB_END_IDLE_MS || s_st.state == State::Alarm) {
                s_st.job_active = false;
                s_idle_since_ms = 0;
            }
        } else {
            s_idle_since_ms = 0;
        }
    }
}

// ---- line from grblHAL ----------------------------------------------------

static void handle_line(const std::string &line) {
    HostWriter host = nullptr;
    bool forward = true;
    {
        std::lock_guard<std::mutex> g(s_mx);
        host = s_host;
        if (line.size() >= 2 && line.front() == '<' && line.back() == '>') {
            parse_status_locked(line);
        } else if (line == "ok" || line.rfind("error:", 0) == 0) {
            if (!s_inflight.empty()) {
                Owner who = s_inflight.front();
                s_inflight.pop_front();
                if (who == Owner::Panel) {
                    forward = false;
                    --s_panel_inflight;
                    if (line != "ok") console_push("panel " + line);
                    flush_host_pending_locked();
                }
            }
        } else if (!line.empty()) {
            if (line.rfind("ALARM:", 0) == 0) {
                s_st.alarm = atoi(line.c_str() + 6);
                s_st.state = State::Alarm;
            }
            if (line.rfind("Grbl", 0) == 0) {  // reset banner: queued replies are gone
                clear_streams_locked();
                s_st.job_active = false;
            }
            console_push(line);
        }
    }
    if (forward && host) {
        host((const uint8_t *)line.data(), line.size());
        host((const uint8_t *)"\r\n", 2);
    }
}

static void rx_task(void *) {
    uint8_t buf[256];
    std::string line;
    while (true) {
        int n = uart_read_bytes(PORT, buf, sizeof(buf), pdMS_TO_TICKS(20));
        for (int i = 0; i < n; ++i) {
            char c = (char)buf[i];
            if (c == '\n') {
                handle_line(line);
                line.clear();
            } else if (c != '\r') {
                if (line.size() < 512) line.push_back(c);
            }
        }
    }
}

static void poll_task(void *) {
    while (true) {
        uart_put("?", 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        std::lock_guard<std::mutex> g(s_mx);
        s_st.link_ok = now_ms() - s_last_report_ms < 1000;
        if (!s_st.link_ok) s_st.state = State::Unknown;
    }
}

// ---- public API ------------------------------------------------------------

void init() {
    uart_config_t cfg = {};
    cfg.baud_rate = CONFIG_K40_GRBL_BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_driver_install(PORT, 4096, 2048, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(PORT, CONFIG_K40_GRBL_TX_GPIO, CONFIG_K40_GRBL_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreatePinnedToCore(rx_task, "grbl_rx", 6144, nullptr, 6, nullptr, 0);
    xTaskCreatePinnedToCore(poll_task, "grbl_poll", 3072, nullptr, 5, nullptr, 0);
    ESP_LOGI(TAG, "UART%d TX=%d RX=%d @%d", PORT, CONFIG_K40_GRBL_TX_GPIO, CONFIG_K40_GRBL_RX_GPIO,
             CONFIG_K40_GRBL_BAUD);
}

Status status() {
    std::lock_guard<std::mutex> g(s_mx);
    Status s = s_st;
    s.host_connected = s_host != nullptr;
    s.panel_busy = s_panel_inflight > 0;
    return s;
}

std::vector<std::string> console() {
    std::lock_guard<std::mutex> g(s_mx);
    return std::vector<std::string>(s_console.begin(), s_console.end());
}

void host_attach(HostWriter w) {
    std::lock_guard<std::mutex> g(s_mx);
    s_host = w;
    s_host_blocked = false;
    // Anything LightBurn had in flight is now orphaned; drop its bookkeeping
    // (lines already sent stay in s_inflight so their replies still pair up)
    s_host_line.clear();
    s_host_pending.clear();
    console_push(w ? "LightBurn connected" : "LightBurn disconnected");
}

void host_rx(const uint8_t *data, size_t len) {
    std::lock_guard<std::mutex> g(s_mx);
    uint32_t t = now_ms();
    for (size_t i = 0; i < len; ++i) {
        uint8_t c = data[i];
        if (s_host_blocked) {
            // Status polls are harmless and don't count as LightBurn activity
            if (c == '?') {
                uart_put("?", 1);
                continue;
            }
            if (t - s_host_last_ms < HOST_QUIET_MS) {
                s_host_last_ms = t;
                continue;  // still draining LightBurn's leftovers
            }
            s_host_blocked = false;
            console_push("LightBurn input resumed");
        }
        if (is_realtime(c)) {
            uart_put((const char *)&c, 1);
            if (c == 0x18) clear_streams_locked();
            continue;
        }
        s_host_last_ms = t;
        s_host_line.push_back((char)c);
        if (c == '\n') {
            if (s_panel_inflight == 0 && s_host_pending.empty()) {
                write_line_locked(Owner::Host, s_host_line);
            } else {
                s_host_pending.push_back(s_host_line);
            }
            s_host_line.clear();
        } else if (s_host_line.size() > 256) {
            s_host_line.clear();  // garbage; grbl would reject it anyway
        }
    }
}

void realtime(uint8_t c) {
    uart_put((const char *)&c, 1);
}

bool panel_can_send() {
    std::lock_guard<std::mutex> g(s_mx);
    return s_st.link_ok && s_inflight.empty() && s_host_pending.empty() && s_host_line.empty() &&
           !job_running_locked();
}

bool send_lines(const char *const *lines, int n) {
    std::lock_guard<std::mutex> g(s_mx);
    if (!(s_st.link_ok && s_inflight.empty() && s_host_pending.empty() && s_host_line.empty() &&
          !job_running_locked())) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        std::string l(lines[i]);
        console_push("> " + l);
        l.push_back('\n');
        write_line_locked(Owner::Panel, l);
    }
    return true;
}

bool send(const char *line) { return send_lines(&line, 1); }

void panel_stop() {
    std::lock_guard<std::mutex> g(s_mx);
    uint8_t c = rt::RESET;
    uart_put((const char *)&c, 1);
    clear_streams_locked();
    s_st.job_active = false;
    if (s_host) {
        s_host_blocked = true;
        s_host_last_ms = now_ms();
    }
    console_push("Stopped from panel");
}

}  // namespace grbl
