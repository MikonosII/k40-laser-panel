// Four portrait screens (480x800): Status, Jog, Laser, Alarms.
// Layout and colors follow the "K40 laser panel" mockup.
#include "ui.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "board.hpp"
#include "display.hpp"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "grbl_link.hpp"
#include "lvgl.h"
#include "net_bridge.hpp"
#include "sdkconfig.h"
#include "settings.hpp"

namespace ui {

// ---- palette ----------------------------------------------------------------
static lv_color_t C(uint32_t hex) { return lv_color_hex(hex); }
#define COL_BG 0x0F1113
#define COL_CARD 0x1A1D21
#define COL_BTN 0x23272D
#define COL_LINE 0x262A30
#define COL_EDGE 0x343941
#define COL_TEXT 0xE8E6E1
#define COL_SUB 0x9A978F
#define COL_SOFT 0xC9C6BF
#define COL_AMBER 0xF5A524
#define COL_AMBER_BG 0x3A2E12
#define COL_AMBER_TX 0xF5C060
#define COL_GREEN_BG 0x1E3A28
#define COL_GREEN 0x7BE0A0
#define COL_RED_BG 0x4A1C1C
#define COL_RED 0xFF9A9A
#define COL_RED_CARD 0x2A1515
#define COL_BLUE_BG 0x142B35
#define COL_BLUE 0x8FD3EE
#define COL_BLUE_EDGE 0x4FB3D9
#define COL_GRAY_BG 0x2A2D33

enum Screen { SCR_STATUS, SCR_JOG, SCR_LASER, SCR_ALARMS, SCR_SETUP, SCR_COUNT };

struct Header {
    lv_obj_t *badge, *badge_lbl, *pos, *link, *dot;
};

static lv_obj_t *s_scr[SCR_COUNT];
static Header s_hdr[SCR_COUNT];
static lv_obj_t *s_toast;
static lv_timer_t *s_toast_timer;

// Status
static lv_obj_t *s_job_time, *s_job_sub, *s_speed, *s_power, *s_work, *s_sval;
static lv_obj_t *s_feed_ov, *s_pwr_ov, *s_air_lbl[2], *s_air_btn[2];
// Jog
static const float STEPS[] = {0.1f, 1.f, 10.f, 50.f};
static const int SPEEDS[] = {1000, 3000, 6000};
static int s_step_i = 2, s_speed_i = 1;
static lv_obj_t *s_step_btn[4], *s_speed_btn[3], *s_ptr_btn, *s_ptr_lbl;
static bool s_ptr_on = false;
static lv_timer_t *s_ptr_timer;
// Laser
static const int DUR_MS[] = {50, 200, 1000};
static int s_test_pct = 10, s_dur_i = 1;
static bool s_armed = false, s_exhaust = false;
static lv_obj_t *s_test_lbl, *s_dur_btn[3], *s_arm_btn, *s_arm_lbl, *s_fire_btn;
static lv_obj_t *s_lid_lbl, *s_water_lbl, *s_cap_lbl;
static lv_obj_t *s_exh_lbl;
static lv_timer_t *s_arm_timer;
// Alarms
static lv_obj_t *s_alarm_card, *s_alarm_title, *s_alarm_desc, *s_console;
static int s_last_alarm = 0;

// ---- small builders ---------------------------------------------------------

static lv_obj_t *box(lv_obj_t *parent, lv_flex_flow_t flow, int gap) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_style_pad_row(o, gap, 0);
    lv_obj_set_style_pad_column(o, gap, 0);
    lv_obj_set_size(o, LV_PCT(100), LV_SIZE_CONTENT);
    return o;
}

static lv_obj_t *card(lv_obj_t *parent, uint32_t bg = COL_CARD) {
    lv_obj_t *o = box(parent, LV_FLEX_FLOW_COLUMN, 10);
    lv_obj_set_style_bg_color(o, C(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 10, 0);
    lv_obj_set_style_pad_all(o, 14, 0);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, C(color), 0);
    return l;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, uint32_t bg, uint32_t fg,
                        const lv_font_t *font, lv_event_cb_t cb, void *ud = nullptr,
                        uint32_t edge = 0, lv_obj_t **lbl_out = nullptr) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_style_bg_color(b, C(bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, edge ? 1 : 0, 0);
    if (edge) lv_obj_set_style_border_color(b, C(edge), 0);
    lv_obj_t *l = label(b, text, font, fg);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    if (lbl_out) *lbl_out = l;
    return b;
}

static void set_btn_colors(lv_obj_t *b, uint32_t bg, uint32_t fg, uint32_t edge) {
    lv_obj_set_style_bg_color(b, C(bg), 0);
    lv_obj_set_style_border_width(b, edge ? 1 : 0, 0);
    if (edge) lv_obj_set_style_border_color(b, C(edge), 0);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    if (l) lv_obj_set_style_text_color(l, C(fg), 0);
}

static void select_style(lv_obj_t *b, bool on) {
    if (on)
        set_btn_colors(b, COL_AMBER_BG, COL_AMBER_TX, COL_AMBER);
    else
        set_btn_colors(b, COL_CARD, COL_SOFT, COL_EDGE);
}

static void toast(const char *msg) {
    lv_label_set_text(s_toast, msg);
    lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(s_toast_timer);
    lv_timer_resume(s_toast_timer);
}

static void toast_hide(lv_timer_t *t) {
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(t);
}

// Send a panel line, explaining why when it isn't allowed right now
static bool panel_send(const char *line) {
    if (grbl::send(line)) return true;
    grbl::Status st = grbl::status();
    if (!st.link_ok)
        toast("No link to grblHAL");
    else if (st.job_active)
        toast("Job running - use LightBurn");
    else
        toast("Busy - try again");
    return false;
}

// ---- shared chrome: header + bottom nav --------------------------------------

// ---- PIN lock -----------------------------------------------------------------
// When a PIN is set, opening Setup and arming test fire ask for it. A correct PIN
// unlocks both for 5 minutes. Forgotten PIN: put reset_pin.txt on the SD card and reboot.

static lv_obj_t *s_pin_overlay, *s_pin_ta, *s_pin_msg;
static void (*s_pin_action)() = nullptr;
static uint32_t s_unlocked_until = 0;
static constexpr uint32_t UNLOCK_MS = 5 * 60 * 1000;

static bool pin_unlocked() {
    if (settings::get().pin.empty()) return true;
    return s_unlocked_until && (int32_t)(s_unlocked_until - lv_tick_get()) > 0;
}

static void pin_close() {
    lv_obj_add_flag(s_pin_overlay, LV_OBJ_FLAG_HIDDEN);
    s_pin_action = nullptr;
}

static void pin_check() {
    if (settings::get().pin == lv_textarea_get_text(s_pin_ta)) {
        s_unlocked_until = lv_tick_get() + UNLOCK_MS;
        void (*action)() = s_pin_action;
        pin_close();
        if (action) action();
    } else {
        lv_label_set_text(s_pin_msg, "Wrong PIN");
        lv_textarea_set_text(s_pin_ta, "");
    }
}

static void pin_kb_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CANCEL) pin_close();
    else if (code == LV_EVENT_READY) pin_check();
}

static void pin_ta_cb(lv_event_t *) {
    // Check as soon as the 4th digit goes in
    if (strlen(lv_textarea_get_text(s_pin_ta)) == 4) pin_check();
}

static void pin_build() {
    s_pin_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_pin_overlay);
    lv_obj_set_size(s_pin_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_pin_overlay, C(0x000000), 0);
    lv_obj_set_style_bg_opa(s_pin_overlay, LV_OPA_80, 0);
    lv_obj_add_flag(s_pin_overlay, LV_OBJ_FLAG_CLICKABLE);  // swallow taps behind the dialog
    lv_obj_set_flex_flow(s_pin_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_pin_overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(s_pin_overlay, 120, 0);

    lv_obj_t *c = card(s_pin_overlay);
    lv_obj_set_width(c, 360);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(c, "Enter PIN", &lv_font_montserrat_24, COL_TEXT);
    s_pin_ta = lv_textarea_create(c);
    lv_textarea_set_one_line(s_pin_ta, true);
    lv_textarea_set_password_mode(s_pin_ta, true);
    lv_textarea_set_accepted_chars(s_pin_ta, "0123456789");
    lv_textarea_set_max_length(s_pin_ta, 4);
    lv_obj_set_width(s_pin_ta, 200);
    lv_obj_set_style_text_font(s_pin_ta, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_align(s_pin_ta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(s_pin_ta, C(0x14171A), 0);
    lv_obj_set_style_text_color(s_pin_ta, C(COL_TEXT), 0);
    lv_obj_set_style_border_color(s_pin_ta, C(COL_AMBER), 0);
    lv_obj_add_event_cb(s_pin_ta, pin_ta_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    s_pin_msg = label(c, "", &lv_font_montserrat_16, COL_RED);

    lv_obj_t *kb = lv_keyboard_create(s_pin_overlay);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(kb, LV_PCT(100), 300);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    static const char *pin_map[] = {"1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
                                    "4", "5", "6", LV_SYMBOL_OK, "\n",
                                    "7", "8", "9", LV_SYMBOL_CLOSE, "\n",
                                    "0", ""};
    static lv_buttonmatrix_ctrl_t pin_ctrl[13];
    for (int i = 0; i < 13; ++i) pin_ctrl[i] = (lv_buttonmatrix_ctrl_t)(i == 7 ? (1 | LV_BUTTONMATRIX_CTRL_CHECKED) : 1);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, pin_map, pin_ctrl);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
    lv_keyboard_set_textarea(kb, s_pin_ta);
    lv_obj_set_style_bg_color(kb, C(COL_CARD), 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 6, 0);
    lv_obj_set_style_pad_gap(kb, 6, 0);
    lv_obj_set_style_bg_color(kb, C(COL_BTN), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, C(COL_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_24, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, C(COL_AMBER_BG), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(kb, C(COL_AMBER_TX), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_event_cb(kb, pin_kb_cb, LV_EVENT_ALL, nullptr);
    lv_obj_add_flag(s_pin_overlay, LV_OBJ_FLAG_HIDDEN);
}

// Run action now if unlocked, otherwise after the right PIN is entered
static void require_pin(void (*action)()) {
    if (pin_unlocked()) {
        action();
        return;
    }
    if (!s_pin_overlay) pin_build();
    lv_textarea_set_text(s_pin_ta, "");
    lv_label_set_text(s_pin_msg, "");
    s_pin_action = action;
    lv_obj_remove_flag(s_pin_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_pin_overlay);
}

// ---- screen sleep -----------------------------------------------------------------

static lv_obj_t *s_sleep_overlay;
static bool s_asleep = false;

static void wake() {
    if (!s_asleep) return;
    s_asleep = false;
    lv_obj_add_flag(s_sleep_overlay, LV_OBJ_FLAG_HIDDEN);
    board::backlight((uint8_t)settings::get().brightness);
}

static void sleep_now() {
    s_asleep = true;
    lv_obj_remove_flag(s_sleep_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_sleep_overlay);
    board::backlight(0);
}

// The first tap on a sleeping screen only wakes it; it never presses a button
static void sleep_overlay_cb(lv_event_t *) { wake(); }

static void go_setup() { lv_screen_load(s_scr[SCR_SETUP]); }

static void nav_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i == SCR_SETUP && lv_screen_active() != s_scr[SCR_SETUP]) {
        require_pin(go_setup);
        return;
    }
    lv_screen_load(s_scr[i]);
}

static lv_obj_t *make_screen(Screen id) {
    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, C(COL_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_text_color(scr, C(COL_TEXT), 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    s_scr[id] = scr;

    // Header
    lv_obj_t *hdr = box(scr, LV_FLEX_FLOW_ROW, 12);
    lv_obj_set_height(hdr, 56);
    lv_obj_set_style_pad_hor(hdr, 16, 0);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_border_color(hdr, C(COL_LINE), 0);
    Header &h = s_hdr[id];
    h.badge = lv_obj_create(hdr);
    lv_obj_remove_style_all(h.badge);
    lv_obj_set_size(h.badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(h.badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(h.badge, 6, 0);
    lv_obj_set_style_pad_hor(h.badge, 14, 0);
    lv_obj_set_style_pad_ver(h.badge, 6, 0);
    h.badge_lbl = label(h.badge, "--", &lv_font_montserrat_16, COL_SOFT);
    h.pos = label(hdr, "X --  Y --", &lv_font_montserrat_16, COL_SOFT);
    lv_obj_t *spacer = box(hdr, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_width(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);
    h.link = label(hdr, "", &lv_font_montserrat_14, COL_SUB);
    h.dot = lv_obj_create(hdr);
    lv_obj_remove_style_all(h.dot);
    lv_obj_set_size(h.dot, 10, 10);
    lv_obj_set_style_radius(h.dot, 5, 0);
    lv_obj_set_style_bg_opa(h.dot, LV_OPA_COVER, 0);
    return scr;
}

static lv_obj_t *make_content(lv_obj_t *scr) {
    lv_obj_t *c = box(scr, LV_FLEX_FLOW_COLUMN, 12);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_style_pad_all(c, 16, 0);
    return c;
}

static lv_obj_t *make_nav(lv_obj_t *scr, Screen active) {
    static const char *names[SCR_COUNT] = {"Status", "Jog", "Laser", "Alarms", "Setup"};
    static const char *icons[SCR_COUNT] = {LV_SYMBOL_HOME, LV_SYMBOL_GPS, LV_SYMBOL_CHARGE,
                                           LV_SYMBOL_WARNING, LV_SYMBOL_SETTINGS};
    lv_obj_t *nav = box(scr, LV_FLEX_FLOW_ROW, 6);
    lv_obj_set_height(nav, 84);
    lv_obj_set_style_pad_all(nav, 8, 0);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(nav, 1, 0);
    lv_obj_set_style_border_color(nav, C(COL_LINE), 0);
    for (int i = 0; i < SCR_COUNT; ++i) {
        bool on = i == active;
        uint32_t fg = on ? (i == SCR_ALARMS ? COL_RED : COL_AMBER) : COL_SUB;
        char text[32];
        snprintf(text, sizeof(text), "%s\n%s", icons[i], names[i]);
        lv_obj_t *b = button(nav, text, on ? COL_BTN : COL_BG, fg, &lv_font_montserrat_14, nav_cb,
                             (void *)(intptr_t)i);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_height(b, LV_PCT(100));
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_text_align(lv_obj_get_child(b, 0), LV_TEXT_ALIGN_CENTER, 0);
    }
    return nav;
}

// ---- Status -----------------------------------------------------------------

static void rt_cb(lv_event_t *e) { grbl::realtime((uint8_t)(intptr_t)lv_event_get_user_data(e)); }
static void stop_cb(lv_event_t *) {
    grbl::panel_stop();
    toast("Stopped. Clear the job in LightBurn.");
}

static lv_obj_t *override_card(lv_obj_t *parent, const char *title, uint8_t m10, uint8_t reset,
                               uint8_t p10, lv_obj_t **value_out, uint32_t value_color) {
    lv_obj_t *c = card(parent);
    lv_obj_set_style_pad_ver(c, 12, 0);
    lv_obj_t *row = box(c, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(row, title, &lv_font_montserrat_14, COL_SUB);
    *value_out = label(row, "100%", &lv_font_montserrat_16, value_color);
    lv_obj_t *btns = box(c, LV_FLEX_FLOW_ROW, 8);
    const char *txt[3] = {"-10", "100", "+10"};
    uint8_t cmd[3] = {m10, reset, p10};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *b = button(btns, txt[i], COL_BTN, COL_TEXT, &lv_font_montserrat_20, rt_cb,
                             (void *)(intptr_t)cmd[i], COL_EDGE);
        lv_obj_set_height(b, 52);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_radius(b, 8, 0);
    }
    return c;
}

static void build_status() {
    lv_obj_t *scr = make_screen(SCR_STATUS);
    lv_obj_t *c = make_content(scr);

    lv_obj_t *job = card(c);
    lv_obj_set_flex_flow(job, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(job, 16, 0);
    lv_obj_set_flex_align(job, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *jl = box(job, LV_FLEX_FLOW_COLUMN, 4);
    lv_obj_set_width(jl, LV_SIZE_CONTENT);
    label(jl, "Job time", &lv_font_montserrat_14, COL_SUB);
    s_job_sub = label(jl, "Ready", &lv_font_montserrat_14, COL_SUB);
    s_job_time = label(job, "00:00", &lv_font_montserrat_40, COL_TEXT);

    lv_obj_t *live = card(c);
    lv_obj_set_flex_flow(live, LV_FLEX_FLOW_ROW);
    lv_obj_t *lc = box(live, LV_FLEX_FLOW_COLUMN, 4);
    lv_obj_set_width(lc, LV_PCT(50));
    label(lc, "Speed  mm/min", &lv_font_montserrat_14, COL_SUB);
    s_speed = label(lc, "0", &lv_font_montserrat_28, COL_TEXT);
    s_work = label(lc, "Work X 0.00 Y 0.00", &lv_font_montserrat_14, COL_SUB);
    lv_obj_t *rc = box(live, LV_FLEX_FLOW_COLUMN, 4);
    lv_obj_set_width(rc, LV_PCT(48));
    label(rc, "Laser power", &lv_font_montserrat_14, COL_SUB);
    s_power = label(rc, "0 %", &lv_font_montserrat_28, COL_AMBER);
    s_sval = label(rc, "S0", &lv_font_montserrat_14, COL_SUB);

    override_card(c, "Feed override", grbl::rt::FEED_M10, grbl::rt::FEED_RESET, grbl::rt::FEED_P10,
                  &s_feed_ov, COL_TEXT);
    override_card(c, "Power override", grbl::rt::SPINDLE_M10, grbl::rt::SPINDLE_RESET,
                  grbl::rt::SPINDLE_P10, &s_pwr_ov, COL_AMBER);

    lv_obj_t *grid = box(c, LV_FLEX_FLOW_COLUMN, 12);
    lv_obj_set_flex_grow(grid, 1);
    for (int r = 0; r < 2; ++r) {
        lv_obj_t *row = box(grid, LV_FLEX_FLOW_ROW, 12);
        lv_obj_set_flex_grow(row, 1);
        lv_obj_t *a, *b;
        if (r == 0) {
            a = button(row, "Pause", COL_AMBER_BG, COL_AMBER_TX, &lv_font_montserrat_24, rt_cb,
                       (void *)(intptr_t)grbl::rt::HOLD);
            b = button(row, "Resume", COL_GREEN_BG, COL_GREEN, &lv_font_montserrat_24, rt_cb,
                       (void *)(intptr_t)grbl::rt::RESUME);
        } else {
            a = button(row, "Stop", COL_RED_BG, COL_RED, &lv_font_montserrat_24, stop_cb);
            b = button(row, "Air off", COL_CARD, COL_SOFT, &lv_font_montserrat_20, rt_cb,
                       (void *)(intptr_t)grbl::rt::FLOOD_TOGGLE, COL_EDGE, &s_air_lbl[0]);
            s_air_btn[0] = b;
        }
        for (lv_obj_t *o : {a, b}) {
            lv_obj_set_height(o, LV_PCT(100));
            lv_obj_set_flex_grow(o, 1);
        }
    }
    make_nav(scr, SCR_STATUS);
}

// ---- Jog --------------------------------------------------------------------

static void jog_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i == 4) {
        grbl::realtime(grbl::rt::JOG_CANCEL);
        return;
    }
    int dx = (i % 3) - 1;
    int dy = 1 - (i / 3);
    float d = STEPS[s_step_i];
    char cmd[64];
    int n = snprintf(cmd, sizeof(cmd), "$J=G91 G21");
    if (dx) n += snprintf(cmd + n, sizeof(cmd) - n, " X%.3f", dx * d);
    if (dy) n += snprintf(cmd + n, sizeof(cmd) - n, " Y%.3f", dy * d);
    snprintf(cmd + n, sizeof(cmd) - n, " F%d", SPEEDS[s_speed_i]);
    panel_send(cmd);
}

static void step_cb(lv_event_t *e) {
    s_step_i = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 4; ++i) select_style(s_step_btn[i], i == s_step_i);
}

static void speed_cb(lv_event_t *e) {
    s_speed_i = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 3; ++i) select_style(s_speed_btn[i], i == s_speed_i);
}

static void line_cb(lv_event_t *e) { panel_send((const char *)lv_event_get_user_data(e)); }

static void pointer_set(bool on) {
    if (on) {
        char cmd[24];
        int s = CONFIG_K40_S_MAX * CONFIG_K40_POINTER_PCT / 100;
        snprintf(cmd, sizeof(cmd), "M3 S%d", s);
        if (!panel_send(cmd)) return;
        lv_timer_reset(s_ptr_timer);
        lv_timer_resume(s_ptr_timer);
    } else {
        grbl::send("M5");
        lv_timer_pause(s_ptr_timer);
    }
    s_ptr_on = on;
    lv_label_set_text(s_ptr_lbl, on ? "Pointer on" : "Pointer 1%");
    if (on)
        set_btn_colors(s_ptr_btn, COL_AMBER, 0x2A1D04, 0);
    else
        set_btn_colors(s_ptr_btn, COL_CARD, COL_AMBER_TX, COL_AMBER);
}

static void pointer_cb(lv_event_t *) { pointer_set(!s_ptr_on); }
static void pointer_timeout(lv_timer_t *) { pointer_set(false); }

static void build_jog() {
    lv_obj_t *scr = make_screen(SCR_JOG);
    lv_obj_t *c = make_content(scr);
    lv_obj_set_style_pad_row(c, 14, 0);

    lv_obj_t *pad = box(c, LV_FLEX_FLOW_ROW_WRAP, 8);
    static const char *lbl[9] = {LV_SYMBOL_UP " " LV_SYMBOL_LEFT, "Y+", LV_SYMBOL_UP " " LV_SYMBOL_RIGHT,
                                 "X-", "Stop", "X+",
                                 LV_SYMBOL_DOWN " " LV_SYMBOL_LEFT, "Y-", LV_SYMBOL_DOWN " " LV_SYMBOL_RIGHT};
    for (int i = 0; i < 9; ++i) {
        bool diag = i == 0 || i == 2 || i == 6 || i == 8;
        lv_obj_t *b;
        if (i == 4)
            b = button(pad, lbl[i], COL_RED_CARD, COL_RED, &lv_font_montserrat_20, jog_cb,
                       (void *)(intptr_t)i, COL_RED_BG);
        else
            b = button(pad, lbl[i], diag ? COL_CARD : COL_BTN, diag ? COL_SUB : COL_TEXT,
                       diag ? &lv_font_montserrat_20 : &lv_font_montserrat_24, jog_cb,
                       (void *)(intptr_t)i, COL_EDGE);
        lv_obj_set_size(b, 144, 96);
    }

    label(c, "Step (mm)", &lv_font_montserrat_14, COL_SUB);
    lv_obj_t *steps = box(c, LV_FLEX_FLOW_ROW, 6);
    static const char *step_txt[4] = {"0.1", "1", "10", "50"};
    for (int i = 0; i < 4; ++i) {
        s_step_btn[i] = button(steps, step_txt[i], COL_CARD, COL_SOFT, &lv_font_montserrat_20, step_cb,
                               (void *)(intptr_t)i, COL_EDGE);
        lv_obj_set_height(s_step_btn[i], 52);
        lv_obj_set_flex_grow(s_step_btn[i], 1);
        lv_obj_set_style_radius(s_step_btn[i], 8, 0);
        select_style(s_step_btn[i], i == s_step_i);
    }

    label(c, "Jog speed (mm/min)", &lv_font_montserrat_14, COL_SUB);
    lv_obj_t *speeds = box(c, LV_FLEX_FLOW_ROW, 6);
    static const char *spd_txt[3] = {"1000", "3000", "6000"};
    for (int i = 0; i < 3; ++i) {
        s_speed_btn[i] = button(speeds, spd_txt[i], COL_CARD, COL_SOFT, &lv_font_montserrat_20,
                                speed_cb, (void *)(intptr_t)i, COL_EDGE);
        lv_obj_set_height(s_speed_btn[i], 52);
        lv_obj_set_flex_grow(s_speed_btn[i], 1);
        lv_obj_set_style_radius(s_speed_btn[i], 8, 0);
        select_style(s_speed_btn[i], i == s_speed_i);
    }

    lv_obj_t *acts = box(c, LV_FLEX_FLOW_COLUMN, 8);
    lv_obj_set_flex_grow(acts, 1);
    static const char *home = "$H";
    static const char *set0 = "G10 L20 P1 X0 Y0";
    static const char *go0 = "G90 G0 X0 Y0";
    for (int r = 0; r < 2; ++r) {
        lv_obj_t *row = box(acts, LV_FLEX_FLOW_ROW, 8);
        lv_obj_set_flex_grow(row, 1);
        lv_obj_t *a, *b;
        if (r == 0) {
            a = button(row, "Home", COL_BTN, COL_TEXT, &lv_font_montserrat_20, line_cb, (void *)home);
            b = button(row, "Set origin", COL_BTN, COL_TEXT, &lv_font_montserrat_20, line_cb, (void *)set0);
        } else {
            a = button(row, "Go to origin", COL_BTN, COL_TEXT, &lv_font_montserrat_20, line_cb, (void *)go0);
            b = button(row, "Pointer 1%", COL_CARD, COL_AMBER_TX, &lv_font_montserrat_20, pointer_cb,
                       nullptr, COL_AMBER, &s_ptr_lbl);
            s_ptr_btn = b;
        }
        for (lv_obj_t *o : {a, b}) {
            lv_obj_set_height(o, LV_PCT(100));
            lv_obj_set_flex_grow(o, 1);
        }
    }
    s_ptr_timer = lv_timer_create(pointer_timeout, 30000, nullptr);  // pointer auto-off
    lv_timer_pause(s_ptr_timer);
    make_nav(scr, SCR_JOG);
}

// ---- Laser ------------------------------------------------------------------

static void test_adj_cb(lv_event_t *e) {
    s_test_pct += (int)(intptr_t)lv_event_get_user_data(e);
    if (s_test_pct < 0) s_test_pct = 0;
    int cap = settings::get().max_power_pct;
    if (s_test_pct > cap) s_test_pct = cap;
    lv_label_set_text_fmt(s_test_lbl, "%d %%", s_test_pct);
}

static void dur_cb(lv_event_t *e) {
    s_dur_i = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 3; ++i) select_style(s_dur_btn[i], i == s_dur_i);
}

static void arm_set(bool on) {
    s_armed = on;
    lv_label_set_text(s_arm_lbl, on ? "Armed" : "Arm");
    if (on) {
        set_btn_colors(s_arm_btn, COL_AMBER_BG, COL_AMBER_TX, COL_AMBER);
        set_btn_colors(s_fire_btn, COL_AMBER, 0x2A1D04, 0);
        lv_timer_reset(s_arm_timer);
        lv_timer_resume(s_arm_timer);
    } else {
        set_btn_colors(s_arm_btn, COL_CARD, COL_SOFT, COL_EDGE);
        set_btn_colors(s_fire_btn, COL_BTN, COL_SUB, 0);
        lv_timer_pause(s_arm_timer);
    }
}

static void arm_on() { arm_set(true); }
static void arm_cb(lv_event_t *) {
    if (s_armed)
        arm_set(false);
    else
        require_pin(arm_on);
}
static void arm_timeout(lv_timer_t *) { arm_set(false); }

static void fire_cb(lv_event_t *) {
    if (!s_armed) {
        toast("Tap Arm first");
        return;
    }
    grbl::Status st = grbl::status();
    if (st.hold_pin || st.probe_pin || st.door_pin) {
        toast("Interlock open - check lid and water");
        return;
    }
    // M3 fires even when stationary in laser mode; G4 holds it, M5 ends it
    char on[24], dwell[24];
    snprintf(on, sizeof(on), "M3 S%d", CONFIG_K40_S_MAX * s_test_pct / 100);
    snprintf(dwell, sizeof(dwell), "G4 P%.3f", DUR_MS[s_dur_i] / 1000.0f);
    const char *lines[3] = {on, dwell, "M5"};
    if (!grbl::send_lines(lines, 3)) {
        toast("Not now - job or link busy");
        return;
    }
    arm_set(false);  // one pulse per arm
}

static void exhaust_cb(lv_event_t *) {
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "%s P%d", s_exhaust ? "M65" : "M64", CONFIG_K40_EXHAUST_AUX_PORT);
    if (!panel_send(cmd)) return;
    s_exhaust = !s_exhaust;
    lv_label_set_text(s_exh_lbl, s_exhaust ? "Exhaust on" : "Exhaust off");
}

static void build_laser() {
    lv_obj_t *scr = make_screen(SCR_LASER);
    lv_obj_t *c = make_content(scr);

    lv_obj_t *pw = card(c);
    lv_obj_t *top = box(pw, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(top, "Test power", &lv_font_montserrat_14, COL_SUB);
    s_test_lbl = label(top, "", &lv_font_montserrat_40, COL_AMBER);
    lv_obj_t *adj = box(pw, LV_FLEX_FLOW_ROW, 8);
    static const int steps[4] = {-5, -1, 1, 5};
    static const char *txt[4] = {"-5", "-1", "+1", "+5"};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *b = button(adj, txt[i], COL_BTN, COL_TEXT, &lv_font_montserrat_20, test_adj_cb,
                             (void *)(intptr_t)steps[i], COL_EDGE);
        lv_obj_set_height(b, 52);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_radius(b, 8, 0);
    }
    s_cap_lbl = label(pw, "", &lv_font_montserrat_14, COL_SUB);
    lv_label_set_text_fmt(s_cap_lbl, "Capped at %d%% (tube safe current)", settings::get().max_power_pct);

    lv_obj_t *durs = box(c, LV_FLEX_FLOW_ROW, 6);
    static const char *dtxt[3] = {"50 ms", "200 ms", "1 s"};
    for (int i = 0; i < 3; ++i) {
        s_dur_btn[i] = button(durs, dtxt[i], COL_CARD, COL_SOFT, &lv_font_montserrat_20, dur_cb,
                              (void *)(intptr_t)i, COL_EDGE);
        lv_obj_set_height(s_dur_btn[i], 52);
        lv_obj_set_flex_grow(s_dur_btn[i], 1);
        lv_obj_set_style_radius(s_dur_btn[i], 8, 0);
        select_style(s_dur_btn[i], i == s_dur_i);
    }

    s_fire_btn = button(c, "Fire pulse", COL_BTN, COL_SUB, &lv_font_montserrat_28, fire_cb);
    lv_obj_set_width(s_fire_btn, LV_PCT(100));
    lv_obj_set_flex_grow(s_fire_btn, 1);
    lv_obj_set_style_radius(s_fire_btn, 12, 0);

    lv_obj_t *il = card(c);
    label(il, "Interlocks", &lv_font_montserrat_14, COL_SUB);
    lv_obj_t *lid = box(il, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(lid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(lid, "Lid", &lv_font_montserrat_20, COL_TEXT);
    s_lid_lbl = label(lid, "--", &lv_font_montserrat_20, COL_SUB);
    lv_obj_t *water = box(il, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(water, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(water, "Water flow", &lv_font_montserrat_20, COL_TEXT);
    s_water_lbl = label(water, "--", &lv_font_montserrat_20, COL_SUB);
    lv_obj_t *arm = box(il, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(arm, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(arm, "Test fire", &lv_font_montserrat_20, COL_TEXT);
    s_arm_btn = button(arm, "Arm", COL_CARD, COL_SOFT, &lv_font_montserrat_20, arm_cb, nullptr, COL_EDGE,
                       &s_arm_lbl);
    lv_obj_set_size(s_arm_btn, 140, 48);
    lv_obj_set_style_radius(s_arm_btn, 8, 0);

    lv_obj_t *row = box(c, LV_FLEX_FLOW_ROW, 8);
    s_air_btn[1] = button(row, "Air off", COL_CARD, COL_SOFT, &lv_font_montserrat_20, rt_cb,
                          (void *)(intptr_t)grbl::rt::FLOOD_TOGGLE, COL_EDGE, &s_air_lbl[1]);
    lv_obj_t *items[3] = {s_air_btn[1], nullptr, nullptr};
    int n = 1;
    if (CONFIG_K40_EXHAUST_AUX_PORT >= 0) {
        items[n++] = button(row, "Exhaust off", COL_CARD, COL_SOFT, &lv_font_montserrat_20, exhaust_cb,
                            nullptr, COL_EDGE, &s_exh_lbl);
    }
    items[n++] = button(row, "Laser off", COL_BTN, COL_TEXT, &lv_font_montserrat_20, line_cb, (void *)"M5");
    for (int i = 0; i < n; ++i) {
        lv_obj_set_height(items[i], 60);
        lv_obj_set_flex_grow(items[i], 1);
    }

    s_arm_timer = lv_timer_create(arm_timeout, 60000, nullptr);  // disarm after a minute
    lv_timer_pause(s_arm_timer);
    lv_label_set_text_fmt(s_test_lbl, "%d %%", s_test_pct);
    make_nav(scr, SCR_LASER);
}

// ---- Alarms -----------------------------------------------------------------

static const char *alarm_text(int code, const char **desc) {
    switch (code) {
        case 1: *desc = "An endstop tripped during motion. Position is lost - home before running again."; return "Hard limit";
        case 2: *desc = "A move went past the travel limits. Check the job origin and size."; return "Soft limit";
        case 3: *desc = "Reset while moving. Position may be lost - home before running again."; return "Reset in motion";
        case 4: case 5: *desc = "Probe cycle failed."; return "Probe fail";
        case 6: *desc = "Homing was reset before it finished."; return "Homing reset";
        case 7: *desc = "The door opened during homing."; return "Homing: door open";
        case 8: *desc = "Couldn't clear the limit switch on pull-off. Check pull-off distance and switches."; return "Homing: pull-off";
        case 9: *desc = "No limit switch found. Check endstop wiring and homing direction."; return "Homing: switch not found";
        case 10: *desc = "The E-stop / reset input is active. Release it, then Reset."; return "E-stop";
        case 11: *desc = "Home the machine before running jobs."; return "Homing required";
        case 12: *desc = "A limit switch is held down. Jog off it or check wiring."; return "Limit switch engaged";
        case 14: *desc = "The spindle (laser) didn't report ready in time."; return "Spindle timeout";
        case 17: *desc = "A stepper driver reported a fault."; return "Motor fault";
        default: *desc = "See the grblHAL alarm list for this code."; return "Alarm";
    }
}

static void reset_cb(lv_event_t *) { grbl::panel_stop(); }

static void build_alarms() {
    lv_obj_t *scr = make_screen(SCR_ALARMS);
    lv_obj_t *c = make_content(scr);

    s_alarm_card = card(c, COL_RED_CARD);
    lv_obj_set_style_border_width(s_alarm_card, 1, 0);
    lv_obj_set_style_border_color(s_alarm_card, C(0x5A2424), 0);
    lv_obj_set_style_pad_all(s_alarm_card, 16, 0);
    s_alarm_title = label(s_alarm_card, "No active alarm", &lv_font_montserrat_24, COL_RED);
    s_alarm_desc = label(s_alarm_card, "", &lv_font_montserrat_16, 0xE3C4C4);
    lv_obj_set_width(s_alarm_desc, LV_PCT(100));
    lv_label_set_long_mode(s_alarm_desc, LV_LABEL_LONG_WRAP);

    static const char *home = "$H", *unlock = "$X";
    lv_obj_t *hb = button(c, "Home", COL_AMBER, 0x2A1D04, &lv_font_montserrat_24, line_cb, (void *)home);
    lv_obj_set_size(hb, LV_PCT(100), 76);
    lv_obj_t *row = box(c, LV_FLEX_FLOW_ROW, 10);
    lv_obj_t *u = button(row, "Unlock", COL_BTN, COL_TEXT, &lv_font_montserrat_20, line_cb, (void *)unlock);
    lv_obj_t *r = button(row, "Reset", COL_BTN, COL_TEXT, &lv_font_montserrat_20, reset_cb);
    for (lv_obj_t *o : {u, r}) {
        lv_obj_set_height(o, 68);
        lv_obj_set_flex_grow(o, 1);
    }

    lv_obj_t *con = card(c, 0x14171A);
    lv_obj_set_style_border_width(con, 1, 0);
    lv_obj_set_style_border_color(con, C(COL_LINE), 0);
    lv_obj_set_flex_grow(con, 1);
    s_console = label(con, "", &lv_font_montserrat_14, COL_SUB);
    lv_obj_set_width(s_console, LV_PCT(100));
    lv_label_set_long_mode(s_console, LV_LABEL_LONG_CLIP);
    make_nav(scr, SCR_ALARMS);
}

// ---- Setup ------------------------------------------------------------------

static lv_obj_t *s_kb, *s_set_nav, *s_set_content;
static lv_obj_t *s_ssid_ta, *s_pass_ta, *s_ip_ta, *s_scan_dd, *s_scan_lbl, *s_show_lbl;
static lv_obj_t *s_cap_slider, *s_cap_val, *s_bright_slider, *s_bright_val;
static lv_obj_t *s_wifi_status, *s_panel_ip_lbl, *s_set_err;
static bool s_scanning = false;
static const int SLEEP_OPTS[4] = {0, 2, 5, 15};
static lv_obj_t *s_sleep_btn[4], *s_flip_btn[2], *s_pin_set_ta, *s_tube_lbl, *s_tube_reset_lbl;
static int s_sleep_i = 2, s_flip_i = 0;
static uint32_t s_tube_reset_armed = 0;

static void kb_hide() {
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_set_nav, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *ta = lv_keyboard_get_textarea(s_kb);
    if (ta) lv_obj_remove_state(ta, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(s_kb, nullptr);
}

// Tapping a field pops the keyboard up under it; the nav bar hides to make room
static void ta_event_cb(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        lv_keyboard_set_mode(s_kb, (ta == s_ip_ta || ta == s_pin_set_ta) ? LV_KEYBOARD_MODE_NUMBER
                                                                    : LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_keyboard_set_textarea(s_kb, ta);
        lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_set_nav, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(s_set_content);
        lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
        lv_obj_add_state(ta, LV_STATE_FOCUSED);
    }
}

static void kb_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) kb_hide();
}

static lv_obj_t *text_field(lv_obj_t *parent, const char *placeholder) {
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_obj_set_height(ta, 52);
    lv_obj_set_style_bg_color(ta, C(0x14171A), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ta, C(COL_EDGE), 0);
    lv_obj_set_style_border_color(ta, C(COL_AMBER), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_text_color(ta, C(COL_TEXT), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ta, C(COL_SUB), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_pad_hor(ta, 12, 0);
    lv_obj_set_style_pad_ver(ta, 12, 0);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, nullptr);
    return ta;
}

static void style_slider(lv_obj_t *s) {
    lv_obj_set_width(s, LV_PCT(100));
    lv_obj_set_height(s, 10);
    lv_obj_set_style_bg_color(s, C(COL_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, C(COL_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, C(COL_AMBER), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 10, LV_PART_KNOB);
    lv_obj_set_style_margin_ver(s, 12, 0);  // bigger touch area for the knob
}

static void cap_slider_cb(lv_event_t *) {
    lv_label_set_text_fmt(s_cap_val, "%d %%", (int)lv_slider_get_value(s_cap_slider));
}

static void bright_slider_cb(lv_event_t *e) {
    int v = (int)lv_slider_get_value(s_bright_slider);
    lv_label_set_text_fmt(s_bright_val, "%d %%", v);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) board::backlight((uint8_t)v);  // live preview
}

static void show_pass_cb(lv_event_t *) {
    bool hidden = lv_textarea_get_password_mode(s_pass_ta);
    lv_textarea_set_password_mode(s_pass_ta, !hidden);
    lv_label_set_text(s_show_lbl, hidden ? "Hide" : "Show");
}

static void scan_pick_cb(lv_event_t *) {
    char buf[40];
    lv_dropdown_get_selected_str(s_scan_dd, buf, sizeof(buf));
    lv_textarea_set_text(s_ssid_ta, buf);
}

static void scan_task(void *) {
    std::vector<std::string> found = net::scan();
    display::lock();
    if (found.empty()) {
        toast("No networks found");
    } else {
        std::string opts;
        for (size_t i = 0; i < found.size(); ++i) {
            if (i) opts += '\n';
            opts += found[i];
        }
        lv_dropdown_set_options(s_scan_dd, opts.c_str());
        lv_obj_remove_flag(s_scan_dd, LV_OBJ_FLAG_HIDDEN);
        lv_dropdown_open(s_scan_dd);
    }
    lv_label_set_text(s_scan_lbl, "Scan");
    s_scanning = false;
    display::unlock();
    vTaskDelete(nullptr);
}

static void scan_cb(lv_event_t *) {
    if (s_scanning) return;
    s_scanning = true;
    kb_hide();
    lv_label_set_text(s_scan_lbl, "...");
    xTaskCreate(scan_task, "wifi_scan", 6144, nullptr, 3, nullptr);
}

static void sleep_sel_cb(lv_event_t *e) {
    s_sleep_i = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 4; ++i) select_style(s_sleep_btn[i], i == s_sleep_i);
}

static void flip_sel_cb(lv_event_t *e) {
    s_flip_i = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 2; ++i) select_style(s_flip_btn[i], i == s_flip_i);
}

// Two taps within 3 s so the counter can't be cleared by accident
static void tube_reset_cb(lv_event_t *) {
    uint32_t now = lv_tick_get();
    if (s_tube_reset_armed && now - s_tube_reset_armed < 3000) {
        settings::tube_reset();
        s_tube_reset_armed = 0;
        lv_label_set_text(s_tube_reset_lbl, "Reset");
        toast("Laser on-time reset");
    } else {
        s_tube_reset_armed = now;
        lv_label_set_text(s_tube_reset_lbl, "Sure?");
    }
}

static void save_cb(lv_event_t *) {
    kb_hide();
    if (grbl::status().job_active) {
        toast("Can't change settings during a job");
        return;
    }
    settings::Values old = settings::get();
    settings::Values v = old;
    v.ssid = lv_textarea_get_text(s_ssid_ta);
    v.password = lv_textarea_get_text(s_pass_ta);
    v.allowed_ip = lv_textarea_get_text(s_ip_ta);
    v.max_power_pct = (int)lv_slider_get_value(s_cap_slider);
    v.brightness = (int)lv_slider_get_value(s_bright_slider);
    v.sleep_min = SLEEP_OPTS[s_sleep_i];
    v.flipped = s_flip_i == 1;
    v.pin = lv_textarea_get_text(s_pin_set_ta);
    const char *err = settings::check_ssid(v.ssid);
    if (!err) err = settings::check_password(v.password);
    if (!err) err = settings::check_ip(v.allowed_ip);
    if (!err) err = settings::check_pin(v.pin);
    if (err) {
        lv_label_set_text(s_set_err, err);
        lv_obj_remove_flag(s_set_err, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(s_set_err, LV_OBJ_FLAG_HIDDEN);
    if (!settings::save(v)) {
        toast("Save failed");
        return;
    }
    board::backlight((uint8_t)v.brightness);
    if (v.flipped != old.flipped) display::set_flipped(v.flipped);
    if (!v.pin.empty() && v.pin != old.pin) s_unlocked_until = lv_tick_get() + UNLOCK_MS;  // don't lock yourself out
    lv_label_set_text_fmt(s_cap_lbl, "Capped at %d%% (tube safe current)", v.max_power_pct);
    if (s_test_pct > v.max_power_pct) {
        s_test_pct = v.max_power_pct;
        lv_label_set_text_fmt(s_test_lbl, "%d %%", s_test_pct);
    }
    if (v.ssid != old.ssid || v.password != old.password) {
        net::reconnect();
        toast("Saved - reconnecting Wi-Fi");
    } else {
        toast("Saved");
    }
}

static lv_obj_t *section(lv_obj_t *parent, const char *title) {
    lv_obj_t *c = card(parent);
    label(c, title, &lv_font_montserrat_16, COL_SUB);
    return c;
}

static void build_setup() {
    settings::Values v = settings::get();
    lv_obj_t *scr = make_screen(SCR_SETUP);
    s_set_content = make_content(scr);
    lv_obj_add_flag(s_set_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_set_content, LV_DIR_VER);
    // Always show the scrollbar so it's obvious there's more below (Save is at the bottom)
    lv_obj_set_scrollbar_mode(s_set_content, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_bg_color(s_set_content, C(COL_SUB), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_set_content, LV_OPA_60, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(s_set_content, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(s_set_content, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_bottom(s_set_content, 24, 0);

    // Wi-Fi
    lv_obj_t *wifi = section(s_set_content, "Wi-Fi (2.4 GHz)");
    s_wifi_status = label(wifi, "", &lv_font_montserrat_14, COL_SOFT);
    label(wifi, "Network name", &lv_font_montserrat_14, COL_SUB);
    lv_obj_t *row = box(wifi, LV_FLEX_FLOW_ROW, 8);
    s_ssid_ta = text_field(row, "Wi-Fi name");
    lv_obj_set_flex_grow(s_ssid_ta, 1);
    lv_obj_set_width(s_ssid_ta, 1);
    lv_obj_t *sb = button(row, "Scan", COL_BTN, COL_TEXT, &lv_font_montserrat_16, scan_cb, nullptr, COL_EDGE,
                          &s_scan_lbl);
    lv_obj_set_size(sb, 84, 52);
    lv_obj_set_style_radius(sb, 8, 0);
    s_scan_dd = lv_dropdown_create(wifi);
    lv_obj_set_width(s_scan_dd, LV_PCT(100));
    lv_dropdown_set_text(s_scan_dd, "Pick a network");
    lv_obj_set_style_bg_color(s_scan_dd, C(COL_BTN), 0);
    lv_obj_set_style_text_color(s_scan_dd, C(COL_TEXT), 0);
    lv_obj_set_style_border_color(s_scan_dd, C(COL_EDGE), 0);
    lv_obj_t *list = lv_dropdown_get_list(s_scan_dd);
    lv_obj_set_style_bg_color(list, C(COL_BTN), 0);
    lv_obj_set_style_text_color(list, C(COL_TEXT), 0);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(s_scan_dd, scan_pick_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_flag(s_scan_dd, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(s_ssid_ta, v.ssid.c_str());

    label(wifi, "Password", &lv_font_montserrat_14, COL_SUB);
    lv_obj_t *prow = box(wifi, LV_FLEX_FLOW_ROW, 8);
    s_pass_ta = text_field(prow, "Wi-Fi password");
    lv_obj_set_flex_grow(s_pass_ta, 1);
    lv_obj_set_width(s_pass_ta, 1);
    lv_textarea_set_password_mode(s_pass_ta, true);
    lv_textarea_set_text(s_pass_ta, v.password.c_str());
    lv_obj_t *show = button(prow, "Show", COL_BTN, COL_TEXT, &lv_font_montserrat_16, show_pass_cb, nullptr,
                            COL_EDGE, &s_show_lbl);
    lv_obj_set_size(show, 84, 52);
    lv_obj_set_style_radius(show, 8, 0);

    // LightBurn
    lv_obj_t *lb = section(s_set_content, "LightBurn computer");
    label(lb, "Only accept this IP (blank = any)", &lv_font_montserrat_14, COL_SUB);
    s_ip_ta = text_field(lb, "192.168.1.50");
    lv_textarea_set_accepted_chars(s_ip_ta, "0123456789.");
    lv_textarea_set_max_length(s_ip_ta, 15);
    lv_textarea_set_text(s_ip_ta, v.allowed_ip.c_str());
    s_panel_ip_lbl = label(lb, "", &lv_font_montserrat_14, COL_SOFT);
    lv_obj_set_width(s_panel_ip_lbl, LV_PCT(100));
    lv_label_set_long_mode(s_panel_ip_lbl, LV_LABEL_LONG_WRAP);

    // Laser & display
    lv_obj_t *ld = section(s_set_content, "Laser and display");
    lv_obj_t *r1 = box(ld, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(r1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(r1, "Panel power cap", &lv_font_montserrat_20, COL_TEXT);
    s_cap_val = label(r1, "", &lv_font_montserrat_20, COL_AMBER);
    s_cap_slider = lv_slider_create(ld);
    style_slider(s_cap_slider);
    lv_slider_set_range(s_cap_slider, 1, 100);
    lv_slider_set_value(s_cap_slider, v.max_power_pct, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_cap_slider, cap_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_t *r2 = box(ld, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_flex_align(r2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(r2, "Brightness", &lv_font_montserrat_20, COL_TEXT);
    s_bright_val = label(r2, "", &lv_font_montserrat_20, COL_SOFT);
    s_bright_slider = lv_slider_create(ld);
    style_slider(s_bright_slider);
    lv_slider_set_range(s_bright_slider, 10, 100);
    lv_slider_set_value(s_bright_slider, v.brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_bright_slider, bright_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(s_bright_slider, bright_slider_cb, LV_EVENT_RELEASED, nullptr);
    lv_label_set_text_fmt(s_cap_val, "%d %%", v.max_power_pct);
    lv_label_set_text_fmt(s_bright_val, "%d %%", v.brightness);

    // Panel
    lv_obj_t *pn = section(s_set_content, "Panel");
    label(pn, "Screen sleep", &lv_font_montserrat_14, COL_SUB);
    lv_obj_t *sl = box(pn, LV_FLEX_FLOW_ROW, 6);
    static const char *sleep_txt[4] = {"Never", "2 min", "5 min", "15 min"};
    s_sleep_i = 0;
    for (int i = 0; i < 4; ++i)
        if (SLEEP_OPTS[i] == v.sleep_min) s_sleep_i = i;
    for (int i = 0; i < 4; ++i) {
        s_sleep_btn[i] = button(sl, sleep_txt[i], COL_CARD, COL_SOFT, &lv_font_montserrat_16, sleep_sel_cb,
                                (void *)(intptr_t)i, COL_EDGE);
        lv_obj_set_height(s_sleep_btn[i], 48);
        lv_obj_set_flex_grow(s_sleep_btn[i], 1);
        lv_obj_set_style_radius(s_sleep_btn[i], 8, 0);
        select_style(s_sleep_btn[i], i == s_sleep_i);
    }
    label(pn, "Screen orientation", &lv_font_montserrat_14, COL_SUB);
    lv_obj_t *fl = box(pn, LV_FLEX_FLOW_ROW, 6);
    static const char *flip_txt[2] = {"Normal", "Flipped"};
    s_flip_i = v.flipped ? 1 : 0;
    for (int i = 0; i < 2; ++i) {
        s_flip_btn[i] = button(fl, flip_txt[i], COL_CARD, COL_SOFT, &lv_font_montserrat_16, flip_sel_cb,
                               (void *)(intptr_t)i, COL_EDGE);
        lv_obj_set_height(s_flip_btn[i], 48);
        lv_obj_set_flex_grow(s_flip_btn[i], 1);
        lv_obj_set_style_radius(s_flip_btn[i], 8, 0);
        select_style(s_flip_btn[i], i == s_flip_i);
    }
    label(pn, "PIN lock for Setup and test fire (4 digits, blank = off)", &lv_font_montserrat_14, COL_SUB);
    s_pin_set_ta = text_field(pn, "No PIN");
    lv_textarea_set_accepted_chars(s_pin_set_ta, "0123456789");
    lv_textarea_set_max_length(s_pin_set_ta, 4);
    lv_textarea_set_password_mode(s_pin_set_ta, true);
    lv_textarea_set_text(s_pin_set_ta, v.pin.c_str());
    lv_obj_t *tr = box(pn, LV_FLEX_FLOW_ROW, 8);
    lv_obj_set_flex_align(tr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *tl = label(tr, "Laser on-time", &lv_font_montserrat_20, COL_TEXT);
    lv_obj_set_flex_grow(tl, 1);
    s_tube_lbl = label(tr, "0.0 h", &lv_font_montserrat_20, COL_AMBER);
    lv_obj_t *trb = button(tr, "Reset", COL_BTN, COL_TEXT, &lv_font_montserrat_16, tube_reset_cb, nullptr,
                           COL_EDGE, &s_tube_reset_lbl);
    lv_obj_set_size(trb, 84, 44);
    lv_obj_set_style_radius(trb, 8, 0);

    s_set_err = label(s_set_content, "", &lv_font_montserrat_16, COL_RED);
    lv_obj_add_flag(s_set_err, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *save = button(s_set_content, "Save", COL_AMBER, 0x2A1D04, &lv_font_montserrat_24, save_cb);
    lv_obj_set_size(save, LV_PCT(100), 64);
    lv_obj_set_style_radius(save, 10, 0);
    lv_obj_t *ver = label(s_set_content, "", &lv_font_montserrat_14, COL_SUB);
    lv_label_set_text_fmt(ver, "Firmware %s  -  update: firmware.bin on the SD card, then restart",
                          esp_app_get_description()->version);
    lv_obj_set_width(ver, LV_PCT(100));
    lv_label_set_long_mode(ver, LV_LABEL_LONG_WRAP);

    // Small pop-up keyboard: sits between the content and the nav bar, hidden until a field is tapped
    s_kb = lv_keyboard_create(scr);
    lv_obj_set_size(s_kb, LV_PCT(100), 260);
    lv_obj_set_style_bg_color(s_kb, C(COL_CARD), 0);
    lv_obj_set_style_border_width(s_kb, 0, 0);
    lv_obj_set_style_pad_all(s_kb, 6, 0);
    lv_obj_set_style_pad_gap(s_kb, 6, 0);
    lv_obj_set_style_bg_color(s_kb, C(COL_BTN), LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_kb, C(COL_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_kb, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(s_kb, 6, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(s_kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(s_kb, C(COL_AMBER_BG), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(s_kb, C(COL_AMBER_TX), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
    // IP-friendly number pad: no +/- key, close and done on the right edge
    static const char *num_map[] = {"1", "2", "3", LV_SYMBOL_CLOSE, "\n",
                                    "4", "5", "6", LV_SYMBOL_OK, "\n",
                                    "7", "8", "9", LV_SYMBOL_BACKSPACE, "\n",
                                    LV_SYMBOL_LEFT, "0", ".", LV_SYMBOL_RIGHT, ""};
    const lv_buttonmatrix_ctrl_t W1 = (lv_buttonmatrix_ctrl_t)1;  // width 1
    const lv_buttonmatrix_ctrl_t OK = (lv_buttonmatrix_ctrl_t)(1 | LV_BUTTONMATRIX_CTRL_CHECKED);
    static lv_buttonmatrix_ctrl_t num_ctrl[16];
    for (int i = 0; i < 16; ++i) num_ctrl[i] = i == 7 ? OK : W1;
    lv_keyboard_set_map(s_kb, LV_KEYBOARD_MODE_NUMBER, num_map, num_ctrl);
    lv_obj_add_event_cb(s_kb, kb_event_cb, LV_EVENT_ALL, nullptr);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);

    s_set_nav = make_nav(scr, SCR_SETUP);
}

// ---- live refresh -----------------------------------------------------------

static void badge_colors(grbl::State s, uint32_t *bg, uint32_t *fg) {
    using grbl::State;
    switch (s) {
        case State::Run: case State::Jog: case State::Home: *bg = COL_GREEN_BG; *fg = COL_GREEN; break;
        case State::Hold: case State::Door: *bg = COL_AMBER_BG; *fg = COL_AMBER_TX; break;
        case State::Alarm: case State::Unknown: *bg = COL_RED_BG; *fg = COL_RED; break;
        default: *bg = COL_GRAY_BG; *fg = COL_SOFT; break;
    }
}

static void refresh(lv_timer_t *) {
    grbl::Status st = grbl::status();
    net::Info ni = net::info();

    // Laser on-time: count while the laser output is on, save to flash once a minute
    static uint32_t last_tick = 0, last_flush = 0;
    uint32_t now = lv_tick_get();
    uint32_t dt = last_tick ? now - last_tick : 0;
    last_tick = now;
    bool firing = st.link_ok && st.spindle_on && st.spindle > 0 && st.state != grbl::State::Hold &&
                  st.state != grbl::State::Alarm && st.state != grbl::State::Door;
    if (firing && dt < 2000) settings::tube_add_ms(dt);
    if (now - last_flush > 60000 || (!st.job_active && now - last_flush > 5000 && !firing)) {
        last_flush = now;
        settings::tube_flush();  // no-op when nothing changed
    }
    uint32_t secs = settings::tube_seconds();
    lv_label_set_text_fmt(s_tube_lbl, "%u.%u h", (unsigned)(secs / 3600), (unsigned)(secs % 3600 / 360));
    if (s_tube_reset_armed && now - s_tube_reset_armed > 3000) {
        s_tube_reset_armed = 0;
        lv_label_set_text(s_tube_reset_lbl, "Reset");
    }

    // Screen sleep: never during a job, hold or alarm; wake when one starts
    int sleep_min = settings::get().sleep_min;
    bool busy = st.job_active || st.state == grbl::State::Run || st.state == grbl::State::Hold ||
                st.state == grbl::State::Door || st.state == grbl::State::Alarm;
    if (s_asleep && busy) {
        wake();
    } else if (!s_asleep && !busy && sleep_min > 0 &&
               lv_display_get_inactive_time(nullptr) > (uint32_t)sleep_min * 60000u) {
        sleep_now();
    }

    uint32_t bg, fg;
    badge_colors(st.state, &bg, &fg);
    const char *link_txt;
    uint32_t dot;
    std::string ip_txt;
    if (ni.client) {
        link_txt = "LightBurn";
        dot = COL_GREEN;
    } else if (ni.wifi_up) {
        ip_txt = ni.ip;
        link_txt = ip_txt.c_str();
        dot = COL_AMBER;
    } else {
        link_txt = "No Wi-Fi";
        dot = COL_RED;
    }
    for (Header &h : s_hdr) {
        lv_label_set_text(h.badge_lbl, grbl::state_name(st.state));
        lv_obj_set_style_bg_color(h.badge, C(bg), 0);
        lv_obj_set_style_text_color(h.badge_lbl, C(fg), 0);
        lv_label_set_text_fmt(h.pos, "X %.2f  Y %.2f", st.mpos[0], st.mpos[1]);
        lv_label_set_text(h.link, link_txt);
        lv_obj_set_style_bg_color(h.dot, C(dot), 0);
    }

    // Status
    lv_label_set_text_fmt(s_job_time, "%02u:%02u", (unsigned)(st.job_seconds / 60),
                          (unsigned)(st.job_seconds % 60));
    lv_label_set_text(s_job_sub, st.job_active      ? "Streaming from LightBurn"
                                 : st.host_connected ? "LightBurn connected"
                                                     : "Ready");
    lv_label_set_text_fmt(s_speed, "%.0f", st.feed);
    int pct = CONFIG_K40_S_MAX > 0 ? (int)(st.spindle * 100.0f / CONFIG_K40_S_MAX + 0.5f) : 0;
    lv_label_set_text_fmt(s_power, "%d %%", pct);
    lv_label_set_text_fmt(s_sval, "S%.0f / %d", st.spindle, CONFIG_K40_S_MAX);
    lv_label_set_text_fmt(s_work, "Work X %.2f Y %.2f", st.wpos[0], st.wpos[1]);
    lv_label_set_text_fmt(s_feed_ov, "%d%%", st.ov_feed);
    lv_label_set_text_fmt(s_pwr_ov, "%d%%", st.ov_spindle);
    for (int i = 0; i < 2; ++i) {
        lv_label_set_text(s_air_lbl[i], st.flood ? "Air on" : "Air off");
        if (st.flood)
            set_btn_colors(s_air_btn[i], COL_BLUE_BG, COL_BLUE, COL_BLUE_EDGE);
        else
            set_btn_colors(s_air_btn[i], COL_CARD, COL_SOFT, COL_EDGE);
    }

    // Laser
    // Separate switch contacts wired to the SKR (the PSU interlock loop is untouched):
    //   lid's 2nd contact -> PWR-DET (Pn:H when open), 2nd flow switch -> PROBE (Pn:P when no flow)
    bool lid_ok = !st.hold_pin;
    bool water_ok = !st.probe_pin;
    lv_label_set_text(s_lid_lbl, !st.link_ok ? "--" : (lid_ok ? "Closed" : "Open"));
    lv_obj_set_style_text_color(s_lid_lbl, C(!st.link_ok ? COL_SUB : (lid_ok ? COL_GREEN : COL_RED)), 0);
    lv_label_set_text(s_water_lbl, !st.link_ok ? "--" : (water_ok ? "Flowing" : "No flow"));
    lv_obj_set_style_text_color(s_water_lbl, C(!st.link_ok ? COL_SUB : (water_ok ? COL_GREEN : COL_RED)), 0);
    if (st.job_active && s_armed) arm_set(false);
    if (st.job_active && s_ptr_on) {
        s_ptr_on = false;  // LightBurn owns the laser now; don't send M5 into its stream
        lv_timer_pause(s_ptr_timer);
        lv_label_set_text(s_ptr_lbl, "Pointer 1%");
        set_btn_colors(s_ptr_btn, COL_CARD, COL_AMBER_TX, COL_AMBER);
    }

    // Alarms
    if (st.state == grbl::State::Alarm) {
        const char *desc;
        const char *title = alarm_text(st.alarm, &desc);
        if (st.alarm)
            lv_label_set_text_fmt(s_alarm_title, "Alarm %d - %s", st.alarm, title);
        else
            lv_label_set_text(s_alarm_title, "Alarm");
        lv_label_set_text(s_alarm_desc, st.alarm ? desc : "Unlock or home to continue.");
        lv_obj_set_style_bg_color(s_alarm_card, C(COL_RED_CARD), 0);
        lv_obj_set_style_text_color(s_alarm_title, C(COL_RED), 0);
        if (st.alarm && st.alarm != s_last_alarm) lv_screen_load(s_scr[SCR_ALARMS]);
        s_last_alarm = st.alarm;
    } else {
        lv_label_set_text(s_alarm_title, "No active alarm");
        lv_label_set_text(s_alarm_desc, st.link_ok ? "grblHAL is responding." : "No reply from grblHAL - check the UART wiring.");
        lv_obj_set_style_bg_color(s_alarm_card, C(COL_CARD), 0);
        lv_obj_set_style_text_color(s_alarm_title, C(COL_SOFT), 0);
        s_last_alarm = 0;
    }
    // Setup
    if (ni.wifi_up)
        lv_label_set_text_fmt(s_wifi_status, "Connected to %s  -  %s", settings::get().ssid.c_str(), ni.ip.c_str());
    else
        lv_label_set_text(s_wifi_status, settings::get().ssid.empty() ? "Not set up" : "Not connected");
    if (ni.wifi_up)
        lv_label_set_text_fmt(s_panel_ip_lbl, "In LightBurn, connect to %s port %d", ni.ip.c_str(),
                              CONFIG_K40_TELNET_PORT);
    else
        lv_label_set_text(s_panel_ip_lbl, "Panel IP appears here once Wi-Fi connects");

    std::string text;
    for (const std::string &l : grbl::console()) {
        text += l;
        text += '\n';
    }
    lv_label_set_text(s_console, text.c_str());
}

void build() {
    build_status();
    build_jog();
    build_laser();
    build_alarms();
    build_setup();

    s_toast = lv_label_create(lv_layer_top());
    lv_obj_set_style_bg_color(s_toast, C(0x2A2D33), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_toast, C(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_toast, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_hor(s_toast, 16, 0);
    lv_obj_set_style_pad_ver(s_toast, 10, 0);
    lv_obj_set_style_radius(s_toast, 8, 0);
    lv_obj_align(s_toast, LV_ALIGN_TOP_MID, 0, 68);
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    s_toast_timer = lv_timer_create(toast_hide, 2500, nullptr);
    lv_timer_pause(s_toast_timer);

    s_sleep_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_sleep_overlay);
    lv_obj_set_size(s_sleep_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(s_sleep_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_sleep_overlay, sleep_overlay_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_flag(s_sleep_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(refresh, 200, nullptr);
    lv_screen_load(s_scr[SCR_STATUS]);
}

}  // namespace ui
