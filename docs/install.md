# Installing and updating the panel firmware

## First install (one time, over USB)

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

## Updating from a microSD card

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

## Building from source

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

## The SKR (grblHAL)

See [grblhal/](../grblhal/README.md) for the SKR Mini E3 V3 and [grblhal/skr3/](../grblhal/skr3/README.md)
for the SKR 3. Both are ready-made `firmware.bin` files that you copy to the SKR's own SD card.
