#pragma once
#include "driver/sdmmc_host.h"

namespace sd_update {

// Boot-time firmware update from the microSD card, like the SKR's:
// if /firmware.bin is on the card, flash it to the other app slot with a progress bar,
// rename it to firmware.cur and restart. Bad images are renamed firmware.bad and skipped.
// Call after display::init() and BEFORE net::init() (Wi-Fi and the card share the SD host).
void run();

// The panel's microSD slot settings (shared with jobs.cpp)
void sd_config(sdmmc_host_t *host, sdmmc_slot_config_t *slot);

}  // namespace sd_update
