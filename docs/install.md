# Flashing the SKR and the panel

| | First time | Updates after that |
|---|---|---|
| **SKR** (grblHAL) | microSD card. BTT boards ship with an SD-card bootloader, so no computer connection is needed | microSD card |
| **Panel** | USB-C from a computer, in the browser. Nothing to install | microSD card |

You'll need a **microSD card of 32 GB or smaller, formatted FAT32**. 4–16 GB cards are the most
reliable with BTT's bootloader. You'll also need a card reader and a **USB-C data cable** (some
cables are charge-only).

## 1. The SKR

1. **Download the file.** In this repo, open [`release/grblhal/`](../release/grblhal) for the SKR Mini
   E3 V3, or [`release/grblhal-skr3/`](../release/grblhal-skr3) for the SKR 3 (STM32H723 only). Click
   `firmware.bin`, then **Download raw file**.
2. **Copy it to the card** in the top folder, named exactly **`firmware.bin`**. Watch for browsers
   saving it as `firmware (1).bin`.
3. **Flash:** with the SKR powered off, put the card in the **SKR's** card slot, then power it on
   from the 24 V supply. Wait about 30 seconds.
4. **Check:** power off and look at the card. If the file is now called **`FIRMWARE.CUR`**, the
   flash worked. You can leave the card in the SKR or take it out.

**Still `firmware.bin`?** The bootloader didn't take it:
- Reformat the card as FAT32 (or try a smaller card), check the name again and retry.
- If it still won't take, the board's bootloader may have been erased. That happens when a board was
  flashed earlier over USB/DFU or ST-Link (some Klipper guides do this). Getting it back needs a
  computer and STM32CubeProgrammer. Open an issue and say which board you have.

This grblHAL build talks to the panel on the **TFT header**, so the SKR's USB port is only for
power. The first check happens through the panel (step 3 below).

## 2. The panel

### First time: over USB, from your browser

The SD-card updater is part of this firmware, so the panel needs one install over USB first. No
tools to install: you flash it from the browser.

1. From the [`release/`](../release) folder, open the newest version folder, then open
   **`k40-panel-full-vX.Y.Z.bin`** and click **Download raw file**.
2. In **Chrome or Edge**, open **https://espressif.github.io/esptool-js/**.
3. Plug the panel's **UART** USB-C port into the computer. Click **Connect** and pick the port.
   (Windows may need the CH340 driver first.)
4. Set **Flash Address** to `0x0`, choose the downloaded file and click **Program**.
5. When it finishes, press **RESET** on the panel.

If it won't connect, hold **BOOT**, tap **RESET**, release **BOOT**, and click Connect again.

### After that: microSD card

1. Format a microSD card as **FAT32**.
2. Copy the new firmware to the top folder of the card, named exactly **`firmware.bin`**. Use the
   `firmware.bin` from the newest `release/` folder.
3. Power the panel off, insert the card and power it on.
4. A progress screen appears. **Don't power off** until it says *Update installed* and restarts,
   about 30 seconds.

When it's done, the file is renamed `firmware.cur` so it isn't installed again. If the file isn't
valid for this panel, it's renamed `firmware.bad` and the old firmware keeps running. The update
goes into a second firmware slot and only switches over once it's verified, so a pulled card or a
bad file can't brick the panel. Settings are kept. The version is shown at the bottom of the
Setup screen.

## 3. First power-up check

With the panel wired to the SKR (see [wiring.md](wiring.md)):

1. The panel's header shows **Idle** (or **Alarm**, which is fine at this point). *No link* means a
   wiring or 5 V problem: see [troubleshooting.md](troubleshooting.md).
2. Set up Wi-Fi on the Setup screen and connect LightBurn (GRBL, network, panel IP, port 23).
3. In LightBurn's Console, run the checks in [grblhal/README.md](../grblhal/README.md#check-it):
   `$I` shows the board name, `$32=1`, and the PWM pin reads right with the Laser switch **off**.

## Building the panel firmware from source

You need Windows, macOS or Linux and a USB-C data cable. Allow about an hour the first time; most
of that is the one-time ESP-IDF install.

1. **Install the tools.** Install [VS Code](https://code.visualstudio.com/) and
   [Git](https://git-scm.com/downloads). In VS Code, install the **ESP-IDF** extension by Espressif
   Systems. Then run *ESP-IDF: Configure ESP-IDF Extension* → **Express** → ESP-IDF **v5.5.4 or
   newer 5.5.x** → **Install**.
2. **Get the code:** `git clone https://github.com/MikonosII/k40-laser-panel.git`, then
   *File → Open Folder*.
3. **Set the target:** *ESP-IDF: Set Espressif Device Target* → **esp32p4**.
4. **Settings (optional):** everything can be set later on the panel's Setup screen. To pre-fill
   it, open *ESP-IDF: SDK Configuration Editor (menuconfig)* → **K40 panel**. Your Wi-Fi password
   goes in the local `sdkconfig`, which is git-ignored.
5. **Build:** click the wrench icon. The first build downloads LVGL, the touch driver and the Wi-Fi
   coprocessor library.
6. **Flash:** plug into the **UART** USB-C port, click the plug icon to pick the port, then
   **Build, Flash and Monitor**. If it fails to connect, hold BOOT, tap RESET and release BOOT.

From an ESP-IDF terminal, the same steps are:

```
idf.py set-target esp32p4
idf.py menuconfig
idf.py -p <PORT> build flash monitor
```

On a good first boot, the log shows `GT911 at 0x5d` (touch), `UART1 TX=27 RX=28 @115200` (link to
grblHAL), `IP 192.168.x.x` (Wi-Fi) and `LightBurn server on port 23`.

### Wi-Fi coprocessor

Wi-Fi runs on the board's ESP32-C6. If the log shows that `esp_hosted` can't talk to it, update the
C6 with Elecrow's *Advance_P4_On-Board_ESP32C6_Firmware* from their
[P4 5" GitHub repo](https://github.com/Elecrow-RD/-CrowPanel-Advanced-5inch-ESP32-P4-HMI-AI-Display-800x480-IPS-Touch-Screen)
(see `example/CrowPanel_Advance_ESP32-P4 Display_Firmware_Upgrade_Guide`).
