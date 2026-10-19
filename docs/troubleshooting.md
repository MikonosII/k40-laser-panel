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
| `?` shows `Pn:Z`, or homing stops with **Alarm 8** | Nothing is wired to Z on a K40. Send `$X`, `$5=4`, `$44=3`, `$45=0`, `$46=0` (the defaults in builds after October 18, 2026) |
| Homing stops with **Alarm 8** next to a switch | Pull-off too short to clear it: `$27=3`, and slow the search with `$25=1000` |
| Motors grind or skip on fast moves, and the head ends up in the wrong place or hits the frame | Top speed too high for K40 motors: `$110=6000`, `$111=6000`, then `$H`. Raise in steps of 1000 while it stays smooth |
| Head drifts or springs back after a move; axes are easy to push by hand when idle | Motors switch off between moves: `$1=255` |
| **ALARM:1** in the middle of a move, nowhere near a switch | Noise on the end-stop wires: `$21=0` (soft limits still protect the edges after homing) |
| Frame or job runs off the bed | Home first (`$H`). In LightBurn: Origin front left, Start From **Absolute Coords**. With Current Position, the job-origin dot sets which corner sits at the head |
| Measure your own travel | `$20=0`, `G10 L2 P1 X0 Y0`, `$H`, note X; jog slowly to the far right and front and note X and Y. Travel = the difference. Set `$130`/`$131` to the travel minus about 5 mm, `$H`, then `G10 L2 P1 X-(home X) Y-$131` and `$20=1` |
| `?` shows `Pn:X` or `Pn:Y` with the switch not pressed | That end stop reads inverted. Add 1 (X) and/or 2 (Y) to `$5`, e.g. `$5=7` for both plus Z |
| Homing goes the wrong way | `$23` sets the direction: 1 = X toward 0 and Y toward max (back left). Add or remove 1 for X, 2 for Y |
| Laser doesn't fire, but the head moves | Front-panel Laser switch on? Lid and water loop closed? Diode the right way round (band toward the SKR)? |
| Laser fires weakly or the power steps are uneven | Try `$33=2000` to `5000` (PWM frequency) |
| `$I` doesn't show the SKR's board name | The SKR didn't take the grblHAL file: it must be named exactly `firmware.bin` on a FAT32 card, and gets renamed `FIRMWARE.CUR` when it works |
| Board not found when flashing the panel | Use the **UART** USB-C port and a data cable (not charge-only). Install the CH340 driver. Hold BOOT, tap RESET, release BOOT |
| Panel resets when on USB only | A computer's USB port can't supply 8–10 W. Power it from the 5 V supply too |
