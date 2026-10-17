#include "preview.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "display.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jobs.hpp"

static const char *TAG = "preview";

namespace preview {

static constexpr float BED_X = 300, BED_Y = 200;  // stock K40 bed, shown when no job is picked
static constexpr int MARGIN = 8;

static uint16_t *s_front = nullptr;  // what LVGL shows
static uint16_t *s_back = nullptr;   // what the task draws into
static lv_image_dsc_t s_dsc;
static std::mutex s_mx;
static View s_view;
static uint32_t s_rev = 0;
static std::string s_req;
static bool s_has_req = false;
static TaskHandle_t s_task = nullptr;

static uint16_t rgb(uint32_t hex) { return lv_color_to_u16(lv_color_hex(hex)); }
static uint16_t s_bg, s_grid, s_edge;

static uint16_t mix(uint32_t a, uint32_t b, float t) {
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    int g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
    int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    return rgb((uint32_t)(r << 16 | g << 8 | bl));
}

// ---- drawing --------------------------------------------------------------------------

static void plot(int x, int y, uint16_t c) {
    if (x >= 0 && x < W && y >= 0 && y < H) s_back[y * W + x] = c;
}

static void line_px(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 4 * (W + H); ++guard) {
        plot(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

bool to_px(const View &v, float x, float y, int *px, int *py) {
    float fx = v.off_x + (x - v.min_x) * v.scale;
    float fy = H - (v.off_y + (y - v.min_y) * v.scale);
    *px = (int)lroundf(fx);
    *py = (int)lroundf(fy);
    return *px >= 0 && *px < W && *py >= 0 && *py < H;
}

static void fit(View &v, float minx, float miny, float maxx, float maxy) {
    v.size_x = maxx - minx;
    v.size_y = maxy - miny;
    float sx = std::max(v.size_x, 1.0f), sy = std::max(v.size_y, 1.0f);
    v.scale = std::min((W - 2 * MARGIN) / sx, (H - 2 * MARGIN) / sy);
    v.min_x = minx;
    v.min_y = miny;
    v.off_x = (W - v.size_x * v.scale) / 2;
    v.off_y = (H - v.size_y * v.scale) / 2;
}

// Faint grid every 10 or 50 mm so the size reads at a glance
static void grid(const View &v) {
    float step = std::max(v.size_x, v.size_y) > 120 ? 50 : 10;
    for (float g = std::ceil(v.min_x / step) * step; g <= v.min_x + v.size_x + 0.01f; g += step) {
        int x0, y0, x1, y1;
        to_px(v, g, v.min_y, &x0, &y0);
        to_px(v, g, v.min_y + v.size_y, &x1, &y1);
        line_px(x0, y0, x1, y1, s_grid);
    }
    for (float g = std::ceil(v.min_y / step) * step; g <= v.min_y + v.size_y + 0.01f; g += step) {
        int x0, y0, x1, y1;
        to_px(v, v.min_x, g, &x0, &y0);
        to_px(v, v.min_x + v.size_x, g, &x1, &y1);
        line_px(x0, y0, x1, y1, s_grid);
    }
}

static void outline(const View &v) {
    int x0, y0, x1, y1;
    to_px(v, v.min_x, v.min_y, &x0, &y0);
    to_px(v, v.min_x + v.size_x, v.min_y + v.size_y, &x1, &y1);
    line_px(x0, y0, x1, y0, s_edge);
    line_px(x1, y0, x1, y1, s_edge);
    line_px(x1, y1, x0, y1, s_edge);
    line_px(x0, y1, x0, y0, s_edge);
}

static void clear() {
    for (int i = 0; i < W * H; ++i) s_back[i] = s_bg;
}

static void publish(const View &v) {
    display::lock();
    memcpy(s_front, s_back, W * H * 2);
    display::unlock();
    std::lock_guard<std::mutex> g(s_mx);
    s_view = v;
    ++s_rev;
}

// ---- G-code walking -------------------------------------------------------------------------

struct Gc {
    bool abs = true;
    float unit = 1;  // mm per G-code unit
    int motion = 0;  // 0..3
    bool spindle = false;
    float s = 0;
    float x = 0, y = 0;
};

// Calls seg(x0, y0, x1, y1, burn, s) for each straight piece of motion on this line
template <typename F>
static void walk_line(const char *p, Gc &g, F &&seg) {
    bool hx = false, hy = false, hi = false, hj = false, skip_move = false;
    float vx = 0, vy = 0, vi = 0, vj = 0;
    while (*p) {
        char c = (char)toupper((unsigned char)*p);
        if (c == ';') break;
        if (c == '(') {
            while (*p && *p != ')') ++p;
            if (*p) ++p;
            continue;
        }
        if (c < 'A' || c > 'Z') {
            ++p;
            continue;
        }
        char *end;
        float n = strtof(p + 1, &end);
        if (end == p + 1) {
            ++p;
            continue;
        }
        p = end;
        switch (c) {
            case 'G': {
                int gi = (int)lroundf(n * 10);
                if (gi == 0 || gi == 10 || gi == 20 || gi == 30) g.motion = gi / 10;
                else if (gi == 900) g.abs = true;
                else if (gi == 910) g.abs = false;
                else if (gi == 200) g.unit = 25.4f;
                else if (gi == 210) g.unit = 1;
                else if (gi == 280 || gi == 300 || gi == 530 || gi == 920 || gi == 40) skip_move = true;
                break;
            }
            case 'M': {
                int m = (int)lroundf(n);
                if (m == 3 || m == 4) g.spindle = true;
                else if (m == 5 || m == 2 || m == 30) g.spindle = false;
                break;
            }
            case 'S': g.s = n; break;
            case 'X': hx = true; vx = n; break;
            case 'Y': hy = true; vy = n; break;
            case 'I': hi = true; vi = n; break;
            case 'J': hj = true; vj = n; break;
            default: break;
        }
    }
    if (skip_move || !(hx || hy)) return;
    float nx = hx ? (g.abs ? vx * g.unit : g.x + vx * g.unit) : g.x;
    float ny = hy ? (g.abs ? vy * g.unit : g.y + vy * g.unit) : g.y;
    bool burn = g.spindle && g.s > 0 && g.motion != 0;
    if ((g.motion == 2 || g.motion == 3) && (hi || hj)) {
        float cx = g.x + vi * g.unit, cy = g.y + vj * g.unit;
        float r = hypotf(g.x - cx, g.y - cy);
        float a0 = atan2f(g.y - cy, g.x - cx), a1 = atan2f(ny - cy, nx - cx);
        float sweep = a1 - a0;
        if (g.motion == 2) {  // clockwise
            if (sweep >= 0) sweep -= 2 * (float)M_PI;
        } else if (sweep <= 0) {
            sweep += 2 * (float)M_PI;
        }
        int n = std::max(2, (int)(fabsf(sweep) / 0.15f));
        float px = g.x, py = g.y;
        for (int k = 1; k <= n; ++k) {
            float a = a0 + sweep * k / n;
            float qx = k == n ? nx : cx + r * cosf(a), qy = k == n ? ny : cy + r * sinf(a);
            seg(px, py, qx, qy, burn, g.s);
            px = qx;
            py = qy;
        }
    } else {
        seg(g.x, g.y, nx, ny, burn, g.s);
    }
    g.x = nx;
    g.y = ny;
}

// Run walk_line over a whole job; false if reading failed or a newer request arrived
template <typename F>
static bool walk_job(const std::string &name, F &&seg) {
    Gc g;
    std::string line;
    bool ok = jobs::read_job(name, [&](const char *d, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (d[i] == '\n') {
                walk_line(line.c_str(), g, seg);
                line.clear();
            } else if (d[i] != '\r' && line.size() < 300) {
                line.push_back(d[i]);
            }
        }
        std::lock_guard<std::mutex> lk(s_mx);
        return !s_has_req;  // a newer request wins
    });
    if (ok && !line.empty()) walk_line(line.c_str(), g, seg);
    return ok;
}

static void draw_bed() {
    View v;
    fit(v, 0, 0, BED_X, BED_Y);
    clear();
    grid(v);
    outline(v);
    publish(v);
}

static void draw_job(const std::string &name) {
    {
        std::lock_guard<std::mutex> g(s_mx);
        s_view.name = name;
        s_view.busy = true;
        s_view.msg.clear();
        ++s_rev;
    }
    // Pass 1: extents of the burning moves (or of all moves if nothing burns) and the top S
    float bmin[2] = {1e9f, 1e9f}, bmax[2] = {-1e9f, -1e9f};
    float amin[2] = {1e9f, 1e9f}, amax[2] = {-1e9f, -1e9f};
    float smax = 0;
    long burns = 0;
    auto grow = [](float *mn, float *mx, float x, float y) {
        mn[0] = std::min(mn[0], x); mn[1] = std::min(mn[1], y);
        mx[0] = std::max(mx[0], x); mx[1] = std::max(mx[1], y);
    };
    bool ok = walk_job(name, [&](float x0, float y0, float x1, float y1, bool burn, float s) {
        grow(amin, amax, x0, y0);
        grow(amin, amax, x1, y1);
        if (burn) {
            grow(bmin, bmax, x0, y0);
            grow(bmin, bmax, x1, y1);
            smax = std::max(smax, s);
            ++burns;
        }
    });
    View v;
    v.name = name;
    if (!ok) {
        std::lock_guard<std::mutex> g(s_mx);
        if (!s_has_req) {  // a real failure, not a newer request
            s_view.busy = false;
            s_view.msg = "Couldn't read the file";
            ++s_rev;
        }
        return;
    }
    bool any = amax[0] >= amin[0];
    if (!any) {
        fit(v, 0, 0, BED_X, BED_Y);
        clear();
        grid(v);
        v.msg = "No moves found in this file";
        publish(v);
        return;
    }
    if (burns) fit(v, bmin[0], bmin[1], bmax[0], bmax[1]);
    else fit(v, amin[0], amin[1], amax[0], amax[1]);

    // Pass 2: draw. Brighter = more power.
    clear();
    grid(v);
    uint16_t lut[16];
    for (int i = 0; i < 16; ++i) lut[i] = mix(0x14171A, 0xF5A524, 0.35f + 0.65f * i / 15.0f);
    uint16_t travel = mix(0x14171A, 0x5F5E5A, 0.6f);
    ok = walk_job(name, [&](float x0, float y0, float x1, float y1, bool burn, float s) {
        if (burns && !burn) return;
        int a, b, c, d;
        to_px(v, x0, y0, &a, &b);
        to_px(v, x1, y1, &c, &d);
        uint16_t col = burns ? lut[std::min(15, (int)(15 * s / std::max(smax, 1.0f)))] : travel;
        line_px(a, b, c, d, col);
    });
    if (!ok) return;
    if (!burns) v.msg = "No laser moves found (laser never on)";
    publish(v);
}

static void task(void *) {
    draw_bed();
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (true) {
            std::string name;
            {
                std::lock_guard<std::mutex> g(s_mx);
                if (!s_has_req) break;
                name = s_req;
                s_has_req = false;
            }
            if (name.empty()) draw_bed();
            else draw_job(name);
        }
    }
}

// ---- public -------------------------------------------------------------------------------------

void init() {
    s_front = (uint16_t *)heap_caps_malloc(W * H * 2, MALLOC_CAP_SPIRAM);
    s_back = (uint16_t *)heap_caps_malloc(W * H * 2, MALLOC_CAP_SPIRAM);
    if (!s_front || !s_back) {
        ESP_LOGE(TAG, "no memory for the preview");
        return;
    }
    s_bg = rgb(0x14171A);
    s_grid = rgb(0x22262B);
    s_edge = rgb(0x343941);
    for (int i = 0; i < W * H; ++i) s_front[i] = s_bg;
    memset(&s_dsc, 0, sizeof(s_dsc));
    s_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_dsc.header.w = W;
    s_dsc.header.h = H;
    s_dsc.header.stride = W * 2;
    s_dsc.data_size = W * H * 2;
    s_dsc.data = (const uint8_t *)s_front;
    xTaskCreatePinnedToCore(task, "preview", 6144, nullptr, 2, &s_task, 0);
}

void request(const std::string &job_name, bool force) {
    if (!s_task) return;
    {
        std::lock_guard<std::mutex> g(s_mx);
        if (!force && !s_has_req && s_view.name == job_name && !s_view.busy) return;  // already showing it
        s_req = job_name;
        s_has_req = true;
    }
    xTaskNotifyGive(s_task);
}

View view() {
    std::lock_guard<std::mutex> g(s_mx);
    return s_view;
}

const lv_image_dsc_t *image() { return s_front ? &s_dsc : nullptr; }

uint32_t revision() {
    std::lock_guard<std::mutex> g(s_mx);
    return s_rev;
}

}  // namespace preview
