#include "jobs.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>

#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "grbl_link.hpp"
#include "ma_sensor.hpp"
#include "sd_update.hpp"
#include "sdmmc_cmd.h"

static const char *TAG = "jobs";

namespace jobs {

static constexpr const char *MOUNT = "/sdcard";
static constexpr const char *JOB_DIR = "/sdcard/jobs";
static constexpr const char *TMP = "/sdcard/jobs/.upload.tmp";
static constexpr uint32_t RAM_CAP = 20u * 1024 * 1024;  // PSRAM kept for jobs with no card
static constexpr uint32_t SD_RETRY_MS = 5000;
static constexpr size_t MAX_LINE = 250;  // grblHAL's line buffer is 256 incl. terminator

struct RamJob {
    std::string name;
    uint8_t *data = nullptr;
    uint32_t size = 0;
};

struct Upload {
    std::string name;
    bool sd = false;
    FILE *f = nullptr;
    RamJob ram;
    uint32_t pos = 0;
};

static std::recursive_mutex s_mx;
static sdmmc_card_t *s_card = nullptr;
static uint32_t s_last_try_ms = 0;
static std::vector<RamJob> s_ram;
static uint32_t s_rev = 1;
static bool s_uploading = false;
static std::string s_reading;  // job the preview is reading (can't be deleted/replaced meanwhile)

static Progress s_prog;
static uint32_t s_start_ms = 0;
static TaskHandle_t s_task = nullptr;
static volatile bool s_stop_req = false;
// What the runner streams (pinned at start(); RAM jobs can't be deleted while running)
static std::string s_run_path;
static const uint8_t *s_run_ram = nullptr;

static void runner_start();
static uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }
static std::string path_of(const std::string &name) { return std::string(JOB_DIR) + "/" + name; }

// ---- SD card ------------------------------------------------------------------------

static void sd_drop_locked() {
    if (!s_card) return;
    esp_vfs_fat_sdcard_unmount(MOUNT, s_card);  // only releases slot 0; Wi-Fi's slot 1 keeps the host
    s_card = nullptr;
    ++s_rev;
    ESP_LOGW(TAG, "SD card gone - jobs now go to memory");
}

// Mounting a missing card takes a while, so it runs without s_mx (the UI reads progress
// under s_mx several times a second). s_mount_mx keeps two callers from mounting at once.
static std::mutex s_mount_mx;
static bool s_tried = false;

static bool sd_ok(bool force = false) {
    {
        std::lock_guard<std::recursive_mutex> g(s_mx);
        // Is the card still there? CMD13 fails once it's pulled out.
        if (s_card && sdmmc_get_status(s_card) != ESP_OK) {
            // Don't unmount under a job reading from the card; it fails on its next read
            if (s_prog.state == RunState::Running && !s_run_ram) return false;
            sd_drop_locked();
        }
        if (s_card) return true;
        if (s_uploading) return false;
        if (!force && s_tried && now_ms() - s_last_try_ms < SD_RETRY_MS) return false;
        s_tried = true;
        s_last_try_ms = now_ms();
    }
    std::unique_lock<std::mutex> m(s_mount_mx, std::try_to_lock);
    if (!m.owns_lock()) return false;

    sdmmc_host_t host;
    sdmmc_slot_config_t slot;
    sd_update::sd_config(&host, &slot);
    esp_vfs_fat_sdmmc_mount_config_t mcfg = {};
    mcfg.format_if_mount_failed = false;
    mcfg.max_files = 4;
    mcfg.allocation_unit_size = 16 * 1024;
    sdmmc_card_t *card = nullptr;
    if (esp_vfs_fat_sdmmc_mount(MOUNT, &host, &slot, &mcfg, &card) != ESP_OK) return false;
    mkdir(JOB_DIR, 0775);
    ::remove(TMP);  // leftover from an interrupted upload
    std::lock_guard<std::recursive_mutex> g(s_mx);
    s_card = card;
    ++s_rev;
    ESP_LOGI(TAG, "SD card mounted - jobs in %s", JOB_DIR);
    return true;
}

// ---- names -----------------------------------------------------------------------------

static std::string clean_name(const std::string &in) {
    std::string out;
    for (char c : in) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  strchr(" .-_()", c);
        if (ok && c) out.push_back(c);
    }
    while (!out.empty() && (out.front() == ' ' || out.front() == '.')) out.erase(out.begin());
    while (!out.empty() && out.back() == ' ') out.pop_back();
    if (out.size() > 60) out.resize(60);
    if (out.empty()) out = "job";
    if (out.find('.') == std::string::npos) out += ".gc";
    return out;
}

static RamJob *ram_find_locked(const std::string &name) {
    for (RamJob &j : s_ram)
        if (j.name == name) return &j;
    return nullptr;
}

static uint32_t ram_used_locked() {
    uint32_t n = 0;
    for (RamJob &j : s_ram) n += j.size;
    return n;
}

// ---- public: storage and list -------------------------------------------------------------

void init() {
    sd_ok(true);
    runner_start();
}

StorageInfo storage() {
    StorageInfo si;
    si.sd = sd_ok();
    if (si.sd && esp_vfs_fat_info(MOUNT, &si.total, &si.free) == ESP_OK) return si;
    si.sd = false;
    {
        std::lock_guard<std::recursive_mutex> g(s_mx);
        si.total = RAM_CAP;
        uint32_t used = ram_used_locked();
        uint64_t left = RAM_CAP > used ? RAM_CAP - used : 0;
        uint64_t block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        si.free = std::min(left, block > 256 * 1024 ? block - 256 * 1024 : 0);  // keep some for the UI
    }
    return si;
}

std::vector<Info> list() {
    bool sd = sd_ok();
    std::lock_guard<std::recursive_mutex> g(s_mx);
    std::vector<Info> out;
    if (sd && s_card) {
        if (DIR *d = opendir(JOB_DIR)) {
            while (dirent *e = readdir(d)) {
                if (e->d_name[0] == '.') continue;
                struct stat st;
                if (stat(path_of(e->d_name).c_str(), &st) == 0 && S_ISREG(st.st_mode))
                    out.push_back({e->d_name, (uint32_t)st.st_size});
            }
            closedir(d);
        }
    }
    for (RamJob &j : s_ram) out.push_back({j.name, j.size});
    std::sort(out.begin(), out.end(), [](const Info &a, const Info &b) { return a.name < b.name; });
    return out;
}

uint32_t revision() {
    std::lock_guard<std::recursive_mutex> g(s_mx);
    return s_rev;
}

bool remove(const std::string &name) {
    std::lock_guard<std::recursive_mutex> g(s_mx);
    if (s_prog.state == RunState::Running && s_prog.name == name) return false;
    if (s_reading == name) return false;
    for (size_t i = 0; i < s_ram.size(); ++i) {
        if (s_ram[i].name == name) {
            heap_caps_free(s_ram[i].data);
            s_ram.erase(s_ram.begin() + i);
            ++s_rev;
            return true;
        }
    }
    if (s_card && ::remove(path_of(clean_name(name)).c_str()) == 0) {
        ++s_rev;
        return true;
    }
    return false;
}

// ---- uploads ------------------------------------------------------------------------------

Upload *upload_begin(const std::string &raw_name, uint32_t size, std::string &err) {
    StorageInfo si = storage();
    std::lock_guard<std::recursive_mutex> g(s_mx);
    if (s_uploading) {
        err = "Another upload is in progress";
        return nullptr;
    }
    std::string name = clean_name(raw_name);
    if (s_prog.state == RunState::Running && s_prog.name == name) {
        err = "That job is running - stop it or rename the file";
        return nullptr;
    }
    if (s_reading == name) {
        err = "The panel is drawing that job's preview - try again in a moment";
        return nullptr;
    }
    if (si.sd && !s_card) si.sd = false;  // pulled out in between
    if ((uint64_t)size + 64 * 1024 > si.free) {
        err = si.sd ? "Not enough space on the SD card" : "Too big for memory - insert an SD card";
        return nullptr;
    }
    Upload *u = new Upload();
    u->name = name;
    u->sd = si.sd;
    if (u->sd) {
        u->f = fopen(TMP, "wb");
        if (!u->f) {
            delete u;
            err = "Can't write to the SD card";
            return nullptr;
        }
        setvbuf(u->f, nullptr, _IOFBF, 16 * 1024);
    } else {
        u->ram.name = name;
        u->ram.size = size;
        u->ram.data = (uint8_t *)heap_caps_malloc(size ? size : 1, MALLOC_CAP_SPIRAM);
        if (!u->ram.data) {
            delete u;
            err = "Out of memory";
            return nullptr;
        }
    }
    s_uploading = true;
    return u;
}

bool upload_write(Upload *u, const void *data, size_t len) {
    if (u->sd) return fwrite(data, 1, len, u->f) == len;
    if (u->pos + len > u->ram.size) return false;
    memcpy(u->ram.data + u->pos, data, len);
    u->pos += len;
    return true;
}

bool upload_end(Upload *u, bool keep) {
    std::lock_guard<std::recursive_mutex> g(s_mx);
    bool ok = keep;
    if (u->sd) {
        ok = fclose(u->f) == 0 && ok;
        if (ok) {
            std::string dst = path_of(u->name);
            ::remove(dst.c_str());
            ok = rename(TMP, dst.c_str()) == 0;
        }
        if (!ok) ::remove(TMP);
    } else if (ok && u->pos == u->ram.size) {
        if (RamJob *old = ram_find_locked(u->name)) {
            heap_caps_free(old->data);
            *old = u->ram;
        } else {
            s_ram.push_back(u->ram);
        }
    } else {
        heap_caps_free(u->ram.data);
        ok = false;
    }
    if (ok) ++s_rev;
    s_uploading = false;
    ESP_LOGI(TAG, "upload %s %s (%s)", u->name.c_str(), ok ? "saved" : "dropped", u->sd ? "SD" : "memory");
    delete u;
    return ok;
}

// ---- reading a job (preview) ---------------------------------------------------------------

bool read_job(const std::string &name, const std::function<bool(const char *, size_t)> &fn) {
    const uint8_t *ram = nullptr;
    uint32_t ram_size = 0;
    std::string path;
    {
        std::lock_guard<std::recursive_mutex> g(s_mx);
        if (!s_reading.empty()) return false;
        if (RamJob *j = ram_find_locked(name)) {
            ram = j->data;
            ram_size = j->size;
        } else if (s_card) {
            path = path_of(name);
        } else {
            return false;
        }
        s_reading = name;
    }
    bool ok = true;
    if (ram) {
        for (uint32_t pos = 0; ok && pos < ram_size; pos += 4096)
            ok = fn((const char *)ram + pos, std::min<uint32_t>(4096, ram_size - pos));
    } else if (FILE *f = fopen(path.c_str(), "rb")) {
        static char buf[4096];
        size_t n;
        while (ok && (n = fread(buf, 1, sizeof(buf), f)) > 0) ok = fn(buf, n);
        fclose(f);
    } else {
        ok = false;
    }
    std::lock_guard<std::recursive_mutex> g(s_mx);
    s_reading.clear();
    return ok;
}

// ---- running a job ----------------------------------------------------------------------------

// Strip comments and spaces; grblHAL ignores them and they cost serial time
static void clean_line(const std::string &in, std::string &out) {
    out.clear();
    bool paren = false;
    for (char c : in) {
        if (paren) {
            if (c == ')') paren = false;
            continue;
        }
        if (c == '(') {
            paren = true;
            continue;
        }
        if (c == ';') break;
        if (c == ' ' || c == '\t' || c == '\r') continue;
        out.push_back(c);
    }
    if (!out.empty() && out[0] == '%') out.clear();
}

static void finish(RunState st, const char *msg) {
    std::lock_guard<std::recursive_mutex> g(s_mx);
    s_prog.state = st;
    s_prog.msg = msg;
    s_prog.errors = grbl::job_errors();
    s_prog.elapsed_s = (now_ms() - s_start_ms) / 1000;
    s_run_ram = nullptr;
    s_run_path.clear();
    grbl::job_end();
    ESP_LOGI(TAG, "job %s: %s", s_prog.name.c_str(), msg);
}

static const char *abort_reason() {
    if (s_stop_req) return "Stopped from the panel";
    if (ma_sensor::get().fault) return "Stopped - tube current fault";
    grbl::Status st = grbl::status();
    if (st.state == grbl::State::Alarm) return "Stopped - grblHAL alarm";
    return "Stopped - grblHAL was reset";
}

// false = abort the job
static bool send_line(const std::string &line) {
    while (true) {
        if (s_stop_req) return false;
        switch (grbl::job_send(line)) {
            case grbl::SendResult::Sent: return true;
            case grbl::SendResult::Aborted: return false;
            case grbl::SendResult::Full: vTaskDelay(1); break;
        }
    }
}

static void run_one() {
    FILE *f = nullptr;
    const uint8_t *ram = s_run_ram;
    uint32_t total, ram_pos = 0;
    {
        std::lock_guard<std::recursive_mutex> g(s_mx);
        total = s_prog.total;
        if (!ram) {
            f = fopen(s_run_path.c_str(), "rb");
            if (!f) {
                finish(RunState::Failed, "Couldn't open the file");
                return;
            }
        }
    }

    static char buf[8192];
    std::string raw, line;
    raw.reserve(512);
    uint32_t sent = 0, lines = 0;
    bool ok = true, too_long = false, read_err = false;
    while (ok) {
        size_t n;
        if (ram) {
            n = std::min<uint32_t>(sizeof(buf), total - ram_pos);
            memcpy(buf, ram + ram_pos, n);
            ram_pos += n;
        } else {
            n = fread(buf, 1, sizeof(buf), f);
            if (n == 0 && ferror(f)) read_err = true;
        }
        if (n == 0) {
            if (!raw.empty()) {  // last line without a newline
                clean_line(raw, line);
                if (line.size() > MAX_LINE) {
                    too_long = true;
                    ok = false;
                } else if (!line.empty()) {
                    ok = send_line(line);
                }
            }
            break;
        }
        for (size_t i = 0; i < n && ok; ++i) {
            char c = buf[i];
            ++sent;
            if (c != '\n') {
                if (raw.size() < 1024) raw.push_back(c);
                continue;
            }
            clean_line(raw, line);
            raw.clear();
            if (line.empty()) continue;
            if (line.size() > MAX_LINE) {
                too_long = true;
                ok = false;
                break;
            }
            ok = send_line(line);
            ++lines;
            if ((lines & 15) == 0) {
                std::lock_guard<std::recursive_mutex> g(s_mx);
                s_prog.sent = sent;
                s_prog.lines = lines;
            }
        }
    }
    if (f) fclose(f);
    {
        std::lock_guard<std::recursive_mutex> g(s_mx);
        s_prog.sent = sent;
        s_prog.lines = lines;
    }

    if (read_err || too_long) {
        grbl::panel_stop();  // laser off; don't leave a half-sent job running
        finish(RunState::Failed, read_err ? "Couldn't read the file (SD card removed?)"
                                          : "A line is longer than grblHAL accepts");
        return;
    }
    if (!ok) {
        finish(RunState::Stopped, abort_reason());
        return;
    }

    // Everything sent; wait for grblHAL to acknowledge and finish moving
    int idle_polls = 0;
    while (true) {
        if (s_stop_req || grbl::job_aborted()) {
            finish(RunState::Stopped, abort_reason());
            return;
        }
        grbl::Status st = grbl::status();
        if (grbl::job_acked() && st.state == grbl::State::Idle) {
            if (++idle_polls >= 4) break;  // ~0.4 s idle after the last ok
        } else {
            idle_polls = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    finish(RunState::Done, grbl::job_errors() ? "Finished with G-code errors (see Alarms)" : "Finished");
}

static void runner_task(void *) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        run_one();
    }
}

static void runner_start() {
    if (!s_task) xTaskCreatePinnedToCore(runner_task, "job_run", 6144, nullptr, 5, &s_task, 0);
}

bool start(const std::string &name, std::string &err) {
    std::lock_guard<std::recursive_mutex> g(s_mx);
    if (s_prog.state == RunState::Running) {
        err = "A job is already running";
        return false;
    }
    grbl::Status st = grbl::status();
    if (!st.link_ok) {
        err = "No link to grblHAL";
        return false;
    }
    if (ma_sensor::get().fault) {
        err = "Clear the tube current fault first";
        return false;
    }
    if (st.hold_pin) {
        err = "Lid is open";
        return false;
    }
    if (st.probe_pin) {
        err = "No water flow";
        return false;
    }
    if (st.job_active) {
        err = "LightBurn is running a job";
        return false;
    }
    if (st.state != grbl::State::Idle) {
        err = st.state == grbl::State::Alarm ? "grblHAL is in alarm - unlock or home first"
                                             : "grblHAL must be Idle";
        return false;
    }

    uint32_t size = 0;
    s_run_ram = nullptr;
    s_run_path.clear();
    if (RamJob *j = ram_find_locked(name)) {
        s_run_ram = j->data;
        size = j->size;
    } else {
        struct stat fs;
        std::string p = path_of(name);
        if (!s_card || stat(p.c_str(), &fs) != 0) {
            err = "File not found";
            return false;
        }
        s_run_path = p;
        size = (uint32_t)fs.st_size;
    }
    if (!grbl::job_begin()) {
        err = "grblHAL is busy - try again";
        return false;
    }
    s_stop_req = false;
    s_prog = Progress();
    s_prog.state = RunState::Running;
    s_prog.name = name;
    s_prog.total = size;
    s_start_ms = now_ms();
    xTaskNotifyGive(s_task);
    return true;
}

void stop() {
    s_stop_req = true;
    grbl::panel_stop();
}

Progress progress() {
    std::lock_guard<std::recursive_mutex> g(s_mx);
    Progress p = s_prog;
    if (p.state == RunState::Running) {
        p.elapsed_s = (now_ms() - s_start_ms) / 1000;
        p.errors = grbl::job_errors();
    }
    return p;
}

bool running() {
    std::lock_guard<std::recursive_mutex> g(s_mx);
    return s_prog.state == RunState::Running;
}

}  // namespace jobs
