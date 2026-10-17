#pragma once

namespace sd_update {

// Boot-time firmware update from the microSD card, like the SKR's:
// if /firmware.bin is on the card, flash it to the other app slot with a progress bar,
// rename it to firmware.cur and restart. Bad images are renamed firmware.bad and skipped.
// Call after display::init() and BEFORE net::init() (Wi-Fi and the card share the SD host).
void run();

}  // namespace sd_update
