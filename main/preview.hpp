// Picture of a job's laser path, drawn once in the background into a small RGB565 image.
// The UI shows it on the Status screen (with the live head position on top) and as a
// thumbnail on the Jobs screen.
#pragma once
#include <cstdint>
#include <string>

#include "lvgl.h"

namespace preview {

constexpr int W = 416, H = 220;

void init();

// Draw this job (async). An empty name clears the picture back to the empty bed.
// force = redraw even if it's already showing (e.g. the file was re-uploaded).
void request(const std::string &job_name, bool force = false);

struct View {
    std::string name;     // job shown, empty for the bed
    bool busy = false;    // still drawing
    std::string msg;      // e.g. "No laser moves found"
    float min_x = 0, min_y = 0, scale = 1;  // mm -> px: px = off + (mm - min) * scale (y flipped)
    float off_x = 0, off_y = 0;
    float size_x = 0, size_y = 0;  // job extents, mm
};
View view();

// Map a work-coordinate position (mm) to image pixels; false if it's outside the picture
bool to_px(const View &v, float x_mm, float y_mm, int *px, int *py);

// Shared image (both screens point at it). Call under the display lock.
const lv_image_dsc_t *image();
uint32_t revision();  // bumps when the picture changes

}  // namespace preview
