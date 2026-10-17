# grblHAL for the BTT SKR 3 (STM32H723, K40 build)

Same job as the [SKR Mini E3 V3 build](../README.md), for the full-size **SKR 3 / SKR 3 EZ with
the STM32H723 chip** (check the chip; H743 boards need a different file) and **TMC2209 drivers
in UART mode**.

## Wiring

| Function | SKR 3 header | Pin | Goes to |
|---|---|---|---|
| Panel link | **TFT** | PA10 (RX) | Panel UART3-IN pin 1 (TX3) |
| | | PA9 (TX) | Panel UART3-IN pin 2 (RX3) |
| | | GND | Panel UART3-IN pin 4 (GND) |
| Laser PWM | **RGB** | PE6 (5 V buffered) | Laser PSU **IN** |
| | | GND | Laser PSU **G** |
| Laser enable (M3/M4) | **FAN1** | PB6, switched | as on the Mini E3 |
| Air assist (flood) | **HE0** | PB3, switched 24 V | Air pump relay / valve |
| Lid switch | **PWRDET** | PC15 ↔ GND | 2nd lid contact, closed when shut |
| Water flow switch | **Probe** | PC13 ↔ GND | 2nd flow switch, closed while flowing |
| X / Y motors | **X / Y** driver slots | | TMC2209, UART jumper fitted under each |

Panel UART3-IN pin 3 (+5V) still comes from your 5 V supply, not the SKR's TFT +5V pin.

## Flash it

1. Copy **`release/grblhal-skr3/firmware.bin`** to a FAT32 microSD card.
2. Put it in the **SKR 3's** card slot and power on. The bootloader flashes it and renames it
   `FIRMWARE.CUR`.
3. If the board ran grblHAL before, paste [`../k40-settings.txt`](../k40-settings.txt) in
   LightBurn's Console. The settings are the same for both boards.

Check it the same way as the Mini E3 build: `$I` should report **BTT SKR-3**.

## Extras on this board

- **SD card works in grblHAL**: `$F` lists the card's files, `$F=/job.gc` runs one. The panel's
  own Jobs screen is still the easier way.
- More headroom: a 550 MHz H7 instead of the Mini's 64 MHz G0, so dense rasters stream more
  smoothly once the serial link isn't the limit.

## Build it yourself

```bash
./build.sh      # -> build/firmware.bin
```

Fetches [grblHAL's STM32H7xx driver](https://github.com/dresco/STM32H7xx) (pinned commit, which
already contains ST's HAL), applies [`k40.patch`](k40.patch) (PWM on the RGB header, lid on
PWRDET) and builds with the K40 defaults in the [`Makefile`](Makefile).

Upstream has SKR 3 H723 builds for TMC5160 drivers only; this one is the same board map with
TMC2209 and the UART on the TFT header instead of USB. Compiled, not yet run on a board: first
test with the K40's Laser switch off.
