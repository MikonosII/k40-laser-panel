#include "display.hpp"

#include "board.hpp"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include "settings.hpp"

static const char *TAG = "display";

namespace display {

static esp_lcd_panel_handle_t s_panel = nullptr;
static esp_lcd_touch_handle_t s_touch = nullptr;
static SemaphoreHandle_t s_lvgl_mutex = nullptr;
static uint8_t *s_rot_buf = nullptr;  // scratch for software rotation
static lv_display_t *s_disp = nullptr;

// Partial draw buffers: 480 px wide (logical) x this many rows
static constexpr int DRAW_ROWS = 60;
static constexpr size_t DRAW_BUF_BYTES = board::LCD_V_RES * DRAW_ROWS * 2;

bool lock(int timeout_ms) {
    TickType_t t = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mutex, t) == pdTRUE;
}

void unlock() { xSemaphoreGiveRecursive(s_lvgl_mutex); }

void set_flipped(bool flipped) {
    // menuconfig picks the "normal" portrait direction; Flip turns it 180 degrees
#if CONFIG_K40_ROTATION_270
    bool r270 = !flipped;
#else
    bool r270 = flipped;
#endif
    if (s_disp) lv_display_set_rotation(s_disp, r270 ? LV_DISPLAY_ROTATION_270 : LV_DISPLAY_ROTATION_90);
}

static uint32_t tick_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }

// LVGL renders portrait; rotate each dirty area back to the panel's landscape
// framebuffer (pattern from the LVGL 9 display-rotation docs).
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    lv_display_rotation_t rot = lv_display_get_rotation(disp);
    lv_area_t a = *area;
    if (rot != LV_DISPLAY_ROTATION_0) {
        lv_color_format_t cf = lv_display_get_color_format(disp);
        lv_display_rotate_area(disp, &a);
        uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
        uint32_t dst_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&a), cf);
        lv_draw_sw_rotate(px_map, s_rot_buf, lv_area_get_width(area), lv_area_get_height(area),
                          src_stride, dst_stride, rot, cf);
        px_map = s_rot_buf;
    }
#if CONFIG_K40_SWAP_COLOR_BYTES
    lv_draw_sw_rgb565_swap(px_map, lv_area_get_size(&a));
#endif
    // RGB panel draw_bitmap copies into the framebuffer synchronously
    esp_lcd_panel_draw_bitmap(s_panel, a.x1, a.y1, a.x2 + 1, a.y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

// GT911 reports raw landscape coordinates; LVGL 9 applies the display
// rotation to pointer input itself.
static void touch_read_cb(lv_indev_t *, lv_indev_data_t *data) {
    esp_lcd_touch_point_data_t pt = {};
    uint8_t count = 0;
    if (!s_touch) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    esp_lcd_touch_read_data(s_touch);
    if (esp_lcd_touch_get_data(s_touch, &pt, &count, 1) == ESP_OK && count > 0) {
        data->point.x = pt.x;
        data->point.y = pt.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void init_panel() {
    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.data_width = 16;
    cfg.bits_per_pixel = 16;
    cfg.dma_burst_size = 64;
    cfg.num_fbs = 1;
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    cfg.disp_gpio_num = GPIO_NUM_NC;
    cfg.pclk_gpio_num = board::LCD_PCLK;
    cfg.vsync_gpio_num = board::LCD_VSYNC;
    cfg.hsync_gpio_num = board::LCD_HSYNC;
    cfg.de_gpio_num = board::LCD_DE;
    for (int i = 0; i < 16; ++i) cfg.data_gpio_nums[i] = board::LCD_DATA[i];

    // Timings from Elecrow's P4 course code (bsp_illuminate.h)
    cfg.timings.pclk_hz = CONFIG_K40_PCLK_HZ;
    cfg.timings.h_res = board::LCD_H_RES;
    cfg.timings.v_res = board::LCD_V_RES;
    cfg.timings.hsync_pulse_width = 4;
    cfg.timings.hsync_back_porch = 8;
    cfg.timings.hsync_front_porch = 8;
    cfg.timings.vsync_pulse_width = 4;
    cfg.timings.vsync_back_porch = 16;
    cfg.timings.vsync_front_porch = 16;
    cfg.timings.flags.pclk_active_neg = 1;
    cfg.timings.flags.pclk_idle_high = 1;
    cfg.flags.fb_in_psram = 1;

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
}

static void init_touch() {
    // GT911 answers on 0x5D or 0x14 depending on INT at reset; try both like Elecrow does
    const uint8_t addrs[2] = {ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP};
    for (uint8_t addr : addrs) {
        // Same values as ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG(), which isn't valid C++
        esp_lcd_panel_io_i2c_config_t io_cfg = {};
        io_cfg.dev_addr = addr;
        io_cfg.control_phase_bytes = 1;
        io_cfg.dc_bit_offset = 0;
        io_cfg.lcd_cmd_bits = 16;
        io_cfg.flags.disable_control_phase = 1;
        io_cfg.scl_speed_hz = 400000;
        esp_lcd_panel_io_handle_t io = nullptr;
        if (esp_lcd_new_panel_io_i2c(board::i2c_bus(), &io_cfg, &io) != ESP_OK) continue;

        static uint32_t dev_addr;
        dev_addr = addr;
        esp_lcd_touch_config_t tp = {};
        tp.x_max = board::LCD_H_RES;
        tp.y_max = board::LCD_V_RES;
        tp.rst_gpio_num = board::TOUCH_RST;
        tp.int_gpio_num = board::TOUCH_INT;
        tp.driver_data = &dev_addr;
        if (esp_lcd_touch_new_i2c_gt911(io, &tp, &s_touch) == ESP_OK) {
            ESP_LOGI(TAG, "GT911 at 0x%02x", addr);
            return;
        }
        esp_lcd_panel_io_del(io);
    }
    ESP_LOGE(TAG, "GT911 touch not found");
}

void init() {
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    init_panel();
    init_touch();

    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_display_create(board::LCD_H_RES, board::LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    void *buf1 = heap_caps_aligned_alloc(64, DRAW_BUF_BYTES, MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_aligned_alloc(64, DRAW_BUF_BYTES, MALLOC_CAP_SPIRAM);
    s_rot_buf = (uint8_t *)heap_caps_aligned_alloc(64, DRAW_BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_rot_buf) s_rot_buf = (uint8_t *)heap_caps_aligned_alloc(64, DRAW_BUF_BYTES, MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2 && s_rot_buf);
    lv_display_set_buffers(disp, buf1, buf2, DRAW_BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    s_disp = disp;
    set_flipped(settings::get().flipped);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_display(indev, disp);

    // Backlight on only once there's something to show
    board::backlight((uint8_t)settings::get().brightness);
}

static void lvgl_task(void *) {
    while (true) {
        uint32_t wait = 10;
        if (lock(50)) {
            wait = lv_timer_handler();
            unlock();
        }
        if (wait < 5) wait = 5;
        if (wait > 30) wait = 30;
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

void start() {
    // Core 1: keep rendering off the Wi-Fi core
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, nullptr, 4, nullptr, 1);
}

}  // namespace display
