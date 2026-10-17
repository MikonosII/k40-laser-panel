# K40 laser panel

ESP-IDF (C++) firmware for the **Elecrow CrowPanel Advanced 5" ESP32-P4 (SKU DHE04005D, 800×480 IPS)**,
used in portrait as a control panel for a K40 running **grblHAL on a BTT SKR Mini E3 V3**, with
**LightBurn streaming over Wi-Fi** through the panel.

```
LightBurn ──Wi-Fi TCP:23──► panel (ESP32-P4 + C6 Wi-Fi) ──UART3 115200──► SKR Mini E3 V3 (grblHAL)
                              ▲ touch: pause / resume / stop / overrides / jog / test fire
```

## Wiring (panel ↔ SKR)

Use the panel's **UART3-IN** header (4-pin XH2.54, under the USB-C ports): **TX = GPIO27, RX = GPIO28**.

| Panel UART3-IN | SKR TFT header |
|---|---|
| TX (GPIO27) | RX — PA3 |
| RX (GPIO28) | TX — PA2 |
| GND | GND |
| 5V | **leave unconnected** (see below) |

UART3-IN's 5V pin is a power **input** to the panel, and the panel can draw 8–10 W. Don't feed it
from the SKR's 5V. Power the panel from its USB-C port with a 5V/2A+ supply, or wire a separate
5V 3A buck to that 5V pin.

Both sides are 3.3 V logic. Flash grblHAL with the **UART** build (`BTT_SKR_MINI_E3_V30`) so its
serial port is the TFT header.

## Lid and water status (optional)

The lid and water-flow switches stay in series on the laser PSU's **P–G** pins, so the PSU still
cuts the beam by itself. The SKR only listens, through two Schottky diodes (1N5819 or BAT85).
The diodes also keep the PSU's 5V out of the SKR's 3.3V inputs.

```
PSU P ──┬── lid switch ──┬── water flow switch ── PSU G (= SKR GND)
        │                │
       ─┴─ (band)       ─┴─ (band)
        ▲                ▲
   SKR PWR-DET       SKR PROBE
```

- Diode **band (cathode) toward the PSU side**; plain end to the SKR signal pin.
- SKR GND must already be tied to PSU G (it is, for the PWM wiring).

grblHAL settings:

| Setting | Change | Result |
|---|---|---|
| `$14` | add **2** (feed hold invert) | Loop open → feed hold: a running job pauses (`Pn:H`) |
| `$6` | `1` (probe invert) | Water switch open → `Pn:P` |

The Laser screen then shows **Lid** and **Water flow**, and test fire is refused while either is
open. With the water switch open, the lid state can't be told apart, so Lid shows `--`.

## grblHAL settings this firmware assumes

| Setting | Value | Why |
|---|---|---|
| `$32` | `1` | Laser mode |
| `$30` | `1000` | Max S (must match *Spindle max* in menuconfig) |
| `$31` | `0` | Min S |
| Coolant flood | air assist | Panel's Air button toggles flood (realtime `0xA0`) |
| `$14`, `$6` (optional) | see above | Lid / water status on the Laser screen |

## Build and flash

1. Install **ESP-IDF v5.5.4 or newer** (VS Code + the *ESP-IDF* extension is easiest).
2. In this folder:
   ```
   idf.py set-target esp32p4
   idf.py menuconfig        # → "K40 panel": Wi-Fi, allowed LightBurn IP, power cap, rotation
   idf.py build flash monitor
   ```
   LVGL 9, the GT911 touch driver and the ESP32-C6 Wi-Fi link (esp_hosted) download automatically.

Wi-Fi runs on the board's ESP32-C6 coprocessor. If the log shows esp_hosted can't talk to it,
update the C6 with Elecrow's *Advance_P4_On-Board_ESP32C6_Firmware* from their GitHub repo.

## LightBurn

Add the laser as a **GRBL** device using the **network (Ethernet/TCP)** connection, address = the
IP shown in the panel's header (reserve it in your router), port **23**.
Set *K40 panel → Only accept LightBurn from this IP* to your PC's address so nothing else on the
network can drive the laser. Never port-forward it.

## How control is shared

- **Pause / Resume / Stop / overrides / air** are grblHAL realtime bytes — they work instantly,
  even mid-job. LightBurn sees *Hold* and simply waits on Pause.
- **Stop** soft-resets grblHAL (laser off, motion stops). The panel then ignores LightBurn's
  leftover lines until it goes quiet; press Stop in LightBurn to clear its side.
- **Jog, Home, Set origin, test fire, pointer** send G-code lines, so they're only allowed when
  no job is running and LightBurn has nothing in flight. Replies to the panel's lines are
  swallowed so LightBurn's stream never gets an unexpected `ok`.
- **Test fire** needs *Arm* first (auto-disarms after one pulse or 60 s), refuses if the lid or
  water interlock is open, and is capped at *Panel test/pointer power cap*.
- **Pointer** auto-switches off after 30 s.
- A new alarm jumps to the Alarms screen with a plain-English explanation.

## First power-up checklist

This was compile-checked, not run on hardware. If something's off:

| Symptom | Fix |
|---|---|
| Upside-down | menuconfig → Display → *Rotate 270* |
| Colors scrambled (e.g. blue/green garbage) | menuconfig → Display → *Swap RGB565 bytes* |
| Image shifts/flickers | Lower *RGB pixel clock* (e.g. 16000000) |
| Won't boot, log mentions chip revision v3 | Turn off `ESP32P4_SELECTS_REV_LESS_V3` in sdkconfig.defaults |
| "No link to grblHAL" | Swap TX/RX wires; confirm grblHAL baud is 115200 |
| "No Wi-Fi" | Check SSID/password; update the C6 firmware (above) |

## Files

| File | What it does |
|---|---|
| `main/board.*` | Pins, I2C, STC8 backlight helper (from Elecrow's P4 code) |
| `main/display.*` | RGB panel, GT911 touch, LVGL 9 with portrait rotation |
| `main/grbl_link.*` | UART to grblHAL, status parsing, LightBurn/panel line arbitration |
| `main/net_bridge.*` | Wi-Fi station + LightBurn TCP server with IP allowlist |
| `main/ui.*` | Status, Jog, Laser and Alarms screens |
