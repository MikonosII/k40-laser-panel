"""Build the board: python3 build.py place   -> unrouted board + Specctra DSN
                   python3 build.py finish  -> import routed SES, pour zones, save"""
import sys, os, re
import pcbnew
sys.path.insert(0, os.path.dirname(__file__))
from parts import *

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..'))
PCB = os.path.join(OUT, 'k40-current-sensor.kicad_pcb')
UNROUTED = os.path.join(HERE, 'unrouted.kicad_pcb')
FPLIB = '/usr/share/kicad/footprints/'
mm = pcbnew.FromMM
def P(x, y): return pcbnew.VECTOR2I(mm(ORIGIN[0] + x), mm(ORIGIN[1] + y))
F_CU, B_CU = pcbnew.F_Cu, pcbnew.B_Cu
BAR_L, BAR_R = U2_X - 2.55, U2_X + 2.55


def line(b, layer, x1, y1, x2, y2, w=0.15):
    s = pcbnew.PCB_SHAPE(b); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(P(x1, y1)); s.SetEnd(P(x2, y2)); s.SetLayer(layer); s.SetWidth(mm(w)); b.Add(s)


def text(b, s, x, y, size=1.0, layer=pcbnew.F_SilkS, bold=False, rot=0):
    t = pcbnew.PCB_TEXT(b); t.SetText(s); t.SetPosition(P(x, y)); t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size))); t.SetTextThickness(mm(size * (0.2 if bold else 0.15)))
    if rot: t.SetTextAngleDegrees(rot)
    b.Add(t)


def rect_poly(x1, y1, x2, y2):
    c = pcbnew.SHAPE_LINE_CHAIN()
    for x, y in [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]:
        c.Append(P(x, y))
    c.SetClosed(True); return c


def setup(b):
    ds = b.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetTrackWidth(mm(0.25)); nc.SetClearance(mm(0.15))
    nc.SetViaDiameter(mm(0.7)); nc.SetViaDrill(mm(0.35))
    ds.m_MinClearance = mm(0.15); ds.m_TrackMinWidth = mm(0.2)
    ds.m_ViasMinSize = mm(0.6); ds.m_MinThroughDrill = mm(0.3)
    ds.m_CopperEdgeClearance = mm(0.4); ds.m_HoleClearance = mm(0.25)


def place():
    b = pcbnew.NewBoard(PCB)
    setup(b)
    nets = {}
    for _, _, _, _, _, _, pads in PARTS:
        for n in pads.values():
            if n and n not in nets:
                ni = pcbnew.NETINFO_ITEM(b, n); b.Add(ni); nets[n] = ni
    for ref, val, (lib, name), lcsc, (x, y), rot, pads in PARTS:
        f = pcbnew.FootprintLoad(FPLIB + lib + '.pretty', name)
        f.SetReference(ref); f.SetValue(val)
        f.SetPosition(P(x, y)); f.SetOrientationDegrees(rot)
        f.Value().SetVisible(False)
        if ref == 'J3': f.Reference().SetVisible(False)
        f.Reference().SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
        f.Reference().SetTextThickness(mm(0.12))
        f.SetFPIDAsString(lib + ':' + name)
        for p in f.Pads():
            n = pads.get(p.GetNumber(), '')
            if n: p.SetNet(nets[n])
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                p.SetZoneConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        if lcsc: f.SetProperty('LCSC', lcsc)
        b.Add(f)
        if ref[0] in 'RC' and ref[1].isdigit():
            bb = f.GetBoundingBox(False, False)
            r = f.Reference(); r.SetTextAngleDegrees(0)
            r.SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8))); r.SetTextThickness(mm(0.12))
            r.SetPosition(pcbnew.VECTOR2I(bb.GetCenter().x, bb.GetTop() - mm(0.6)))
    for i, (x, y) in enumerate(HOLES):
        f = pcbnew.FootprintLoad(FPLIB + 'MountingHole.pretty', 'MountingHole_3.2mm_M3')
        f.SetReference(f'H{i+1}'); f.Reference().SetVisible(False); f.Value().SetVisible(False)
        f.SetPosition(P(x, y)); b.Add(f)
    # outline with 1 mm corner radius kept simple: straight rectangle
    for (x1, y1, x2, y2) in [(0, 0, BOARD_W, 0), (BOARD_W, 0, BOARD_W, BOARD_H),
                             (BOARD_W, BOARD_H, 0, BOARD_H), (0, BOARD_H, 0, 0)]:
        line(b, pcbnew.Edge_Cuts, x1, y1, x2, y2, 0.1)
    # isolation barrier: no copper of any kind on either layer
    z = pcbnew.ZONE(b); z.SetIsRuleArea(True)
    # Under U2 the band is inset 0.2 mm so the router can still reach U2's own pads,
    # and vias are kept 1.5 mm further out on both sides.
    def rule(x1, y1, x2, y2, tracks=True, vias=True, pour=True, name='ISOLATION'):
        z = pcbnew.ZONE(b); z.SetIsRuleArea(True)
        ls = pcbnew.LSET(); ls.AddLayer(F_CU); ls.AddLayer(B_CU); z.SetLayerSet(ls)
        z.SetDoNotAllowCopperPour(pour); z.SetDoNotAllowTracks(tracks); z.SetDoNotAllowVias(vias)
        z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
        z.Outline().AddOutline(rect_poly(x1, y1, x2, y2)); z.SetZoneName(name)
        b.Add(z)
    rule(BAR_L, 0, BAR_R, 16.5)
    rule(BAR_L, 25.5, BAR_R, BOARD_H)
    rule(BAR_L + 0.2, 16.5, BAR_R - 0.2, 25.5)
    rule(BAR_L - 1.5, 16.5, BAR_R + 1.5, 25.5, tracks=False, pour=False, name='NO_VIAS_U2')
    # moat: ground pour stays >= 1.5 mm from the EARTH copper (GDT pad 2 -> J3 pin 3)
    rule(46.5, 16.8, 65.5, 22.6, tracks=False, vias=True, pour=True, name='EARTH_MOAT')
    silk(b)
    pcbnew.SaveBoard(UNROUTED, b)
    b = pcbnew.LoadBoard(UNROUTED)
    pcbnew.ExportSpecctraDSN(b, os.path.join(HERE, 'board.dsn'))
    print('placed; DSN written')


def silk(b):
    S = pcbnew.F_SilkS
    # dashed barrier lines (gaps around U2)
    for x in (BAR_L, BAR_R):
        y = 1.0
        while y < BOARD_H - 1:
            if not (16.5 < y < 25.5):
                line(b, S, x, y, x, min(y + 1.0, BOARD_H - 1), 0.15)
            y += 2.0
    text(b, 'PANEL SIDE', 14.0, 2.2, 1.0, bold=True)
    text(b, 'TUBE SIDE - ISOLATED', 48.0, 2.2, 1.0, bold=True)
    text(b, 'K40 tube mA sensor  v1.0', 48.0, 40.5, 0.9)
    text(b, 'INA226 + ADM3260  0x40', 14.0, 40.5, 0.9)
    # terminal labels
    for i, lab in enumerate(['SCL', 'SDA', '3V3', 'GND']):
        text(b, lab, 10.6, 9.5 + 3.5 * i, 0.9)
    for i, lab in enumerate(['+5V', 'GND']):
        text(b, lab, 10.6, 27.5 + 3.5 * i, 0.9)
    for i, lab in enumerate(['IN', 'OUT', 'EARTH']):
        text(b, lab, 54.4, 30.0 - 5.08 * i, 0.8)
    text(b, '5V in REQUIRED', 9.5, 35.0, 0.8)
    text(b, 'to I2C-OUT', 10.5, 23.2, 0.8)
    text(b, 'tube cathode wire', 55.0, 35.0, 0.8)
    text(b, 'IN -> OUT', 55.0, 36.4, 0.8)


def parse_ses(path):
    s = open(path).read()
    m = re.search(r'\(resolution\s+(\w+)\s+(\d+)\)', s)
    unit, res = m.group(1), int(m.group(2))
    scale = {'um': 1e-3, 'mm': 1.0, 'mil': 0.0254, 'inch': 25.4}[unit] / res
    i = s.find('(network_out')
    body = s[i:]
    out = []
    for nm in re.finditer(r'\(net\s+("[^"]+"|\S+)(.*?)(?=\(net\s|\Z)', body, re.S):
        net = nm.group(1).strip('"')
        blk = nm.group(2)
        for w in re.finditer(r'\(path\s+(\S+)\s+([\d.]+)\s+([-\d.\s]+)\)', blk):
            layer, width = w.group(1), float(w.group(2)) * scale
            v = [float(t) * scale for t in w.group(3).split()]
            pts = list(zip(v[0::2], v[1::2]))
            out.append(('wire', net, layer, width, pts))
        for v in re.finditer(r'\(via\s+("[^"]+"|\S+)\s+([-\d.]+)\s+([-\d.]+)', blk):
            out.append(('via', net, v.group(1).strip('"'), float(v.group(2)) * scale, float(v.group(3)) * scale))
    return out


def finish():
    b = pcbnew.LoadBoard(UNROUTED)
    setup(b)
    nets = {p.GetNetname(): p.GetNet() for p in b.GetPads() if p.GetNetname()}
    lc = {p[0]: p[3] for p in PARTS}
    for fp in b.GetFootprints():
        if lc.get(fp.GetReference()): fp.SetProperty('LCSC', lc[fp.GetReference()])
    # gerbers / drill / CPL all use the board's bottom-left corner as 0,0
    b.GetDesignSettings().SetAuxOrigin(P(0, BOARD_H))
    for item in parse_ses(os.path.join(HERE, 'board.ses')):
        if item[0] == 'wire':
            _, net, layer, width, pts = item
            lay = F_CU if layer.startswith('F') else B_CU
            for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
                t = pcbnew.PCB_TRACK(b)
                # SES coordinates are in board units with y up
                t.SetStart(pcbnew.VECTOR2I(mm(x1), mm(-y1))); t.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(-y2)))
                t.SetWidth(mm(width)); t.SetLayer(lay); t.SetNet(nets[net]); b.Add(t)
        else:
            _, net, _, x, y = item
            v = pcbnew.PCB_VIA(b); v.SetPosition(pcbnew.VECTOR2I(mm(x), mm(-y)))
            v.SetWidth(mm(0.7)); v.SetDrill(mm(0.35)); v.SetNet(nets[net]); b.Add(v)
    # ground pours, both layers, one per side
    for netname, (x1, x2) in [('GND', (0, BAR_L)), ('GNDISO', (BAR_R, BOARD_W))]:
        for lay in (F_CU, B_CU):
            z = pcbnew.ZONE(b); z.SetLayer(lay); z.SetNet(nets[netname])
            z.Outline().AddOutline(rect_poly(x1, 0, x2, BOARD_H))
            z.SetLocalClearance(mm(0.3)); z.SetMinThickness(mm(0.25))
            z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
            z.SetThermalReliefGap(mm(0.3)); z.SetThermalReliefSpokeWidth(mm(0.4))
            z.SetZoneName(netname + ('_F' if lay == F_CU else '_B'))
            b.Add(z)
    # keep EARTH away from the pour: EARTH gets a rule-area moat around its copper
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    pcbnew.SaveBoard(PCB, b)
    print('finished; tracks', len(b.GetTracks()))


if __name__ == '__main__':
    {'place': place, 'finish': finish}[sys.argv[1]]()
