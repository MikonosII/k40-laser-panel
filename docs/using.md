# Using the panel

## Screens

| Screen | What's on it |
|---|---|
| **Status** | Job time, speed, laser power, tube current, feed and power overrides, Pause / Resume / Stop / Air |
| **Jog** | Jog X/Y in 0.1–50 mm steps, Home, Unlock, Set origin, Go to origin, pointer (fires the tube at 1% so you can see the spot; switches off after 30 s) |
| **Laser** | Test fire (arm first; pulses of 50 ms–1 s at a set %), lid and water status, measured tube current, Air |
| **Alarms** | The active alarm in plain English, the tube-current fault with its Clear button, recent grblHAL messages |
| **Setup** | Wi-Fi, LightBurn computer, power cap, brightness, sleep, orientation, PIN, laser hours, current limits |

## LightBurn

Add the laser as a **GRBL** device with the **network (Ethernet/TCP)** connection:
- **Address:** the IP shown in the panel's header. Reserve it in your router.
- **Port:** 23.

On the panel's Setup screen, set **LightBurn computer** to your PC's IP so no other device on the
network can drive the laser. Never port-forward port 23.

## How the panel and LightBurn share control

- **Pause, Resume, Stop, overrides and Air** work instantly, even mid-job. LightBurn sees *Hold* and
  waits.
- **Stop** soft-resets grblHAL: the laser goes off and motion stops. The panel then ignores
  LightBurn's leftover lines until it goes quiet; press Stop in LightBurn to clear its side.
- **Jog, Home, Set origin, test fire and pointer** only work when no job is running.
- **Test fire** needs *Arm* first. It disarms after one pulse or 60 s, refuses to fire if the lid
  or water switch is open, and never goes above *Panel power cap*.
- A new alarm jumps straight to the Alarms screen.

## Setup screen

Settings are saved on the panel and survive firmware updates.

| Setting | Notes |
|---|---|
| Wi-Fi name / password | Type it, or tap **Scan** and pick one (2.4 GHz networks only). Blank leaves Wi-Fi off, and the other settings still save |
| LightBurn computer | Only this IP may connect. Blank allows any device |
| Panel power cap | Highest power the panel's own test fire and pointer will use |
| Brightness | Previews as you drag |
| Screen sleep | Never / 2 / 5 / 15 min. The first tap only wakes the screen. Never sleeps during a job, hold or alarm |
| Orientation | **Flipped** turns the screen 180° if it's mounted the other way up. Takes effect and saves as soon as you tap it |
| PIN lock | 4 digits, asked for when opening Setup or arming test fire. Unlocks both for 5 minutes. Blank turns it off |
| Laser on-time | Hours the laser has been on (tube wear). **Reset** needs two taps |
| Tube current warning | 5–30 mA (default 18). Needs the current sensor |
| Tube current shut-off | 6–35 mA (default 20). Above it the laser is shut off and a fault stays until you clear it on Alarms |

**Forgot the PIN?** Put an empty file named `reset_pin.txt` on the panel's SD card and restart.

## Setting the power limit on first use

Nothing in hardware limits tube current in this design, so set the limits once:

1. With the current sensor fitted, open the Laser screen and test fire at a low percentage.
2. Step up while watching *Measured*. Note the % that gives your tube's maximum current (around
   15–18 mA for most K40 tubes).
3. Set **Panel power cap** to that %, and set the same maximum in LightBurn.
4. Leave **Tube current shut-off** at 20 mA (or your tube's rating) as the backstop.

## grblHAL settings the panel expects

The grblHAL builds in this repo already use these as their defaults.

| Setting | Value | Why |
|---|---|---|
| `$32` | `1` | Laser mode |
| `$30` / `$31` | `1000` / `0` | Power scale (must match *Spindle max* in menuconfig) |
| `$33` | `1000` | PWM frequency, Hz |
| `$14` | `68` | Lid switch on feed hold, not inverted. E-stop and cycle start inverted because they're unused |
| `$6` | `0` | Water flow switch on the probe input |
