# Running jobs from the panel (beta)

On the `feature/sd-jobs` branch, firmware v0.6.0-beta3 in `release/v0.6.0-beta3/`. Once a file is uploaded, no PC is needed.

LightBurn can't send a file to a GRBL controller, so this uses LightBurn's **Save GCode**:

1. In LightBurn, click **Save GCode** (turn off Beginner Mode in *Edit → Settings* if you don't
   see it). Set *Start From* to **Absolute Coords** so the file doesn't depend on where the head is.
2. In a browser on the same network, open **http://&lt;panel IP&gt;/** (the address is on the Jobs
   screen) and upload the file.
3. On the panel's **Jobs** screen, pick the file and press **Start** (asks for the PIN if one is set).

**Where files go:** onto the panel's microSD card, in a `jobs` folder. With no card in the slot,
they're held in memory (up to 20 MB) and lost at restart. The panel checks for a card every few
seconds while the Jobs screen is open, so you can insert one at any time.

**Preview:** picking a job draws its laser path (brighter = more power) as a thumbnail on the
Jobs screen and full-size on the Status screen, with a red dot showing where the head is right
now. Travel moves aren't drawn. Big photo engravings take a few seconds to draw. With no job
picked, the Status screen shows the 300 x 200 mm bed instead. The picture uses Y up, like
LightBurn's default view.

**History:** the Jobs screen's **History** button lists the last 20 jobs, panel or LightBurn,
with date and time, run time, peak tube current (with the sensor fitted) and how each ended. It
survives restarts. The clock comes from the internet once Wi-Fi is up; the time zone is
*menuconfig → K40 panel → Time zone* (US Central by default).

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

## If something's off

| Symptom | Try |
|---|---|
| Jobs screen says *No SD card* with a card in | The card must be FAT32 (exFAT isn't supported). Try another card |
| Wi-Fi drops while uploading or running from SD | The card and the Wi-Fi chip share one SD controller. Please report it. As a workaround, remove the card and jobs are kept in memory instead |
| Preview looks mirrored top to bottom | Your machine's Y runs the other way from LightBurn's default view. Please report it |
