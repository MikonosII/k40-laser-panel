#pragma once

namespace display {

// Bring up the RGB panel, GT911 touch and LVGL (portrait, 480x800 logical).
void init();

// LVGL is not thread-safe: wrap every lv_* call made outside the LVGL task.
bool lock(int timeout_ms = -1);
void unlock();

// Start the LVGL handler task (call after building the UI).
void start();

}  // namespace display
