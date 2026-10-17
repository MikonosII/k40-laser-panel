# grblHAL for the SKR Mini E3 V3 (K40 build)

Ready-to-flash grblHAL for the K40's BTT SKR Mini E3 V3.0, set up for this panel:

| | |
|---|---|
| Serial | TFT header, **USART2 PA2/PA3, 115200** (wired to the panel's UART3-IN) |
| Laser PWM | **NEOPIXEL / RGB connector data pin (PA8)**, 5 V buffered on the board, 1 kHz |
| Laser enable (M3/M4) | FAN1 (PC7), switched |
| Air assist (flood, M8 / panel Air button) | HE0 (PC8), switched 24 V |
| Lid → feed hold | PWR-DET (PC12) |
| Water flow → probe input | PROBE (PC14) |
| Motors | X, Y on the onboard TMC2209s (UART mode) |

Defaults on first boot: laser mode on, S 0-1000, the input polarities in
[`k40-settings.txt`](k40-settings.txt), 157.48 steps/mm, 600 mA motor current.

## Flash it

1. Copy **`release/grblhal/firmware.bin`** to a FAT32 microSD card (exactly that name).
2. Put the card in the **SKR's** slot and power the SKR on. Wait about 30 seconds.
3. The SKR's bootloader flashes it and renames the file to `FIRMWARE.CUR`. That rename is how you
   know it worked.
4. If the SKR ran grblHAL before, its old settings are kept: paste
   [`k40-settings.txt`](k40-settings.txt) in LightBurn's Console, one line at a time.

Back up first if you might put Marlin back: the stock firmware is on BTT's GitHub, so a backup
is optional.

## Check it

In LightBurn's Console (connected through the panel):

| Send | Expect |
|---|---|
| `$I` | `BTT SKR MINI E3 V3.0` in the board name |
| `$32` | `$32=1` (laser mode) |
| `?` with the lid shut and water flowing | no `H` or `P` after `Pn:` |
| With the K40's **Laser** switch **off**: `M3 S500` | PWM pin (RGB connector data) reads about 2.5 V on a meter |
| `M5` | PWM pin reads about 0 V. If it reads about 5 V instead, set `$16=4` (invert PWM) |

Then calibrate steps/mm: jog 100 mm and measure. If it moves 200 mm your motors are 1.8°: use
`$100=78.74` and `$101=78.74`.

## What's not included

- **SD card on the SKR**: this grblHAL port doesn't support it yet. The panel's own SD card
  covers that (Jobs screen, on the `feature/sd-jobs` branch).
- **USB serial**: off, so the TFT header is the only port. The upstream `BTT_SKR_MINI_E3_V30_USB`
  build has it.

## Build it yourself

```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi make git python3
./build.sh      # -> build/firmware.bin
```

`build.sh` fetches the [STM32G0xx grblHAL port](https://github.com/mtaylor819/STM32G0xx) and ST's
HAL at pinned commits, applies [`k40.patch`](k40.patch) (PWM on the NEOPIXEL pin) and builds
with the K40 defaults in the [`Makefile`](Makefile). Upstream builds with PlatformIO; this
Makefile does the same thing with only the ARM compiler.

The port itself is marked **testing stage** by its author (verified on one SKR driving a CNC
router). Moving the PWM to PA8 uses TIM1 and moves step timing to TIM2, which the driver
supports but the author's build didn't exercise. Test with the laser switch off first.
