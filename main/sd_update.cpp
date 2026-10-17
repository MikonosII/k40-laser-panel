#include "sd_update.hpp"

#include <sys/stat.h>

#include <cstdio>

#include "display.hpp"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdmmc_cmd.h"
#include "settings.hpp"

static const char *TAG = "sd_update";

namespace sd_update {

static constexpr const char *MOUNT = "/sdcard";
static constexpr const char *NEW_FW = "/sdcard/firmware.bin";
static constexpr const char *DONE_FW = "/sdcard/firmware.cur";
static constexpr const char *BAD_FW = "/sdcard/firmware.bad";

static lv_obj_t *s_title, *s_bar, *s_pct;

// LVGL's task isn't running yet at boot, so drive it by hand
static void pump() {
    display::lock();
    lv_timer_handler();
    display::unlock();
}

static void show_screen() {
    display::lock();
    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F1113), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xE8E6E1), 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(scr, 20, 0);
    s_title = lv_label_create(scr);
    lv_label_set_text(s_title, "Updating firmware\nfrom SD card");
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_28, 0);
    s_bar = lv_bar_create(scr);
    lv_obj_set_size(s_bar, 380, 20);
    lv_bar_set_range(s_bar, 0, 100);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0x23272D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0xF5A524), LV_PART_INDICATOR);
    s_pct = lv_label_create(scr);
    lv_label_set_text(s_pct, "0 %  -  don't power off");
    lv_obj_set_style_text_font(s_pct, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_pct, lv_color_hex(0x9A978F), 0);
    lv_screen_load(scr);
    display::unlock();
    pump();
}

static void show_message(const char *title, const char *detail) {
    display::lock();
    lv_label_set_text(s_title, title);
    lv_label_set_text(s_pct, detail);
    display::unlock();
    pump();
}

void run() {
    esp_vfs_fat_sdmmc_mount_config_t mcfg = {};
    mcfg.format_if_mount_failed = false;  // never wipe the user's card
    mcfg.max_files = 2;
    mcfg.allocation_unit_size = 16 * 1024;

    // Pins and slot from Elecrow's P4 SD example (1-bit SDMMC, slot 0)
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = 10000;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = GPIO_NUM_43;
    slot.cmd = GPIO_NUM_44;
    slot.d0 = GPIO_NUM_39;
    slot.width = 1;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    sdmmc_card_t *card = nullptr;
    if (esp_vfs_fat_sdmmc_mount(MOUNT, &host, &slot, &mcfg, &card) != ESP_OK) {
        ESP_LOGI(TAG, "no SD card (or not FAT32) - skipping update check");
        return;
    }

    // Forgotten PIN recovery: an (empty) reset_pin.txt on the card clears the PIN
    struct stat st;
    if (stat("/sdcard/reset_pin.txt", &st) == 0) {
        settings::clear_pin();
        remove("/sdcard/reset_pin.done");
        rename("/sdcard/reset_pin.txt", "/sdcard/reset_pin.done");
    }

    if (stat(NEW_FW, &st) != 0 || st.st_size <= 0) {
        ESP_LOGI(TAG, "no firmware.bin on the card");
        esp_vfs_fat_sdcard_unmount(MOUNT, card);
        return;
    }

    ESP_LOGI(TAG, "firmware.bin found (%ld bytes) - updating", (long)st.st_size);
    show_screen();

    const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
    FILE *f = fopen(NEW_FW, "rb");
    esp_ota_handle_t ota = 0;
    bool ok = target && f && esp_ota_begin(target, st.st_size, &ota) == ESP_OK;

    static uint8_t buf[8192];
    size_t done = 0;
    int last_pct = -1;
    while (ok) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n == 0) break;
        if (esp_ota_write(ota, buf, n) != ESP_OK) {
            ok = false;
            break;
        }
        done += n;
        int pct = (int)(done * 100 / (size_t)st.st_size);
        if (pct != last_pct) {
            last_pct = pct;
            display::lock();
            lv_bar_set_value(s_bar, pct, LV_ANIM_OFF);
            lv_label_set_text_fmt(s_pct, "%d %%  -  don't power off", pct);
            display::unlock();
            pump();
        }
    }
    if (f) fclose(f);
    // esp_ota_end also verifies the image (checksum, chip type)
    if (ok) ok = esp_ota_end(ota) == ESP_OK && esp_ota_set_boot_partition(target) == ESP_OK;
    else if (ota) esp_ota_abort(ota);

    if (ok) {
        remove(DONE_FW);
        rename(NEW_FW, DONE_FW);
        esp_vfs_fat_sdcard_unmount(MOUNT, card);
        ESP_LOGI(TAG, "update installed - restarting");
        show_message("Update installed", "Restarting...");
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
    }

    // Leave the current firmware running; rename the file so it isn't retried every boot
    ESP_LOGE(TAG, "firmware.bin is not a valid image for this panel");
    remove(BAD_FW);
    rename(NEW_FW, BAD_FW);
    esp_vfs_fat_sdcard_unmount(MOUNT, card);
    show_message("Update failed", "firmware.bin isn't valid for this panel.\nRenamed to firmware.bad");
    vTaskDelay(pdMS_TO_TICKS(5000));
}

}  // namespace sd_update
