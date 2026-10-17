"""Write JLCPCB files: gerbers+drill zip, BOM csv, CPL csv, previews."""
import os, sys, csv, subprocess, zipfile, glob, shutil
import pcbnew
sys.path.insert(0, os.path.dirname(__file__))
from parts import PARTS, ORIGIN, BOARD_H

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..'))
PCB = os.path.join(OUT, 'k40-current-sensor.kicad_pcb')
FAB = os.path.join(OUT, 'fab')
G = os.path.join(FAB, 'gerbers')
shutil.rmtree(FAB, ignore_errors=True); os.makedirs(G)

layers = 'F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts'
subprocess.run(['kicad-cli', 'pcb', 'export', 'gerbers', '--layers', layers, '--subtract-soldermask',
                '--no-x2', '--use-drill-file-origin', '-o', G + '/', PCB], check=True, capture_output=True)
subprocess.run(['kicad-cli', 'pcb', 'export', 'drill', '--format', 'excellon', '--excellon-separate-th',
                '--drill-origin', 'plot', '--excellon-units', 'mm', '--generate-map', '--map-format', 'gerberx2', '-o', G + '/', PCB], check=True, capture_output=True)
zp = os.path.join(FAB, 'k40-current-sensor-gerbers.zip')
with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(glob.glob(G + '/*')):
        z.write(f, os.path.basename(f))

# BOM (JLC format), SMT parts only; terminals are listed separately for hand soldering
groups = {}
for ref, val, (lib, name), lcsc, *_ in PARTS:
    if not lcsc:
        continue
    groups.setdefault((val, name, lcsc), []).append(ref)
with open(os.path.join(FAB, 'k40-current-sensor-bom.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['Comment', 'Designator', 'Footprint', 'LCSC Part #'])
    for (val, name, lcsc), refs in groups.items():
        w.writerow([val, ','.join(sorted(refs, key=lambda r: (r.rstrip('0123456789'), int(''.join(c for c in r if c.isdigit()))))), name, lcsc])

b = pcbnew.LoadBoard(PCB)
with open(os.path.join(FAB, 'k40-current-sensor-cpl.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation'])
    smt = {p[0] for p in PARTS if p[3]}
    for fp in b.GetFootprints():
        r = fp.GetReference()
        if r not in smt:
            continue
        pos = fp.GetPosition()
        x = pcbnew.ToMM(pos.x) - ORIGIN[0]
        y = BOARD_H - (pcbnew.ToMM(pos.y) - ORIGIN[1])
        w.writerow([r, f'{x:.3f}mm', f'{y:.3f}mm', 'Top', f'{fp.GetOrientationDegrees() % 360:.0f}'])
print('wrote', os.listdir(FAB))
