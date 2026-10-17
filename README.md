# K40 laser panel

ESP-IDF (C++) firmware for the **Elecrow CrowPanel Advanced 5" ESP32-P4 (SKU DHE04005D, 800×480 IPS)**,
used in portrait as a control panel for a K40 running **grblHAL on a BTT SKR Mini E3 V3**, with
**LightBurn streaming over Wi-Fi** through the panel.

```
LightBurn ──Wi-Fi TCP:23──► panel (ESP32-P4 + C6 Wi-Fi) ──UART3 115200──► SKR Mini E3 V3 (grblHAL)
                              ▲ touch: pause / resume / stop / overrides / jog / test fire
```

## Wiring (panel ↔ SKR)

Use the panel's **UART3-IN** header (J10, 4-pin XH2.54, under the USB-C ports). From Elecrow's
schematic, its TX/RX go through a BSS138 level shifter whose header side is pulled up (4.7 kΩ)
to the header's own 5V pin, so **pin 3 must have 5V for the link to work**.

| UART3-IN pin | Signal | Connect to |
|---|---|---|
| 1 | TX3 (GPIO27 via shifter) | SKR TFT **RX — PA3** |
| 2 | RX3 (GPIO28 via shifter) | SKR TFT **TX — PA2** |
| 3 | +5V_IN | **5V from the power supply** (also powers the panel) |
| 4 | GND | Supply GND **and** SKR GND |

- Feed pin 3 from a real 5V supply (≥2 A; the panel can draw 8–10 W), e.g. the 5V side of a
  Mean Well RD-125B. Not the SKR's 5V, and not the laser PSU's 5V.
- The header's signal lines idle at **5V**. The SKR's PA2/PA3 are 5V-tolerant and the pull-ups
  limit current to <1 mA. For extra margin, add 1 kΩ in series on each data wire.
- Measure pin 1 → GND with the panel powered (≈5V) before connecting the SKR.

Flash grblHAL with the **UART** build (`BTT_SKR_MINI_E3_V30`) so its serial port is the TFT header.

## Lid and water status (optional)

The lid and water-flow switches on the laser PSU's **P–G** loop stay exactly as they are, so the
PSU still cuts the beam by itself. The SKR gets its **own, separate switch contacts**: they never
touch the PSU circuit, so no diodes or level shifting are needed.

| Sensor | Add | Wire to SKR |
|---|---|---|
| Lid | A second microswitch at the lid (or a 2-circuit switch replacing the stock one) | **PWR-DET** signal ↔ **GND** |
| Water | A second flow switch in series in the water line | **PROBE** signal ↔ **GND** |

Use normally-open contacts that **close** when the lid is shut / water is flowing.

grblHAL settings:

| Setting | Change | Result |
|---|---|---|
| `$14` | add **2** (feed hold invert) | Lid opens → feed hold: a running job pauses (`Pn:H`) |
| `$6` | `1` (probe invert) | Water stops → `Pn:P` |

The Laser screen then shows **Lid: Closed/Open** and **Water flow: Flowing/No flow**, and test
fire is refused while either is open.

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
