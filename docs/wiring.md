# Wiring

![Wiring diagram](wiring.png)

The full diagram is also available as [wiring.svg](wiring.svg). Work with the laser **unplugged**, and
wait a few minutes after unplugging before reaching near the laser PSU or the tube.

## Power

| Supply | Feeds |
|---|---|
| **24 V** (e.g. Mean Well RD-125B, 24 V side) | SKR power input |
| **5 V, 2 A or more** (RD-125B, 5 V side) | Panel UART3-IN pin 3 (this powers the panel), and the tube current sensor's J2 |

Don't power the panel from the SKR's 5 V or from the laser PSU's 5 V. The panel draws 8–10 W.
All grounds (the RD-125B's COM, the SKR and the laser PSU's G) end up connected.

## Panel ↔ SKR (serial link)

Use the panel's **UART3-IN** header: J10, a 4-pin JST-XH 2.54 mm connector under the USB-C ports.
Its TX/RX lines go through a level shifter pulled up to the header's own 5 V pin, so **pin 3 must
have 5 V or the link won't work**.

| UART3-IN pin | Signal | SKR Mini E3 V3 | SKR 3 |
|---|---|---|---|
| 1 | TX3 | TFT header **RX, PA3** | TFT header **RX, PA10** |
| 2 | RX3 | TFT header **TX, PA2** | TFT header **TX, PA9** |
| 3 | +5V in | (from the 5 V supply) | (from the 5 V supply) |
| 4 | GND | TFT header GND | TFT header GND |

- The signal lines idle at 5 V. The SKR's pins are 5 V-tolerant and the pull-ups limit the current to
  under 1 mA. For extra margin, add 1 kΩ in series on each data wire.
- Before connecting the SKR, power the panel and check that pin 1 measures about 5 V to GND.

## Laser PSU

| PSU terminal | Connect to |
|---|---|
| **IN** | SKR **RGB connector data pin** (laser PWM, 5 V). Mini E3: PA8. SKR 3: PE6 |
| **G** | SKR **RGB connector GND** |
| **L** (the fire input, active low) | Front-panel **Laser switch** → **1N5819 diode** (band toward the SKR) → SKR **HB −** (bed output) |
| **P** | Unchanged: the stock lid and water switches |

- **IN** sets the power level: grblHAL's S value turns into a 0–5 V average.
- **L** is the laser enable. grblHAL switches the bed output on with M3/M4, which pulls L low through
  the diode.
- **The diode is required.** The SKR's switched outputs have an indicator LED that pulls the "−"
  terminal up toward 24 V when they're off. The diode stops that voltage from reaching L. A 1N4148 or
  1N4001 also works.
- The easiest way to fit the diode is inline: solder it into the wire near the SKR end, cover it
  with heat-shrink, and mark which way the band faces.
- With this wiring, the tube can only fire when **all four** of these are true: the Laser switch is
  on, the lid and water loop is closed, grblHAL has the laser enabled, and IN is above 0.

On the SKR 3, use the **BED −** output (PD7) in place of HB −.

## Lid and water switches for the panel (optional)

The stock lid and water switches on the PSU's **P–G** loop stay exactly as they are; they still cut
the beam by themselves. To show lid and water status on the panel, fit a **second, separate** set of
contacts and wire them to the SKR:

| Sensor | Add | SKR Mini E3 V3 | SKR 3 |
|---|---|---|---|
| Lid | A second microswitch (or a 2-circuit switch replacing the stock one) | **PWR-DET** signal ↔ GND | **PWRDET** (PC15) ↔ GND |
| Water | A second flow switch in series in the water line | **PROBE** signal ↔ GND | **Probe** (PC13) ↔ GND |

Use contacts that **close** when the lid is shut and when water is flowing. With the grblHAL build
in this repo, an open lid pauses a running job (feed hold) and the Laser screen shows *Lid: Open* or
*No flow*. The settings this relies on are `$14=68` and `$6=0`. Until the switches are fitted, use
`$14=70` and `$6=1`, or the panel will report the lid open and no water flow.

## Tube current sensor (optional)

See [tube-current-sensor.md](tube-current-sensor.md) for the two-module version, or
[the PCB](../hardware/current-sensor-pcb/README.md) for the single assembled board. It connects to the
panel's **I2C-OUT** (Grove) port and to 5 V, and goes in series with the tube's cathode (return) wire.

## Spare outputs (for later)

- **HE0**: air assist relay. The panel's **Air** buttons switch it (grblHAL "flood coolant").
- **FAN1** and **FAN2** are free.
- Never connect a switched output directly to the PSU's L. It needs the diode described above.
