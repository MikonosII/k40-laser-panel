# Tube current sensor (optional)

Shows the laser tube's real current (mA) on the panel, live on the **Status** screen and
during **test fire** on the Laser screen. It warns above one limit (default 18 mA) and **shuts
the laser off** above a second, higher one (default 20 mA). Both are set on the **Setup** screen.

It's one small box, built from two breakout boards plus protection parts, with one cable to the
panel. No off-the-shelf part combines mA sensing, high-voltage isolation and I2C output, so this
is the fewest-parts way to do it.

```
 tube cathode (-) ──►[IN]┐                                     ┌─────────── panel I2C-OUT
                         │  ┌─ GDT ─► chassis earth            │  (3V3, SDA, SCL, GND)
                         ▼  │                                  │
                 ┌───────────────┐     ┌──────────────────┐    │
                 │ INA226 board  │ I2C │ ADM3260 isolator │ I2C│
                 │ (0.1 Ω shunt) │◄───►│  + isolated power│◄───┘
                 └───────────────┘     └──────────────────┘
                         │            isolated side │ panel side
 PSU return  ◄──[OUT]────┘            (2.5 kV rms barrier)
 (via analog mA meter, if kept)
```

## Bill of materials

| # | Part | Qty | Example / notes | Approx. |
|---|---|---|---|---|
| 1 | **INA226 current sensor breakout** | 1 | [Maker Portal INA226 module](https://makersportal.com/shop/ina226-voltage-and-current-module-for-arduino) or [Universal-Solder INA226](https://www.universal-solder.ca/product/ina226-current-sensor-module-with-ina226aidgsr-high-precision/); any "CJMCU-226" with a **0.1 Ω (R100)** shunt, address 0x40 | $5 |
| 2 | **I2C isolator with isolated power** | 1 | [MikroE I2C Isolator 2 Click (MIKROE-2609)](https://www.mikroe.com/i2c-isolator-2-click) — ADM3260, 2.5 kV rms, built-in isolated supply | $24 |
| 3 | Gas discharge tube (GDT), 2-electrode, **230 V** | 1 | [Bourns 2027-23-C](https://www.digikey.com/en/products/detail/bourns-inc/2027-23-C/1219557). Clamps arcs on the return wire to earth | $2 |
| 4 | Bidirectional TVS diode, **6.8 V**, through-hole | 1 | [Littelfuse P6KE6.8CA](https://www.digikey.com/en/products/detail/littelfuse-inc/P6KE6-8CA/285916). Across the shunt | $1 |
| 5 | 2-position screw terminal, 5.08 mm | 1 | [Phoenix Contact 1729128](https://www.digikey.com/en/products/detail/phoenix-contact/1729128/260615) — return wire **IN** and **OUT** | $1 |
| 6 | Grove / HY2.0 **4-pin** cable, 2.0 mm | 1 | [Seeed Grove 4-pin 20 cm (5-pack)](https://www.robotshop.com/en/grove-4-pin-buckled-20cm-cable.html) — plugs into the panel's **I2C-OUT**; cut the other end and solder to the isolator | $4 |
| 7 | Small ABS project box | 1 | [Hammond 1551KBK](https://octopart.com/part/hammond/1551KBK) (80 × 40 × 20 mm) | $4 |
| 8 | Hook-up wire, 22 AWG | – | Internal wiring | – |
| 9 | Earth lead with ring terminal | 1 | GDT to chassis earth screw | – |

**Total: about $45.** Keep the K40's analog mA meter in series too (recommended), as a
backup that needs no electronics.

## Wiring

**Sensor side (isolated, inside the box)**

| From | To |
|---|---|
| Terminal **IN** | INA226 **VIN+** |
| INA226 **VIN−** | Terminal **OUT** |
| INA226 **GND** | INA226 **VIN−** (ties the sensor to the line it measures) |
| TVS (P6KE6.8CA) | Across **VIN+** and **VIN−** |
| GDT | Terminal **IN** → chassis earth |
| INA226 **VCC, GND, SDA, SCL** | Isolator's **isolated-side** VISO, GND, SDA, SCL |

**Panel side**

| Isolator (non-isolated side) | Panel **I2C-OUT** header |
|---|---|
| 3.3V | 3V3 |
| SDA | SDA |
| SCL | SCL |
| GND | GND |

Set the isolator board's supply jumper to **3.3 V**. Check the I2C-OUT pin order against the
silkscreen next to the connector (Elecrow lists it as 3V3, SDA, SCL, GND).

**In the laser**

Cut the thin wire from the **tube's cathode (−) end** (the end away from the thick red HV lead)
to the laser PSU or the analog mA meter, and route it **IN → OUT** through the box. Direction
doesn't matter; the firmware reads either polarity.

## Safety

- **Unplug the laser and wait several minutes** before touching anything near the tube or PSU.
  Never touch the thick red HV lead or the tube's anode end.
- The return wire is normally within a few volts of ground. **If the tube arcs or the cathode
  wire comes loose, it can spike to thousands of volts.** The GDT dumps that to earth, the TVS
  protects the sensor chip, and the isolator's 2.5 kV barrier keeps it away from the panel.
- Mount the box away from the HV lead, on an insulated surface, with the earth lead short.
- The GDT only works if the chassis is actually earthed. Check the K40's mains earth.

## Firmware

Nothing to configure: at boot the panel looks for an INA226 (or INA219) at address 0x40 on its
I2C bus. The Setup screen shows *Current sensor connected* when it's found.

- **Status screen:** "Tube 12.3 mA" under laser power (red when over the limit)
- **Laser screen:** "Measured: x mA now, y mA peak" after a test fire, so you can set
  *Panel power cap* from real current instead of guessing
- **Setup → Tube current warning:** 5–30 mA (default 18). Over it for more than ~0.6 s, the
  panel wakes and shows a warning. It warns only.
- **Setup → Tube current shut-off:** 6–35 mA (default 20, must be above the warning). Over it for
  ~60 ms, the panel **soft-resets grblHAL** (laser off, motion stops, LightBurn's job is dropped)
  and **latches a fault**:
  - The header shows **FAULT** and the Alarms screen explains what tripped and at what current.
  - Test fire and the pointer are blocked, and if the laser fires over the limit again it's
    stopped again.
  - **Clear tube current fault** on the Alarms screen releases it (asks for the PIN if one is set).

The shut-off is a software trip through the panel and grblHAL, typically reacting in well under
0.2 s. It adds protection for the tube, but it doesn't replace the K40's own hardware interlocks
(lid and water on the PSU's P–G loop), which still cut the beam directly.
- A different shunt? Set *menuconfig → K40 panel → Laser → Tube current sensor shunt*.
