# Lid stay

![Lid stay](preview.png)

*Top: side view with the lid closed and stopped at 95°. Bottom: the stay at 95°, and the three
printed parts.*

A printed stay that stops the K40's lid at the same angle every time it's opened. It's meant for a
lid-mounted LightBurn camera: the camera's lens calibration and alignment only stay valid if the
camera comes back to the same spot.

- **Mounts outside the case**, on the left side wall and the lid's left edge, so nothing is near
  the gantry, belts or laser head.
- **Stops just past vertical** (95° by default). The lid's own weight then holds it against the
  stop, so it settles in the same place without a latch.
- **Adjustable from 90° to 100°** with an M3 screw in the end of the strut, about 1° per mm.
- **Folds away when the lid closes.** The strut slides back along its slot and lies against the
  case side.

## Parts

| File | Print |
|---|---|
| [`strut.stl`](strut.stl) | Flat, 185 mm long |
| [`lid_bracket.stl`](lid_bracket.stl) | As modelled: tab flat on the bed, leg standing up |
| [`case_bracket.stl`](case_bracket.stl) | Plate down |

PETG, 0.2 mm layers, 4 walls, 30% infill, no supports. On the lid bracket, the bottom of the
sideways spigot may sag slightly; clean it up with a file or a knife so the strut turns freely.

**Hardware:**
- 2 × M5 × 20 bolts, 2 × M5 nylock nuts, 2 × M5 fender washers (15–20 mm OD)
- 1 × M3 × 25 screw and 2 × M3 nuts
- To mount the brackets: 4 × 4 mm (5/32") pop rivets, or M4 bolts with nuts, or #8 sheet-metal
  screws

## Where the brackets go

All measurements are from the centre of the **left hinge's pin**, with the lid closed.

| | Pivot centre | Notes |
|---|---|---|
| **Lid bracket** | **80 mm** forward of the hinge pin | The tab sits on the lid's top surface at its left edge, and the leg hangs over the edge. That sets the height automatically (8 mm below the lid top) |
| **Case bracket** | **110 mm** forward, **40 mm** below the hinge pin | Plate flat on the case's left side wall. Both screws go below the pivot |

Being a few millimetres off just moves the stop angle by a few degrees, and the adjusting screw
takes that out.

**Does your lid stick out past the case side, or sit inside it?** Measure how far the lid's side
face is outside the case's side face, set `lid_offset` in [`lid-stay.scad`](lid-stay.scad) to that
(negative if it's inside), and export the brackets again. That keeps the strut level between the
two brackets. Flush is 0, the default.

## Assembly

1. **Brackets:** drop an M5 nylock into each bracket's nut slot (the case bracket's slot is on the
   side of the boss, the lid bracket's is on top).
2. **Strut:** drop an M3 nut into the slot at the strut's end, then thread in the M3 × 25 screw
   from the end with the second nut on it as a lock nut.
3. **Test fit first:** tape the brackets in place, fit the strut over both spigots, and bolt each
   one with an M5 × 20 and a fender washer. Snug the bolts down onto the spigot tops; the strut
   should still turn and slide freely. Open and close the lid a few times.
4. **Look inside before drilling.** The left Y rail and its belt run along the inside of that wall,
   so use rivets or short sheet-metal screws that stay clear of them. Then mark the holes through
   the brackets and drill 4.5 mm (5/32" for rivets).
5. **Fix the brackets** with rivets or screws, and refit the strut.

## Setting the angle

Open the lid fully so it rests on the stay. Turn the M3 screw in to stop the lid sooner, or out to
let it open further:

| Screw tip into the slot | Lid angle |
|---|---|
| 0 mm (backed out) | 100° |
| 2 mm | 98° |
| 4 mm | 96° |
| 6 mm | 94° |
| 8 mm | 92° |

The marks on the strut's top face are 2 mm apart, measured from the slot's end. Keep it above 90°,
or the lid won't lean back onto the stop. Lock it with the second nut, then calibrate the camera.
If you change the angle later, LightBurn's camera alignment needs to be done again.

## Changing the design

[`lid-stay.scad`](lid-stay.scad) is parametric: the pivot positions (`a`, `b`, `c`, `e`), the stop
range and `lid_offset` are at the top. The strut length and slot are worked out from those, and
OpenSCAD's console prints the result. `part = "assembly"` with `open_angle` shows the side view at
any angle.

The stay mirrors for the right side: the parts are symmetric, so the same prints fit.

License: CERN-OHL-P-2.0, like the rest of the hardware in this repo.
