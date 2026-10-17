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

Flash the SKR with **[grblhal/](grblhal/README.md)**: a ready-made grblHAL `firmware.bin` (UART on the TFT
header, laser PWM on the RGB connector, K40 defaults). Copy it to the SKR's SD card and power on.

## Lid and water status (optional)

The lid and water-flow switches on the laser PSU's **P–G** loop stay exactly as they are, so the
PSU still cuts the beam by itself. The SKR gets its **own, separate switch contacts**: they never
touch the PSU circuit, so no diodes or level shifting are needed.

| Sensor | Add | Wire to SKR |
|---|---|---|
| Lid | A second microswitch at the lid (or a 2-circuit switch replacing the stock one) | **PWR-DET** signal ↔ **GND** |
| Water | A second flow switch in series in the water line | **PROBE** signal ↔ **GND** |

Use normally-open contacts that **close** when the lid is shut / water is flowing.

grblHAL settings (already the defaults in this repo's grblHAL build, see [grblhal/](grblhal/README.md)):

| Setting | Value | Result |
|---|---|---|
| `$14` | `68` (feed hold **not** inverted) | Lid opens → feed hold: a running job pauses (`Pn:H`) |
| `$6` | `0` | Water stops → `Pn:P` |

grblHAL treats an input that's pulled high (switch open) as active, so a switch that closes to
GND when everything is OK needs no inverting. Until the switches are fitted, use `$14=70` and
`$6=1`, or the panel shows *Lid: Open* and *No flow*.

The Laser screen then shows **Lid: Closed/Open** and **Water flow: Flowing/No flow**, and test
fire is refused while either is open.

## Tube current sensor (optional)

An INA226 current sensor in the tube's return wire, behind an I2C isolator, lets the panel show
real tube current (mA), warn above one limit and shut the laser off above another. Parts list, wiring and safety notes:
**[docs/tube-current-sensor.md](docs/tube-current-sensor.md)**. A single assembled PCB version,
ready to order from JLCPCB, is in **[hardware/current-sensor-pcb](hardware/current-sensor-pcb/README.md)**.

## grblHAL settings this firmware assumes

| Setting | Value | Why |
|---|---|---|
| `$32` | `1` | Laser mode |
| `$30` | `1000` | Max S (must match *Spindle max* in menuconfig) |
| `$31` | `0` | Min S |
| Coolant flood | air assist | Panel's Air button toggles flood (realtime `0xA0`) |
| `$14`, `$6` | `68`, `0` (see above) | Lid / water status on the Laser screen |
| `$33` | `1000` | Laser PWM frequency (Hz) |

## Updating from a microSD card

Like the SKR, the panel can update itself from a memory card:

1. Format a microSD card as **FAT32**.
2. Copy the new firmware onto it, named exactly **`firmware.bin`** (in the top folder).
3. Power the panel off, insert the card, power it on.
4. A progress screen appears ("Updating firmware from SD card"); **don't power off** until it
   says *Update installed* and restarts (about 30 seconds).
5. The file is renamed **`firmware.cur`** so it isn't installed again. If the file wasn't valid
   for this panel, it's renamed **`firmware.bad`** and the old firmware keeps running.

The current version is shown at the bottom of the **Setup** screen. Your saved settings
(Wi-Fi, LightBurn IP, power cap) are kept across updates.

Where to get `firmware.bin`:
- **Ready-made:** download it from this repo's **`release/`** folder (newest version folder).
- **Build it yourself:** after building (below), use `build/k40_panel.bin`, renamed to `firmware.bin`.

The update installs into the panel's second firmware slot and only switches over once the new
copy is fully written and verified, so pulling the card or a bad file can't brick it.

## First install (one time, over USB)

The SD updater is part of this firmware, so it has to get onto the panel once over USB. You
don't need to install any tools for this: use the ready-made full image from the **`release/`** folder.

1. Download **`k40-panel-full-vX.Y.Z.bin`** from the `release/` folder (on GitHub: open the file, then **Download raw file**).
2. In **Chrome or Edge**, open **https://espressif.github.io/esptool-js/**.
3. Plug the panel's **UART** USB-C port into the computer, click **Connect**, pick the port.
   (Windows may need the CH340 driver first.)
4. Set **Flash Address `0x0`**, choose the downloaded file, click **Program**.
5. When it finishes, press **RESET** on the panel. From now on, update with the SD card.

If it won't connect, hold **BOOT**, tap **RESET**, release **BOOT**, and click Connect again.

## Build and flash the display (from source)

You need a computer (Windows, macOS or Linux) and a USB-C data cable. Budget about an hour the
first time; most of it is the one-time ESP-IDF install.

### 1. Install the tools (one time)

1. Install **[VS Code](https://code.visualstudio.com/)** and **[Git](https://git-scm.com/downloads)**.
2. In VS Code, open Extensions (Ctrl/Cmd+Shift+X), search **"ESP-IDF"** and install the one
   published by **Espressif Systems**.
3. Open the Command Palette (Ctrl/Cmd+Shift+P) → **"ESP-IDF: Configure ESP-IDF Extension"** →
   **Express** → pick ESP-IDF **v5.5.4 or newer** (5.5.x) → **Install**. This downloads the compiler
   and Python tools (several GB, 10–30 min).
4. **Windows only:** install the **CH340 USB-serial driver** if the board doesn't show up as a
   COM port later (search "CH340 driver"). macOS and Linux have it built in.

### 2. Get the code

```
git clone https://github.com/MikonosII/k40-laser-panel.git
```

(It's a private repo, so Git will ask you to sign in to GitHub.) Then in VS Code:
**File → Open Folder → `k40-laser-panel`**.

### 3. Set the chip type

Command Palette → **"ESP-IDF: Set Espressif Device Target"** → **esp32p4** →
**"ESP32-P4 chip (via builtin USB-JTAG)"** (any option is fine; you'll flash over serial).

### 4. Enter your settings (optional)

Everything in this table can also be set later on the panel's **Setup** screen (see
*On-panel Setup screen* below), so you can skip this step and flash with blank Wi-Fi.
To pre-fill them instead: Command Palette → **"ESP-IDF: SDK Configuration Editor (menuconfig)"**,
open the **K40 panel** section, fill in the settings below and click **Save**:

| Setting | What to enter |
|---|---|
| Wi-Fi SSID / password | Your **2.4 GHz** network |
| Only accept LightBurn from this IP | Your LightBurn PC's IP (reserve it in your router) |
| Spindle max ($30) | `1000` unless you changed grblHAL's `$30` |
| Panel test/pointer power cap (%) | The duty that gives your tube's safe current (measure with an mA meter) |
| Portrait orientation | Leave at 90°; switch to 270° if the screen comes up upside down |

Your Wi-Fi password is stored in the local `sdkconfig` file, which is git-ignored, so it never
goes to GitHub.

### 5. Build

Click the **Build** (wrench) icon in the bottom status bar, or Command Palette →
**"ESP-IDF: Build your Project"**. The first build downloads LVGL, the touch driver and the
Wi-Fi coprocessor library, and takes a few minutes; later builds are fast.

### 6. Connect and flash

1. Plug the USB-C cable into the port labelled **UART** (the one wired to the CH340 chip).
   The **USB2.0** port is not used for flashing.
2. If the panel is already wired into the laser, you can leave its 5V supply on: the board's
   power path keeps USB and the 5V header from fighting. On USB power alone, the screen may
   flicker or reset at full brightness because a computer port can't supply 8–10 W.
3. Click the **plug** icon in the status bar and pick the new port:
   `COMx` on Windows, `/dev/cu.wchusbserial…` or `/dev/cu.usbserial…` on macOS,
   `/dev/ttyUSB0` or `/dev/ttyACM0` on Linux.
4. Click the **star/flame "Build, Flash and Monitor"** icon (or **Flash**, then **Monitor**).
   Choose **UART** if it asks for a flash method.
5. The board enters download mode automatically. If flashing fails to connect: hold **BOOT**,
   tap **RESET**, release **BOOT**, then flash again.

To leave the serial monitor, press **Ctrl+]**.

### 7. What a good first boot looks like

In the monitor you should see, roughly in order:
- `GT911 at 0x5d` (or `0x14`): touch found
- `UART1 TX=27 RX=28 @115200`: grblHAL link started
- `IP 192.168.x.x`: Wi-Fi connected (also shown in the panel's header)
- `LightBurn server on port 23`

If something's missing, see the checklist below.

### Command-line alternative

From an ESP-IDF terminal (Command Palette → **"ESP-IDF: Open ESP-IDF Terminal"**):

```
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p <PORT> build flash monitor
```

### Wi-Fi coprocessor

Wi-Fi runs on the board's ESP32-C6. If the log shows esp_hosted can't talk to it, update the C6
with Elecrow's *Advance_P4_On-Board_ESP32C6_Firmware* (in their
[P4 5" GitHub repo](https://github.com/Elecrow-RD/-CrowPanel-Advanced-5inch-ESP32-P4-HMI-AI-Display-800x480-IPS-Touch-Screen),
under `example/CrowPanel_Advance_ESP32-P4 Display_Firmware_Upgrade_Guide`).

## On-panel Setup screen

The fifth tab, **Setup**, lets you change settings on the panel itself. They're saved in the
panel's flash and survive reflashing the firmware; the menuconfig values are only used until
the first Save.

| Setting | Notes |
|---|---|
| Wi-Fi name | Type it, or tap **Scan** and pick from the list (2.4 GHz networks only) |
| Wi-Fi password | Hidden by default; **Show** reveals it |
| LightBurn computer IP | Only this PC may connect; leave blank to allow any device |
| Panel power cap | Max power for the panel's own test fire / pointer |
| Brightness | Previews live as you drag |
| Screen sleep | Never / 2 / 5 / 15 min. The screen goes dark when idle; the first tap only wakes it. Never sleeps during a job, hold or alarm |
| Screen orientation | **Flipped** turns the UI 180° if the panel is mounted the other way up |
| PIN lock | 4 digits. Asked for when opening Setup or arming test fire; unlocks both for 5 min. Blank = off |
| Laser on-time | Hours the laser output has been on (tube wear). **Reset** needs two taps |
| Tube current warning | 5–30 mA; warns when the optional current sensor reads above it |
| Tube current shut-off | 6–35 mA; above it the laser is shut off and a fault latches until cleared on Alarms |

**Forgot the PIN?** Put an empty file named `reset_pin.txt` on the SD card and restart; the
PIN is cleared and the file is renamed `reset_pin.done`.

Tapping a field pops up a small on-screen keyboard (a number pad for the IP). Tap **✓** to
finish or **✕** to close it. **Save** checks the entries, stores them, and reconnects Wi-Fi if
the name or password changed. Saving is blocked while a job is running.

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

## Running jobs from the panel (no PC needed once uploaded)

LightBurn can't send a file to a GRBL controller, so this uses LightBurn's **Save GCode**:

1. In LightBurn, click **Save GCode** (turn off Beginner Mode in *Edit → Settings* if you don't
   see it). Set *Start From* to **Absolute Coords** so the file doesn't depend on where the head is.
2. In a browser on the same network, open **http://&lt;panel IP&gt;/** (the address is on the Jobs
   screen) and upload the file.
3. On the panel's **Jobs** screen, pick the file and press **Start** (asks for the PIN if one is set).

**Where files go:** onto the panel's microSD card, in a `jobs` folder. With no card in the slot,
they're held in memory (up to 20 MB) and lost at restart. The panel checks for a card every few
seconds while the Jobs screen is open, so you can insert one at any time.

**While a panel job runs:**
- The panel feeds grblHAL itself, so a Wi-Fi drop or a sleeping PC doesn't affect the job.
- **Pause / Resume / Stop** work from the Jobs or Status screen. Stop soft-resets grblHAL (laser
  off). A tube over-current trip, an alarm or a reset also ends the job.
- LightBurn can stay connected and watch the position. Its own G-code lines get `error:9` and its
  reset is ignored, so stop panel jobs from the panel.
- Start is refused if the lid is open, water isn't flowing, grblHAL isn't Idle, a LightBurn job is
  running, or there's a tube current fault.
- The upload page uses the same *LightBurn computer* IP allowlist as port 23. Jobs can't be
  started from the browser, only from the panel.

Streaming speed is the same as from LightBurn (the 115200-baud link to the SKR is the limit).

## First power-up checklist

This was compile-checked, not run on hardware. If something's off:

| Symptom | Fix |
|---|---|
| Upside-down | menuconfig → Display → *Rotate 270* |
| Colors scrambled (e.g. blue/green garbage) | menuconfig → Display → *Swap RGB565 bytes* |
| Image shifts/flickers | Lower *RGB pixel clock* (e.g. 16000000) |
| Won't boot, log mentions chip revision v3 | Turn off `ESP32P4_SELECTS_REV_LESS_V3` in sdkconfig.defaults |
| "No link to grblHAL" | Swap TX/RX wires; confirm grblHAL baud is 115200 |
| "No Wi-Fi" | Check SSID/password and that it's 2.4 GHz; update the C6 firmware (above) |
| Board not found when flashing | Use the **UART** USB-C port and a data (not charge-only) cable; install the CH340 driver; try BOOT + RESET |
| Jobs screen says *No SD card* with a card in | Card must be FAT32 (exFAT isn't supported); try another card |
| Wi-Fi drops while uploading or running from SD | Report it: the card and the Wi-Fi chip share one SD controller. Use memory (no card) as a workaround |
| Screen resets during flashing/USB-only | Normal on weak USB power; power it from the 5V supply too |

## Files

| File | What it does |
|---|---|
| `main/board.*` | Pins, I2C, STC8 backlight helper (from Elecrow's P4 code) |
| `main/display.*` | RGB panel, GT911 touch, LVGL 9 with portrait rotation |
| `main/grbl_link.*` | UART to grblHAL, status parsing, LightBurn/panel line arbitration |
| `main/net_bridge.*` | Wi-Fi station + LightBurn TCP server with IP allowlist |
| `main/ui.*` | Status, Jog, Laser, Alarms and Setup screens |
| `main/settings.*` | Settings saved in flash (NVS), with menuconfig values as defaults |
