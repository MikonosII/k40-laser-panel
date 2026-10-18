# Printed enclosure for the current sensor

![Enclosure](preview.png)

Two parts, no supports. Outside size is 76 × 58 × 22 mm, plus a mounting ear at each end.

| File | Print |
|---|---|
| [`base.stl`](base.stl) | Floor down |
| [`lid.stl`](lid.stl) | Already flipped: top face on the bed |

PETG or ABS, 0.2 mm layers, 3 walls, 20% infill. Avoid PLA if the box will sit near the PSU,
because it softens above about 55 °C.

## Assembly

1. Solder the screw terminals, with the wire openings facing the board ends.
2. Screw the PCB onto the four standoffs: **M3 × 6, nylon preferred**. They self-tap into the printed
   holes. Panel side (J1/J2) goes to the end marked **PANEL**, tube side (J3) to the end marked **TUBE**.
3. Wire through the end slots. Do this with the lid off: the terminal screws are on top.
4. Fit the lid with **2 × M3 × 8** screws. Each lid screw sits over one half of the board, well away
   from the isolation gap, so a metal screw never bridges the two sides.
5. Mount with M4 screws through the two end ears on an insulated surface, away from the HV lead.

Prefer heat-set inserts? Set `INSERTS = true` in [`enclosure.scad`](enclosure.scad) and export
again (OpenSCAD: **F6**, then **File → Export → STL**, with `part` set to `"base"` or `"lid"`).
If your terminals are taller than 11 mm, raise `term_h`.
