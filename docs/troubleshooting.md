# Troubleshooting

| Symptom | Try |
|---|---|
| Screen upside down | Setup → Orientation → **Flipped** |
| Colors scrambled | menuconfig → K40 panel → Display → *Swap RGB565 bytes*, then rebuild |
| Image shifts or flickers | menuconfig → Display → lower *RGB pixel clock* (e.g. 16000000) |
| Won't boot; log mentions chip revision v3 | Remove `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` from `sdkconfig.defaults` and rebuild |
| **No link to grblHAL** | Check that UART3-IN pin 3 has 5 V. Swap the TX/RX wires. Both ends must use 115200 baud |
| **No Wi-Fi** | 2.4 GHz networks only. Recheck the name and password on Setup. Update the C6 firmware ([install.md](install.md#wi-fi-coprocessor)) |
| LightBurn won't connect | Is the IP in the panel header the one LightBurn uses? Is *LightBurn computer* on Setup blank or set to this PC's IP? |
| Laser screen says *Lid: Open* / *No flow* with no switches fitted | Send `$14=70` and `$6=1`, or fit the switches ([wiring.md](wiring.md#lid-and-water-switches-for-the-panel-optional)) |
| Laser doesn't fire, but the head moves | Front-panel Laser switch on? Lid and water loop closed? Diode the right way round (band toward the SKR)? |
| Laser fires weakly or the power steps are uneven | Try `$33=2000` to `5000` (PWM frequency) |
| `$I` doesn't show the SKR's board name | The SKR didn't take the grblHAL file: it must be named exactly `firmware.bin` on a FAT32 card, and gets renamed `FIRMWARE.CUR` when it works |
| Board not found when flashing the panel | Use the **UART** USB-C port and a data cable (not charge-only). Install the CH340 driver. Hold BOOT, tap RESET, release BOOT |
| Panel resets when on USB only | A computer's USB port can't supply 8–10 W. Power it from the 5 V supply too |
